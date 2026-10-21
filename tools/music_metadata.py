"""Recover the bounded HuneX numeric music table from the reviewed native PE.

Only asset filenames are emitted. Input game bytes are never changed.
"""


import argparse


import hashlib


import json


from pathlib import Path


import re


import struct


import pefile


TARGET_SHA256 = "1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"


TABLE_RVA = 0x5806D0


ROWS = 66


def extract(binary: Path, output: Path, evidence: Path) -> None:
    raw = binary.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != TARGET_SHA256:
        raise ValueError("Native PE identity differs; do not reuse version-specific table RVA")
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    pointers = struct.unpack(f"<{ROWS}Q", pe.get_data(TABLE_RVA, ROWS * 8))
    rows, proof = [], []
    for index, pointer in enumerate(pointers):
        if not pointer:
            continue
        if not base <= pointer < base + pe.OPTIONAL_HEADER.SizeOfImage:
            raise ValueError(f"Music pointer outside image at row {index}")
        data = pe.get_data(pointer - base, 128)
        name = data.decode("utf-16le").split("\0", 1)[0]
        if not re.fullmatch(r"[A-Za-z0-9_]+\.hw", name):
            raise ValueError(f"Unexpected audio asset at row {index}: {name!r}")
        rows.append(f"{index}\t{name.lower()}\n")
        proof.append({"id": index, "pointer_va": hex(pointer), "asset": name,
                      "utf16_hex": name.encode("utf-16le").hex()})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(rows), encoding="utf-8")
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps({"target_sha256": digest, "table_rva": hex(TABLE_RVA),
                                   "row_count": ROWS, "null_ids": [0], "records": proof},
                                  indent=2) + "\n", encoding="utf-8")


