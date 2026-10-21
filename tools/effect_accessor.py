"""Strict integer interpreter used by the animation metadata reader."""


import argparse


import collections


import hashlib


import json


import os


from pathlib import Path


import struct


import sys


import tempfile


import time


ROOT = Path(__file__).resolve().parents[1]


import pefile


from capstone import Cs, CS_ARCH_X86, CS_MODE_64


from capstone.x86 import X86_OP_REG, X86_OP_IMM, X86_OP_MEM


from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC


from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX,
    UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_R10, UC_X86_REG_R11,
    UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_EFLAGS)


TARGET_SHA256 = "1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"


BASE = 0x140000000


EFFECT_COUNT, FRAME_COUNT = 8372, 28141


INDEX_RVA, FRAMES_RVA = 0xB8DC8, 0x3C68A0


LOOP_RVA, REMAP_RVA = 0xA44D0, 0xA43F0


CODE_BOUNDS = {LOOP_RVA: (0xA44D0, 0xA4C58), REMAP_RVA: (0xA43F0, 0xA44CB)}


CAP = 10000


MASK64 = (1 << 64) - 1


def signed(value, bits):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


class Rejected(ValueError):
    pass


class IntegerMachine:
    """Small strict x64 integer interpreter; no Unicorn state or branch results.

    Only instructions reached by these two bounded functions are implemented.
    Registers begin undefined except input CX. Reading other initial register
    bits is rejected, so a reproducible poison seed cannot hide a dependency.
    """
    ALIASES = {"rax": ("a", 64), "eax": ("a", 32), "ax": ("a", 16), "al": ("a", 8),
               "rcx": ("c", 64), "ecx": ("c", 32), "cx": ("c", 16),
               "rdx": ("d", 64), "edx": ("d", 32), "dx": ("d", 16),
               "r8": ("8", 64), "r8d": ("8", 32)}

    def __init__(self, image, base=BASE, bounds=None):
        self.image, self.base = image, base
        self.bounds = bounds or CODE_BOUNDS
        self.cs = Cs(CS_ARCH_X86, CS_MODE_64)
        self.cs.detail = True
        self.instructions = {}
        self.data_reads = set()

    def decode(self, address):
        if address not in self.instructions:
            offset = address - self.base
            if not 0 <= offset < len(self.image):
                raise Rejected("Instruction fetch outside original image")
            ins = next(self.cs.disasm(self.image[offset:offset + 15], address, count=1), None)
            if ins is None:
                raise Rejected("Undecodable native instruction")
            self.instructions[address] = ins
        return self.instructions[address]

    def read_reg(self, name):
        if name not in self.ALIASES:
            raise Rejected(f"Unreviewed register {name}")
        key, bits = self.ALIASES[name]
        mask = (1 << bits) - 1
        if self.defined.get(key, 0) & mask != mask:
            raise Rejected(f"Context-dependent initial register {name}")
        return self.regs.get(key, 0) & mask

    def write_reg(self, name, value):
        if name not in self.ALIASES:
            raise Rejected(f"Unreviewed destination register {name}")
        key, bits = self.ALIASES[name]
        mask = (1 << bits) - 1
        if bits in (32, 64):
            self.regs[key] = value & mask
            self.defined[key] = MASK64
        else:
            self.regs[key] = (self.regs.get(key, 0) & ~mask) | (value & mask)
            self.defined[key] = self.defined.get(key, 0) | mask

    def address(self, ins, operand):
        m = operand.mem
        if m.segment:
            raise Rejected("Unreviewed segmented memory reference")
        base = ins.address + ins.size if ins.reg_name(m.base) == "rip" else \
            self.read_reg(ins.reg_name(m.base)) if m.base else 0
        index = self.read_reg(ins.reg_name(m.index)) if m.index else 0
        return (base + index * m.scale + m.disp) & MASK64

    def read(self, ins, operand):
        if operand.type == X86_OP_IMM:
            return operand.imm
        if operand.type == X86_OP_REG:
            return self.read_reg(ins.reg_name(operand.reg))
        if operand.type == X86_OP_MEM:
            address = self.address(ins, operand)
            offset, size = address - self.base, operand.size
            if not 0 <= offset <= len(self.image) - size:
                raise Rejected("Context-dependent or out-of-image memory read")
            raw = self.image[offset:offset + size]
            self.data_reads.add((offset, size, raw.hex()))
            return int.from_bytes(raw, "little")
        raise Rejected("Unreviewed operand")

    def put(self, ins, operand, value):
        if operand.type != X86_OP_REG:
            raise Rejected("Native accessor writes memory")
        self.write_reg(ins.reg_name(operand.reg), value)

    def arithmetic_flags(self, a, b, result, bits, subtract):
        mask, sign = (1 << bits) - 1, 1 << (bits - 1)
        a, b, result = a & mask, b & mask, result & mask
        self.flags = {"z": result == 0, "s": bool(result & sign),
                      "c": a < b if subtract else a + b > mask,
                      "o": bool(((a ^ b) & (a ^ result) if subtract else
                                  ~(a ^ b) & (a ^ result)) & sign)}

    def branch(self, mnemonic):
        f = self.flags
        required = {"je": ("z",), "jne": ("z",), "ja": ("c", "z"),
                    "jb": ("c",), "jbe": ("c", "z"), "jg": ("z", "s", "o"),
                    "jge": ("s", "o")}
        if mnemonic not in required or any(k not in f for k in required[mnemonic]):
            raise Rejected(f"Undefined or unreviewed condition {mnemonic}")
        return {"je": lambda: f["z"], "jne": lambda: not f["z"],
                "ja": lambda: not f["c"] and not f["z"], "jb": lambda: f["c"],
                "jbe": lambda: f["c"] or f["z"],
                "jg": lambda: not f["z"] and f["s"] == f["o"],
                "jge": lambda: f["s"] == f["o"]}[mnemonic]()

    def call(self, entry, argument):
        lo, hi = self.bounds[entry]
        self.regs, self.defined, self.flags = {"c": argument & 0xFFFF}, {"c": 0xFFFF}, {}
        path, address = [], self.base + entry
        for _ in range(CAP):
            if not self.base + lo <= address < self.base + hi:
                raise Rejected("Control flow left the bounded original accessor")
            ins = self.decode(address)
            if address + ins.size > self.base + hi:
                raise Rejected("Instruction crosses accessor boundary")
            path.append(address - self.base)
            address += ins.size
            op, args = ins.mnemonic, ins.operands
            if op == "ret":
                return self.read_reg("eax" if entry == LOOP_RVA else "al"), tuple(path)
            if op in ("mov", "movzx", "movsx", "movsxd"):
                value = self.read(ins, args[1])
                if op in ("movsx", "movsxd"):
                    value = signed(value, args[1].size * 8)
                self.put(ins, args[0], value)
            elif op == "lea":
                self.put(ins, args[0], self.address(ins, args[1]))
            elif op == "xor" and args[0].type == args[1].type == X86_OP_REG \
                    and args[0].reg == args[1].reg:
                self.put(ins, args[0], 0)
                self.flags = {"z": True, "s": False, "o": False, "c": False}
            elif op in ("cmp", "sub", "add", "dec"):
                a = self.read(ins, args[0])
                b = 1 if op == "dec" else self.read(ins, args[1])
                subtract = op != "add"
                result = a - b if subtract else a + b
                old_cf = self.flags.get("c")
                self.arithmetic_flags(a, b, result, args[0].size * 8, subtract)
                if op == "dec":
                    if old_cf is None:
                        self.flags.pop("c")
                    else:
                        self.flags["c"] = old_cf
                if op != "cmp":
                    self.put(ins, args[0], result)
            elif op == "bt":
                self.flags["c"] = bool(self.read(ins, args[0]) &
                    (1 << (self.read(ins, args[1]) % (args[0].size * 8))))
                self.flags.pop("o", None)
                self.flags.pop("s", None)
            elif op == "jmp":
                address = self.read(ins, args[0])
            elif op.startswith("j"):
                if self.branch(op):
                    address = self.read(ins, args[0])
            else:
                raise Rejected(f"Unreviewed native instruction {op}")
        raise Rejected("Static accessor instruction cap exceeded")


