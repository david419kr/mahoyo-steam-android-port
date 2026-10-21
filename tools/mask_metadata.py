"""Recover the reviewed native rule-mask map and its transition reference math.

Only filenames and numerical evidence are emitted. Native code, shader assembly
and decoded mask images remain under ignored build/work.
"""


import argparse


import hashlib


import json


import math


from pathlib import Path


import re


import struct


import pefile


TARGET_SHA256 = "1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"


TABLE_RVA = 0x5805E0


TABLE_ROWS = 15


def f32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def transition_progress(elapsed, duration, strength):
    """Native mask progress is quantized before the strength-dependent powf.

    >>> transition_progress(500, 1000, 256)
    128
    >>> transition_progress(500, 1000, 20)
    41
    >>> transition_progress(500, 1000, 70)
    75
    """
    if not 0 <= strength <= 256 or elapsed < 0 or duration < 0:
        raise ValueError("Invalid native mask strength, duration or elapsed time")
    if duration == 0 or elapsed >= duration:
        return 256
    value = elapsed * 256 // duration
    if strength < 100:
        exponent = 0.25 if strength < 50 else 0.5
        value = 256 - int(f32(f32(math.pow(f32((256 - value) / 256), exponent)) * 256))
    return value


def mask_constants(progress, strength):
    if not 0 <= progress <= 256 or not 0 <= strength <= 256:
        raise ValueError("Native mask progress and strength must be 0..256")
    # Every value is an exactly representable multiple of 1/256; native epsilon
    # snapping does not change these values before the -1..2 clamps.
    high = f32(progress / 128)
    low = f32(high - f32(strength / 256))
    return max(-1.0, low), min(2.0, high)


def mask_old_alpha(mask, progress, strength):
    """Alpha of the opaque captured old frame, for native reference flag 1.

    >>> mask_old_alpha(0.5, 128, 256)
    0.5
    >>> [mask_old_alpha(0.5, p, 0) for p in (0, 1)]
    [1.0, 0.0]
    """
    if not 0 <= mask <= 1:
        raise ValueError("Mask samples must be the original byte divided by 255")
    low, high = mask_constants(progress, strength)
    if high <= low:
        return 1.0 if low == 0 else 0.0
    value = max(0.0, min(1.0, (mask - low) / (high - low)))
    return value * value * (3 - 2 * value)


def extract(binary: Path, output: Path, evidence: Path):
    raw = binary.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != TARGET_SHA256:
        raise ValueError("Native PE identity differs; do not reuse the mask table RVA")
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    table = pe.get_data(TABLE_RVA, TABLE_ROWS * 8)
    if len(table) != TABLE_ROWS * 8:
        raise ValueError("Truncated native mask map")
    pointers = struct.unpack(f"<{TABLE_ROWS}Q", table)
    if pointers[0] != 0 or any(pointer == 0 for pointer in pointers[1:]):
        raise ValueError("Native mask map must have NULL id0 and fourteen named rules")
    records, names = [], set()
    for index, pointer in enumerate(pointers[1:], 1):
        rva = pointer - base
        section = pe.get_section_by_rva(rva)
        if not base <= pointer < base + pe.OPTIONAL_HEADER.SizeOfImage or section is None:
            raise ValueError(f"Mask pointer outside the native PE at id {index}")
        limit = min(128, section.VirtualAddress + section.SizeOfRawData - rva)
        if limit < 2:
            raise ValueError(f"Mask pointer has no file-backed string at id {index}")
        data = pe.get_data(rva, limit)
        end = next((offset for offset in range(0, len(data) - 1, 2)
                    if data[offset:offset + 2] == b"\0\0"), None)
        if end is None:
            raise ValueError(f"Unterminated native mask name at id {index}")
        name = data[:end].decode("utf-16le")
        if not re.fullmatch(r"rule_\d{2}\.cbg", name) or name in names:
            raise ValueError(f"Unexpected or duplicate native mask name at id {index}")
        names.add(name)
        records.append({"id": index, "name": name, "pointer_va": hex(pointer),
                        "pointer_table_rva": hex(TABLE_RVA + index * 8),
                        "utf16_hex": data[:end + 2].hex()})
    payload = "".join(f"{row['id']}\t{row['name']}\n" for row in records).encode()
    proof = {
        "target_sha256": digest, "table_rva": hex(TABLE_RVA), "row_count": TABLE_ROWS,
        "null_ids": [0], "pointer_table_sha256": hashlib.sha256(table).hexdigest(),
        "output_sha256": hashlib.sha256(payload).hexdigest(), "records": records,
        "fader_va": "0x14014c840",
        "fader_evidence_id": None,
        "constants_evidence_id": None,
        "upload_evidence_id": None,
        "progress_domain": [0, 256], "strength_domain": [0, 256],
        "shader_resources": [134, 135], "mask_sampler": "POINT/NEAREST",
        "mask_coordinates": "(native destination pixel + 0.5) / mask texture storage size",
        "old_alpha": "smoothstep(progress/128-strength/256,progress/128,originalMaskByte/255)",
        "strength_zero": "Copy old at progress0; do not draw old once progress>0",
    }
    validation = Path("build/work/mask-validation.json")
    if validation.exists():
        checked = json.loads(validation.read_text(encoding="utf-8"))
        if checked.get("target_sha256") == digest:
            proof["arithmetic_validation"] = checked
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    return proof


