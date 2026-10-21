"""Bounded, read-only HFA archive reader."""
from __future__ import annotations


import argparse


from collections import Counter


from dataclasses import asdict, dataclass


import hashlib


import importlib


import json


from pathlib import Path


import struct


import sys


import types


from typing import BinaryIO


HFA_MAGIC = b"HUNEXGGEFA10"


HFA_HEADER = 16


HFA_ENTRY = 128


MAX_INDEX_BYTES = 64 * 1024 * 1024


MAX_PROBE = 8192


REFERENCE_COMMIT = "2d3f47c8cc3c31ad1f5d4c62b1726ce460331ae2"


REFERENCE_HASHES = {
    "lenzu.py": "75c269e2f8af5fcb89d3a42b9d8d38a06f0ce9b6702b5cb31397718fbf025bbc",
    "utils/io.py": "ae6ca59f3b804bd4da492067eaa9ec248f3670981811d9937bc3fe17df7ad129",
    "utils/huffman.py": "3d7ab635f4145f62f9f09e9cbfaa2a4aabd451a432b93b0db1f9d02a6fb78691",
    "utils/bitstream.py": "45afc504d8a2e021387c02770bde007f1ede797ee1650aac9a5d35e3d66ec5ba",
}


class FormatError(ValueError):
    """A header violates its format or the source file's bounds."""


@dataclass(frozen=True)
class Entry:
    name: str
    offset: int
    size: int
    index_offset: int
    reserved_hex: str


def read_at(src: BinaryIO, offset: int, count: int, limit: int) -> bytes:
    if offset < 0 or count < 0 or offset > limit or count > limit - offset:
        raise FormatError(f"range {offset:#x}+{count:#x} exceeds {limit:#x}")
    src.seek(offset)
    result = src.read(count)
    if len(result) != count:
        raise FormatError("source ended before the declared range")
    return result


def read_index(src: BinaryIO, size: int) -> tuple[list[Entry], dict]:
    header = read_at(src, 0, HFA_HEADER, size)
    if header[:12] != HFA_MAGIC:
        raise FormatError("not a HUNEXGGEFA10 archive")
    count = struct.unpack_from("<I", header, 12)[0]
    index_size = count * HFA_ENTRY
    if index_size > MAX_INDEX_BYTES:
        raise FormatError("index exceeds the explicit 64 MiB inventory bound")
    data_start = HFA_HEADER + index_size
    index = read_at(src, HFA_HEADER, index_size, size)
    entries = []
    names: Counter[str] = Counter()
    for i in range(count):
        row = index[i * HFA_ENTRY:(i + 1) * HFA_ENTRY]
        if b"\0" not in row[:96]:
            raise FormatError(f"entry {i} has no terminated filename")
        try:
            name = row[:96].split(b"\0", 1)[0].decode("utf-8")
        except UnicodeDecodeError as exc:
            raise FormatError(f"entry {i} filename is not UTF-8") from exc
        if not name:
            raise FormatError(f"empty entry name at {i}")
        names[name] += 1
        relative, length = struct.unpack_from("<II", row, 96)
        absolute = data_start + relative
        if absolute > size or length > size - absolute:
            raise FormatError(f"entry {i} is outside its HFA")
        entries.append(Entry(name, absolute, length,
                             HFA_HEADER + i * HFA_ENTRY, row[104:].hex()))
    occupied = sorted((e.offset, e.offset + e.size) for e in entries if e.size)
    last = data_start
    gap_bytes = 0
    for start, end in occupied:
        if start < last:
            raise FormatError("overlapping payload ranges")
        gap_bytes += start - last
        last = end
    return entries, {
        "entry_count": count, "data_start": data_start,
        "index_sha256": hashlib.sha256(header + index).hexdigest(),
        "payload_bytes": sum(e.size for e in entries),
        "gap_bytes": gap_bytes, "trailing_bytes": size - last,
        "duplicate_names": {k: v for k, v in names.items() if v > 1},
    }


def mzp_size(byte_offset: int, sectors: int, low16: int) -> int:
    """Resolve full size from its low 16 bits and occupied 0x800 sectors."""
    if byte_offset >= 0x800:
        raise FormatError("MZP sub-sector offset is not canonical")
    lower = max(0, (sectors - 1) * 0x800 - byte_offset + 1)
    upper = sectors * 0x800 - byte_offset
    if sectors == 0:
        lower = upper = 0
    high = max(0, (lower - low16 + 0xFFFF) // 0x10000)
    result = low16 + high * 0x10000
    if result < lower or result > upper:
        raise FormatError("inconsistent MZP sector span and low size")
    return result


def inspect_mzp(src: BinaryIO, entry: Entry, source_size: int) -> dict:
    head = read_at(src, entry.offset, min(8, entry.size), source_size)
    if len(head) != 8 or head[:6] != b"mrgd00":
        raise FormatError("invalid MZP header")
    count = struct.unpack_from("<H", head, 6)[0]
    base = 8 + count * 8
    if count == 0 or base > entry.size:
        raise FormatError("invalid MZP entry table")
    table = read_at(src, entry.offset + 8, count * 8, source_size)
    children = []
    for i in range(count):
        sector, byte, span, low = struct.unpack_from("<4H", table, i * 8)
        size = mzp_size(byte, span, low)
        offset = base + sector * 0x800 + byte
        if offset > entry.size or size > entry.size - offset:
            raise FormatError(f"MZP child {i} exceeds its parent")
        children.append((entry.offset + offset, size))
    meta_offset, meta_size = children[0]
    if meta_size < 16:
        raise FormatError("MZP image metadata is shorter than 16 bytes")
    dims = struct.unpack("<7H2B", read_at(src, meta_offset, 16, source_size))
    result = dict(zip(("width", "height", "tile_width", "tile_height",
                       "tiles_x", "tiles_y", "bitmap_type", "bitmap_depth",
                       "tile_crop"), dims))
    result["subentry_count"] = count
    result["metadata_offset"] = meta_offset
    if count > 1:
        offset, size = children[1]
        result["first_tile_prefix_hex"] = read_at(src, offset, min(8, size), source_size).hex()
    return result


def inspect_payload(src: BinaryIO, entry: Entry, source_size: int) -> dict:
    p = read_at(src, entry.offset, min(MAX_PROBE, entry.size), source_size)
    result: dict = {"prefix_hex": p[:64].hex()}
    if p.startswith(b"mrgd00"):
        result.update(kind="mzp", **inspect_mzp(src, entry, source_size))
    elif p.startswith(b"CompressedBG_MT\0"):
        if len(p) < 48:
            raise FormatError("truncated CBG header")
        width, height, stripe_height, bpp = struct.unpack_from("<4I", p, 16)
        if not width or not height or not stripe_height or bpp not in (8, 24, 32):
            raise FormatError("invalid CBG dimensions/depth")
        stripes = (height + stripe_height - 1) // stripe_height
        if 48 + stripes * 4 > entry.size:
            raise FormatError("CBG stripe index exceeds payload")
        result.update(kind="cbg", width=width, height=height,
                      stripe_height=stripe_height, bits_per_pixel=bpp)
    elif p.startswith(b"LenZuCompressor\0"):
        if len(p) < 54:
            raise FormatError("truncated LenZu header")
        result.update(kind="lenzu", declared_uncompressed_bytes=struct.unpack_from("<I", p, 32)[0],
                      options_hex=p[48:54].hex(), version_hex=p[16:32].hex())
    elif len(p) >= 64 and p[4:8] == b"hw  ":
        header_size, payload_bytes, field_0c, rate, channels = struct.unpack_from("<I4x4I", p)
        if header_size != 64 or payload_bytes != entry.size - 64 or p[64:68] != b"OggS":
            raise FormatError("HW wrapper does not match a bounded Ogg payload")
        ident = p.find(b"\x01vorbis", 64)
        result.update(kind="hw-ogg", header_size=header_size,
                      payload_bytes=payload_bytes, field_0c=field_0c,
                      wrapper_sample_rate=rate, wrapper_channels=channels)
        if ident >= 0 and ident + 30 <= len(p):
            result.update(codec="vorbis", channels=p[ident + 11],
                          sample_rate=struct.unpack_from("<I", p, ident + 12)[0])
    elif len(p) >= 12 and p[4:8] == b"ftyp":
        result.update(kind="isobmff", major_brand=p[8:12].decode("ascii", "replace"))
    else:
        result["kind"] = "unknown"
    return result


