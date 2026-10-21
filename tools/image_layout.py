"""Extract prepared MZP anchor sizes from the reviewed native layout table.

Output dimensions remain in native coordinates. No bitmap cropping or scaling
is inferred. Original code, images and the full table stay outside Git.
"""


import argparse


import hashlib


import json


from pathlib import Path


import re


import struct


import pefile


TARGET_SHA256 = "1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"


TABLE_RVA = 0x2F5308


LAST_ROW_RVA = 0x3B4000


STRIDE = 0x48


ROWS = 10856


NAME_PATTERN = re.compile(r"[A-Za-z0-9_]+\.mzp", re.IGNORECASE)


PREPARED_NAME_PATTERN = re.compile(r"[A-Za-z0-9_]+(?:[.-][A-Za-z0-9_]+)*\.mzp", re.IGNORECASE)


VARIANT_PATTERN = re.compile(r"(.+)_\d{2}_\d{2}$")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate manifest key: {key!r}")
        result[key] = value
    return result


def prepared_basename(name, layouts):
    """Strip generated variants only when a corresponding base layout exists.

    >>> prepared_basename('img0408_00_20.mzp', {'img0408': None})
    'img0408'
    >>> prepared_basename('ari_a_13_11_00.mzp', {'ari_a_13_11_00': None})
    'ari_a_13_11_00'
    >>> prepared_basename('img0408.mzp', {'img0408': None})
    'img0408'
    """
    if not isinstance(name, str) or not PREPARED_NAME_PATTERN.fullmatch(name):
        raise ValueError(f"Unexpected prepared MZP name: {name!r}")
    stem = name[:-4].lower()
    if stem in layouts:
        return stem
    variant = VARIANT_PATTERN.fullmatch(stem)
    if variant and variant[1] in layouts:
        return variant[1]
    return stem


def native_name(pe, pointer, row_index):
    base = pe.OPTIONAL_HEADER.ImageBase
    if not base <= pointer < base + pe.OPTIONAL_HEADER.SizeOfImage:
        raise ValueError(f"Layout name pointer outside PE image at row {row_index}")
    rva = pointer - base
    section = pe.get_section_by_rva(rva)
    if section is None or not section.VirtualAddress <= rva < section.VirtualAddress + section.SizeOfRawData:
        raise ValueError(f"Layout name pointer has no file-backed section at row {row_index}")
    limit = min(512, section.VirtualAddress + section.SizeOfRawData - rva)
    data = pe.get_data(rva, limit)
    terminator = data.find(b"\0")
    if terminator < 0:
        raise ValueError(f"Unterminated layout name at row {row_index}")
    name = data[:terminator].decode("ascii")
    if not NAME_PATTERN.fullmatch(name):
        raise ValueError(f"Unexpected layout name at row {row_index}: {name!r}")
    return name[:-4].lower()


def extract(binary: Path, assets: Path, output: Path, evidence: Path,
            origins_output: Path | None = None):
    destinations = [output.resolve(), evidence.resolve()]
    if origins_output is not None:
        destinations.append(origins_output.resolve())
    if len(set(destinations)) != len(destinations) or any(
            path in (binary.resolve(), assets.resolve()) for path in destinations):
        raise ValueError("Native input, manifest and output paths must be distinct")
    raw = binary.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != TARGET_SHA256:
        raise ValueError("Native PE identity differs; do not reuse the layout table RVA")
    if TABLE_RVA + (ROWS - 1) * STRIDE != LAST_ROW_RVA:
        raise ValueError("Layout table bounds differ from the reviewed initializer")
    if ROWS < 1 or STRIDE < 0x34:
        raise ValueError("Native origin fields exceed the reviewed row bounds")
    pe = pefile.PE(data=raw)
    table_end = TABLE_RVA + ROWS * STRIDE
    section = pe.get_section_by_rva(TABLE_RVA)
    if (section is None or TABLE_RVA < section.VirtualAddress or
            table_end > section.VirtualAddress + section.SizeOfRawData or
            table_end > pe.OPTIONAL_HEADER.SizeOfImage):
        raise ValueError("Native layout table exceeds file-backed image bounds")
    table = pe.get_data(TABLE_RVA, ROWS * STRIDE)
    if len(table) != ROWS * STRIDE:
        raise ValueError("Truncated native layout table")
    layouts, origins, empty_rows = {}, {}, []
    for index in range(ROWS):
        row = table[index * STRIDE:(index + 1) * STRIDE]
        primary, alias = struct.unpack_from("<QQ", row)
        if not (primary or alias):
            # The native initializer explicitly skips these reserved rows.
            empty_rows.append(index)
            continue
        names = [native_name(pe, pointer, index) if pointer else None
                 for pointer in (primary, alias)]
        # The initializer derives its lookup key from alias before primary.
        key = names[1] or names[0]
        width, height = struct.unpack_from("<HH", row, 0x24)
        if width == 0 or height == 0:
            raise ValueError(f"Zero native anchor dimensions at row {index}")
        if key in layouts:
            raise ValueError(f"Duplicate native layout basename: {key}")
        layouts[key] = {"width": width, "height": height, "row_index": index,
                        "row_rva": hex(TABLE_RVA + index * STRIDE),
                        "primary_pointer_va": hex(primary), "alias_pointer_va": hex(alias),
                        "row_bytes": row.hex()}
        # These default resource origins are independent of the u16 W/H fields.
        origins[key] = struct.unpack_from("<2i", row, 0x2c)

    manifest_raw = assets.read_bytes()
    manifest = json.loads(manifest_raw, object_pairs_hook=unique_object)
    if not isinstance(manifest, dict) or not isinstance(manifest.get("assets"), dict):
        raise ValueError("Expected a prepared assets.json object with an assets map")
    prepared = [name for name in manifest["assets"] if name.lower().endswith(".mzp")]
    if not prepared:
        raise ValueError("The asset manifest contains no prepared MZP files")
    requested = {prepared_basename(name, layouts) for name in prepared}
    missing = sorted(requested - layouts.keys())
    selected = sorted(requested & layouts.keys())
    payload = "".join(f"{name}\t{layouts[name]['width']}\t{layouts[name]['height']}\n"
                      for name in selected).encode("utf-8")
    samples = sorted({name for name in ("img0408", "img2326", selected[0] if selected else None)
                      if name in selected})
    proof = {
        "target_sha256": digest,
        "initializer_va": "0x1400a07e0",
        "initializer_evidence_id": None,
        "lookup_va": "0x1400a0dc0",
        "lookup_evidence_id": None,
        "table_rva": hex(TABLE_RVA), "last_row_rva": hex(LAST_ROW_RVA),
        "stride": STRIDE, "row_count": ROWS, "named_rows": len(layouts),
        "empty_rows": len(empty_rows),
        "native_width_offset": "0x24:u16", "native_height_offset": "0x26:u16",
        "units": "Unscaled native coordinates; no alpha crop, zoom or pixel-size inference",
        "lookup_key": "Lowercase filename without extension; alias pointer takes precedence",
        "manifest_sha256": hashlib.sha256(manifest_raw).hexdigest(),
        "prepared_mzp_count": len(prepared), "selected_basename_count": len(selected),
        "missing_basenames": missing,
        "output_sha256": hashlib.sha256(payload).hexdigest(), "output_bytes": len(payload),
        "samples": [{"basename": name, **layouts[name]} for name in samples],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    if origins_output is not None:
        origin_payload = "".join(f"{name}\t{origins[name][0]}\t{origins[name][1]}\n"
                                 for name in selected).encode("utf-8")
        origins_output.parent.mkdir(parents=True, exist_ok=True)
        origins_output.write_bytes(origin_payload)
        origin_samples = sorted({name for name in ("tob_n_00_01_02", "tob_n_00_01_04",
                                                  "img2326", selected[0] if selected else None)
                                 if name in selected})
        proof["origins_sidecar"] = {
            "schema_version": 1, "columns": ["name", "originX", "originY"],
            "original_sha256": digest, "field_type": "signed int32, unchanged",
            "field_offsets": ["0x2c", "0x30"],
            "native_lookup_node_offsets": ["0x58", "0x5c"],
            "units": "Unscaled native resource origins; not width/height or inferred half-size",
            "lookup_key": proof["lookup_key"], "rows": len(selected),
            "output_bytes": len(origin_payload),
            "output_sha256": hashlib.sha256(origin_payload).hexdigest(),
            "table_end_exclusive_rva": hex(table_end),
            "file_backed_table_bounds_verified": True,
            "samples": [{"basename": name, "originX": origins[name][0],
                         "originY": origins[name][1], **layouts[name]}
                        for name in origin_samples],
        }
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    return proof


