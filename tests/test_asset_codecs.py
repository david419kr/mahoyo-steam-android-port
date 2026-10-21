"""Synthetic format/range tests; no game data is included."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
from asset_codecs import (AssetError, decode_cbg, decode_lenzu, decode_mzp,
                          decode_mzx, read_ccit, unwrap_audio)
from prepare_assets import canonical_name, prepare


def mzx_literal(payload: bytes) -> bytes:
    size = len(payload)
    payload += bytes(size & 1)
    chunks = [bytes((((len(payload[i:i + 128]) // 2 - 1) << 2) | 3,)) +
              payload[i:i + 128] for i in range(0, len(payload), 128)]
    return b"MZX0" + struct.pack("<I", size) + b"".join(chunks)


def mzp(members: list[bytes]) -> bytes:
    rows, body = [], bytearray()
    for member in members:
        start = len(body)
        sector, byte = divmod(start, 2048)
        span = (byte + len(member) + 2047) // 2048
        rows.append(struct.pack("<4H", sector, byte, span, len(member) & 65535))
        body.extend(member)
    return b"mrgd00" + struct.pack("<H", len(members)) + b"".join(rows) + body


def image_header(w, h, tw, th, nx, ny, kind, depth, crop=0) -> bytes:
    return struct.pack("<7H2B", w, h, tw, th, nx, ny, kind, depth, crop)


def palette() -> bytes:
    return b"".join(bytes((index, index, index, 128)) for index in range(256))


def crc(data: bytes) -> int:
    value = 0
    for index, byte in enumerate(data):
        value = ((value + byte) * (233, 277, 311, 433)[index % 4]) & ((1 << 64) - 1)
    return value


def lenzu_two_literal() -> bytes:
    payload = b"AB"
    checksum = crc(payload)
    header = b"LenZuCompressor\0" + b"1\0\0\x000\0\0\0" + bytes(8)
    header += struct.pack("<4I", 2, checksum >> 32, checksum & 0xffffffff, 0)
    header += bytes((0, 3, 3, 3, 0, 2))
    header += bytes((1, 1)) + struct.pack("<I", 1)  # one symbol, value 1
    bits = "01" + "01000001" + "01000010"  # literal, one-bit tree, A, B
    bits += "0" * ((-len(bits)) % 8)
    return header + int(bits, 2).to_bytes(len(bits) // 8, "big")


def cbg(width: int, height: int, residual: bytes, *, channels=1) -> bytes:
    # A complete equal-weight alphabet gives paths equal to symbol MSB-first.
    # CBG reads each byte LSB-first, so reverse its eight bits in the stream.
    runs = bytes((len(residual),)) + residual
    encoded = bytes(int(f"{symbol:08b}"[::-1], 2) for symbol in runs)
    stripe = struct.pack("<I", len(runs)) + bytes([1]) * 256 + encoded
    return (b"CompressedBG_MT\0" + struct.pack("<4I", width, height, height, channels * 8) +
            bytes(16) + struct.pack("<I", 52) + stripe)


class MzxTests(unittest.TestCase):
    def test_literals_rle_backref_ring_and_odd(self):
        # Two literals, repeat B, repeat previous two words, ring slot A.
        commands = b"\x07AABB\x00\x05\x01\x02"
        data = b"MZX0" + struct.pack("<I", 12) + commands
        self.assertEqual(decode_mzx(data), b"AABBBBBBBBAA")
        self.assertEqual(decode_mzx(mzx_literal(b"12345")), b"12345")
        self.assertEqual(decode_mzx(b"MZX0\x02\0\0\0\xfc"), bytes(2))
        self.assertEqual(decode_mzx(mzx_literal(b"AB"), invert=True), bytes((190, 189)))

    def test_initial_ring_and_reset(self):
        self.assertEqual(decode_mzx(b"MZX0\x02\0\0\0\x02"), bytes(2))
        block = b"\xff" + b"A" * 128
        data = b"MZX0" + struct.pack("<I", 8194) + block * 64 + b"\x00"
        self.assertEqual(decode_mzx(data), b"A" * 8192 + bytes(2))

    def test_bad_reference_truncation_allocation_and_overflow(self):
        for data in (b"MZX0\x02\0\0\0\x01\0", b"MZX0\x02\0\0\0\x03A",
                     b"MZX0\0\0\0\xff", b"MZX0\x02\0\0\0\xff"):
            with self.assertRaises(AssetError):
                decode_mzx(data)


class ImageTests(unittest.TestCase):
    def test_indexed_size_palette_alpha_and_swizzle(self):
        data = mzp([image_header(2, 2, 2, 2, 1, 1, 1, 0x11) + palette(),
                    mzx_literal(bytes((0, 8, 16, 255)))])
        image, meta = decode_mzp(data)
        self.assertEqual(image.size, (2, 2))  # no reference decoder's +1 border
        self.assertEqual(list(image.getdata()), [(0, 0, 0, 255), (16, 16, 16, 255),
                                                  (8, 8, 8, 255), (255, 255, 255, 255)])
        self.assertEqual(meta["bitmap_type"], 1)

    def test_hep_alpha_and_tile_border(self):
        pal = bytearray(palette())
        pal[4:8] = bytes((20, 30, 40, 64))
        hep = struct.pack("<8I", 0x00504548, 32 + 16 + 1024, 0, 0, 16, 4, 4, 1)
        hep += bytes([1]) * 16 + pal
        data = mzp([image_header(8, 4, 4, 4, 2, 1, 0x0c, 0x11, 1),
                    mzx_literal(hep), mzx_literal(hep)])
        image, meta = decode_mzp(data)
        self.assertEqual(image.size, (6, 4))
        self.assertEqual(image.getpixel((5, 3)), (20, 30, 40, 129))

    def test_rgb565_low_bits(self):
        payload = struct.pack("<H", 0xf81f) + b"\xe7\x63"
        image, _ = decode_mzp(mzp([image_header(1, 1, 1, 1, 1, 1, 0x0b, 0x14),
                                  mzx_literal(payload)]))
        self.assertEqual(image.getpixel((0, 0)), (255, 0, 255, 99))

    def test_cbg_huffman_predictor_and_channels(self):
        image, _ = decode_cbg(cbg(2, 2, bytes((3, 5, 6, 9))))
        self.assertEqual([p[0] for p in image.getdata()], [3, 8, 9, 17])
        image, _ = decode_cbg(cbg(1, 1, bytes((3, 5, 7, 9)), channels=4))
        self.assertEqual(image.getpixel((0, 0)), (7, 5, 3, 9))

    def test_nested_range_truncation_and_geometry(self):
        data = mzp([image_header(2, 2, 2, 2, 1, 1, 1, 1) + palette(),
                    mzx_literal(bytes(4))])
        with self.assertRaises(AssetError):
            decode_mzp(data[:-1])
        broken = bytearray(cbg(1, 1, b"\0"))
        struct.pack_into("<I", broken, 48, len(broken) + 1)
        with self.assertRaises(AssetError):
            decode_cbg(broken)


class MetadataTests(unittest.TestCase):
    def test_lenzu_crc_and_truncation(self):
        data = lenzu_two_literal()
        self.assertEqual(decode_lenzu(data), b"AB")
        bad = bytearray(data)
        bad[36] ^= 1
        with self.assertRaises(AssetError):
            decode_lenzu(bad)
        with self.assertRaises(AssetError):
            decode_lenzu(data[:-2])

    def test_ccit_utf8_and_explicit_metrics(self):
        key = int.from_bytes("あ".encode(), "big")
        raw = struct.pack("<6I", 1, 8, key, 0, 1, 20) + struct.pack("<5I", 0, 2, 3, 4, 5)
        result = read_ccit(raw)
        self.assertEqual(result["characters"][0]["character"], "あ")
        self.assertEqual(result["characters"][0]["metrics"]["width"], 4)
        with self.assertRaises(AssetError):
            read_ccit(raw[:-1])

    def test_audio_wrapper(self):
        raw = struct.pack("<I4s4I", 64, b"hw  ", 4, 0, 48000, 2) + bytes(40) + b"OggS"
        payload, meta = unwrap_audio(raw)
        self.assertEqual(payload, b"OggS")
        self.assertEqual(meta["sample_rate"], 48000)
        with self.assertRaises(AssetError):
            unwrap_audio(raw[:-1])

    def test_safe_names_and_source_output_guard(self):
        self.assertEqual(canonical_name("Title.CBG"), "title.cbg")
        for name in ("../title.cbg", "C:title", "a/b", "/x", "", "x\n"):
            with self.assertRaises(AssetError):
                canonical_name(name)
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(AssetError):
                prepare(Path(temp), Path(temp) / "bad", [])

    def test_incremental_conversion_and_last_duplicate_wins(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            source, output = base / "source", base / "output"
            source.mkdir()
            first, last = cbg(1, 1, bytes((10,))), cbg(1, 1, bytes((30,)))
            table = bytearray()
            position = 0
            for name, data in ((b"SAMPLE.cbg", first), (b"sample.cbg", last)):
                table.extend(name.ljust(96, b"\0") +
                             struct.pack("<II", position, len(data)) + bytes(24))
                position += len(data)
            (source / "data00000.hfa").write_bytes(b"HUNEXGGEFA10" +
                struct.pack("<I", 2) + table + first + last)
            result = prepare(source, output, ["SAMPLE.CBG"])
            self.assertEqual(result["converted"], 1)
            from PIL import Image
            path = output / "images/sample.cbg.png"
            self.assertEqual(Image.open(path).getpixel((0, 0)), (30, 30, 30, 255))
            timestamp = path.stat().st_mtime_ns
            second = prepare(source, output, ["sample.cbg"])
            self.assertEqual(second["reused"], 1)
            self.assertEqual(path.stat().st_mtime_ns, timestamp)
            manifest = json.loads((output / "assets.json").read_text())
            self.assertEqual(manifest["assets"]["sample.cbg"]["source"]["index"], 1)
            # Corrupt converted output is regenerated rather than trusted.
            path.write_bytes(b"broken")
            repaired = prepare(source, output, ["sample.cbg"])
            self.assertEqual(repaired["converted"], 1)







if __name__ == "__main__":
    unittest.main()
