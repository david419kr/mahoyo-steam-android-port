"""Bounded CPU reader for metadata initializers; no game process is started."""
import hashlib, math, struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RAX, UC_X86_REG_XMM0, UC_X86_REG_MXCSR)
from effect_accessor import TARGET_SHA256

class Native:
    def __init__(self, raw):
        if hashlib.sha256(raw).hexdigest() != TARGET_SHA256:
            raise ValueError("Unpinned native image")
        self.pe = pefile.PE(data=raw)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        image = self.pe.get_memory_mapped_image()
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(self.base, (len(image) + 4095) & ~4095)
        self.uc.mem_write(self.base, image)
        self.uc.mem_map(0x10000000, 65536)
        self.uc.mem_map(0x30000000, 4096)
        self.sp, self.stop, self.calls = 0x1000E008, 0x30000000, 0
        self.uc.mem_write(self.base + 0x5852E0, bytes(4))
        self.uc.reg_write(UC_X86_REG_MXCSR, 0x1F80)

    def call(self, rva, registers):
        self.uc.mem_write(self.sp, struct.pack("<Q", self.stop))
        self.uc.reg_write(UC_X86_REG_RSP, self.sp)
        for register, value in registers.items():
            self.uc.reg_write(register, value)
        self.uc.emu_start(self.base + rva, self.stop, count=100000)
        if self.uc.reg_read(UC_X86_REG_RIP) != self.stop:
            raise ValueError("Native function did not return")
        self.calls += 1

    def getter(self, index, field):
        self.call(0xC10A0, {UC_X86_REG_RCX: index, UC_X86_REG_RDX: field})
        value = self.uc.reg_read(UC_X86_REG_RAX) & 0xFFFFFFFF
        return value - (1 << 32) if value & 0x80000000 else value

    def scalar(self, effect, field, time, initial):
        frames = effect["frames"]
        current = next((f["global_frame"] for f in frames if f["time"] >= time),
                       frames[-1]["global_frame"])
        self.uc.mem_write(self.sp + 0x28, struct.pack("<f", time))
        self.uc.mem_write(self.sp + 0x30, bytes(8))
        self.uc.mem_write(self.sp + 0x38, struct.pack("<f", initial))
        self.call(0xA3E70, {UC_X86_REG_RCX: field,
            UC_X86_REG_RDX: frames[0]["global_frame"],
            UC_X86_REG_R8: frames[-1]["global_frame"], UC_X86_REG_R9: current})
        value = struct.unpack("<f", struct.pack("<I", self.uc.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF))[0]
        if not math.isfinite(value):
            raise ValueError("Nonfinite native scalar")
        return value

