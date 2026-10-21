"""Export static effective image-resource keys from the pinned native PE.

This is the reviewed initializer's alias-or-primary name inventory, not a
prepared PNG inventory or proof of Native hash lookup/runtime equivalence.
Full names are written only to the caller's output, private build/work by default.
"""


import argparse


from collections import Counter


import hashlib


import json


from pathlib import Path


import re


import struct


import pefile


import image_layout as layout


from image_layout import TARGET_SHA256, TABLE_RVA, LAST_ROW_RVA, STRIDE, ROWS, native_name


EXPECTED_SAMPLES = {
    "img0684.mzp": True,
    "img0684_04_04.mzp": True,
    "img0684_06_00.mzp": True,
    "img0684_06_02.mzp": True,
    "img0684_06_04.mzp": False,
    "img0684_04_06.mzp": False,
}


def fnv1a64(payload: bytes) -> int:
    value = 0xcbf29ce484222325
    for byte in payload:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


def collect_rows(pe, table: bytes, row_count=ROWS):
    """Validate every row; row_count is smaller only in synthetic unit fixtures."""
    if row_count < 1 or STRIDE < 0x28 or len(table) != row_count * STRIDE:
        raise ValueError("Native resource table length/stride mismatch")
    primary_names, alias_names, effective = Counter(), Counter(), Counter()
    empty, alias_examples = [], []
    primary_only = alias_only = different_alias = same_alias = named = 0
    for index in range(row_count):
        row = table[index * STRIDE:(index + 1) * STRIDE]
        primary, alias = struct.unpack_from("<QQ", row)
        if not (primary or alias):
            empty.append(index)
            continue
        # Validate both pointers even though alias takes key precedence.
        names = [native_name(pe, address, index) if address else None for address in (primary, alias)]
        for name in names:
            if name is not None and not re.fullmatch(r"[a-z0-9_]+", name):
                raise ValueError(f"Unexpected normalized native name at row {index}")
        width, height = struct.unpack_from("<HH", row, 0x24)
        if not width or not height:
            raise ValueError(f"Zero native dimensions at row {index}")
        named += 1
        if names[0]:
            primary_names[names[0]] += 1
        if names[1]:
            alias_names[names[1]] += 1
        if names[0] and names[1]:
            if names[0] == names[1]:
                same_alias += 1
            else:
                different_alias += 1
                if len(alias_examples) < 3:
                    alias_examples.append({"row": index, "primary": names[0] + ".mzp", "effectiveAlias": names[1] + ".mzp"})
        elif names[0]:
            primary_only += 1
        else:
            alias_only += 1
        key = names[1] or names[0]
        effective[key] += 1
    duplicate_keys = {name: count for name, count in effective.items() if count > 1}
    if duplicate_keys:
        raise ValueError("Duplicate effective native resource keys")
    names = {name + ".mzp" for name in effective}
    stats = {
        "rowCount": row_count, "rowsVisited": named + len(empty), "namedRows": named,
        "reservedRows": len(empty), "reservedRowIndices": empty, "uniqueEffectiveNames": len(names),
        "effectiveDuplicateKeys": 0, "primaryPointerRows": sum(primary_names.values()),
        "aliasPointerRows": sum(alias_names.values()), "primaryOnlyRows": primary_only,
        "aliasOnlyRows": alias_only, "differentAliasRows": different_alias, "sameAliasRows": same_alias,
        "primaryDuplicateNames": sum(count > 1 for count in primary_names.values()),
        "primaryRepeatedOccurrences": sum(count - 1 for count in primary_names.values()),
        "aliasDuplicateNames": sum(count > 1 for count in alias_names.values()),
        "rawPrimaryAliasUnionNames": len(primary_names.keys() | alias_names.keys()),
        "primaryNamesOutsideEffectiveKeys": len(primary_names.keys() - effective.keys()),
        "aliasExamples": alias_examples,
    }
    return names, stats


def read_table(pe):
    if TABLE_RVA + (ROWS - 1) * STRIDE != LAST_ROW_RVA:
        raise ValueError("Reviewed native table end differs")
    end = TABLE_RVA + ROWS * STRIDE
    section = pe.get_section_by_rva(TABLE_RVA)
    if (section is None or TABLE_RVA < section.VirtualAddress or
            end > section.VirtualAddress + section.SizeOfRawData or end > pe.OPTIONAL_HEADER.SizeOfImage):
        raise ValueError("Native resource table exceeds file-backed image bounds")
    table = pe.get_data(TABLE_RVA, ROWS * STRIDE)
    if len(table) != ROWS * STRIDE:
        raise ValueError("Truncated native resource table")
    return table


def body_for(names):
    if not names or any(not isinstance(name, str) or not re.fullmatch(r"[a-z0-9_]+\.mzp", name) for name in names) or len(set(names)) != len(names):
        raise ValueError("Invalid lowercase native resource names")
    return "".join(name + "\n" for name in sorted(names)).encode("ascii")


def payload_for(names):
    body = body_for(names)
    return f"WOH_NATIVE_IMAGES1 {len(names)} {fnv1a64(body):016x}\n".encode("ascii") + body


def extract(binary: Path, output: Path, evidence: Path):
    if len({binary.resolve(), output.resolve(), evidence.resolve()}) != 3:
        raise ValueError("Input PE and output paths must be distinct")
    raw = binary.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != TARGET_SHA256:
        raise ValueError("Native PE identity differs; refusing the reviewed table RVA")
    pe = pefile.PE(data=raw, fast_load=True)
    try:
        table = read_table(pe)
        names, stats = collect_rows(pe, table)
    finally:
        pe.close()
    membership = {name: name in names for name in EXPECTED_SAMPLES}
    if membership != EXPECTED_SAMPLES:
        raise ValueError("Pinned img0684 membership differs")
    body = body_for(names)
    payload = payload_for(names)
    body_hash = fnv1a64(body)
    proof = {
        "targetSHA256": digest, "reusedHelper": "tools/image_layout.py",
        "reusedHelperSHA256": hashlib.sha256(Path(layout.__file__).read_bytes()).hexdigest(),
        "tableRVA": hex(TABLE_RVA), "lastRowRVA": hex(LAST_ROW_RVA), "stride": STRIDE,
        "tableEndExclusiveRVA": hex(TABLE_RVA + ROWS * STRIDE),
        "fileBackedBoundsVerified": True, "allNamedPointersValidated": True,
        "allNamedDimensionsNonzero": True, "tableSHA256": hashlib.sha256(table).hexdigest(),
        **stats, "keyRule": "lowercase(alias if present else primary) + .mzp",
        "format": "WOH_NATIVE_IMAGES1 <count> <body FNV1a64 lowerhex16> LF; ASCII sorted unique lowercase .mzp names; one name per LF line; final LF; no BOM",
        "rows": len(names), "bytes": len(payload), "outputSHA256": hashlib.sha256(payload).hexdigest(),
        "bodyBytes": len(body), "bodySHA256": hashlib.sha256(body).hexdigest(),
        "bodyFNV1a64Hex": f"{body_hash:016x}", "bodyFNV1a64Decimal": str(body_hash),
        "checksumDefinition": "FNV-1a64 over name body bytes only, including final LF and excluding header; offset cbf29ce484222325, prime 100000001b3, modulo 2^64; accidental-damage check, not cryptographic security",
        "samples": membership, "preparedAssetsRead": False, "originalPEExecuted": False,
        "scope": "Static named-key inventory from the pinned initializer table; not full Native hash lookup equivalence",
        "limits": [
            "Raw primary names suppressed by explicit alias are not added to the existence set",
            "Reserved rows with both pointers zero are skipped by the reviewed initializer rule",
            "A table name does not prove its original resource bytes or prepared PNG exists or decodes",
            "No prepared manifest or filename glob is used as a substitute for native resource names",
            "Native hash collisions, fallback, case handling and dynamic lookup behavior require separate child58 Native evidence",
        ],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(proof, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return proof


