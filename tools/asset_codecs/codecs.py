"""Bounded read-only HuneX asset decoders; image dimensions are preserved."""
from __future__ import annotations

import heapq
import struct
from typing import Sequence

MAX_OUTPUT = 256 * 1024 * 1024
MAX_PIXELS = 64 * 1024 * 1024


class AssetError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes | memoryview):
        self.data = memoryview(data)
        self.pos = 0

    def take(self, count: int) -> memoryview:
        if count < 0 or count > len(self.data) - self.pos:
            raise AssetError(f"truncated payload at {self.pos:#x}, need {count}")
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def byte(self) -> int:
        return self.take(1)[0]

    def uint(self, count: int = 4) -> int:
        return int.from_bytes(self.take(count), "little")

    def varint(self) -> int:
        result = 0
        for shift in range(0, 35, 7):
            value = self.byte()
            result |= (value & 127) << shift
            if not value & 128:
                return result
        raise AssetError("variable integer exceeds 35 bits")


class Bits:
    def __init__(self, src: Reader, msb: bool):
        self.src, self.msb = src, msb
        self.current = self.remaining = 0

    def read(self, count: int = 1) -> int:
        result = 0
        for shift in range(count):
            if not self.remaining:
                self.current = self.src.byte()
                self.remaining = 8
            self.remaining -= 1
            if self.msb:
                result = (result << 1) | ((self.current >> self.remaining) & 1)
            else:
                result |= (self.current & 1) << shift
                self.current >>= 1
        return result


def _limit(length: int, bound: int = MAX_OUTPUT) -> None:
    if length < 0 or length > bound:
        raise AssetError(f"decoded allocation {length} exceeds bound {bound}")


def _tree(weights: Sequence[tuple[int, int]], inverted: bool = False):
    """Stable weight/index priority reproduces the format's tie ordering."""
    queue = []
    serial = len(weights)
    for order, (symbol, weight) in enumerate(weights):
        if weight:
            heapq.heappush(queue, (weight, order, symbol))
    if not queue:
        raise AssetError("empty Huffman alphabet")
    # A single occupied leaf still has a one-bit branch in these streams.
    if len(queue) == 1:
        symbol = queue[0][2]
        return (None, symbol) if inverted else (symbol, None)
    while len(queue) > 1:
        wa, _, a = heapq.heappop(queue)
        wb, _, b = heapq.heappop(queue)
        node = (b, a) if inverted else (a, b)
        heapq.heappush(queue, (wa + wb, serial, node))
        serial += 1
    return queue[0][2]


def _symbol(bits: Bits, node) -> int:
    while isinstance(node, tuple):
        node = node[bits.read()]
        if node is None:
            raise AssetError("invalid empty Huffman branch")
    return node


def _backcopy(output: bytearray, distance: int, length: int) -> None:
    if distance < 1 or distance > len(output):
        raise AssetError("back-reference points before decoded data")
    # Repeating the referenced span also handles overlapping LZ copies.
    base = output[-distance:]
    output.extend((base * ((length + distance - 1) // distance))[:length])


def decode_mzx(data: bytes | memoryview, *, invert: bool = False) -> bytes:
    src = Reader(data)
    if bytes(src.take(4)) != b"MZX0":
        raise AssetError("not MZX0")
    length = src.uint()
    _limit(length)
    out = bytearray()
    ring = [b"\xff\xff" if invert else b"\0\0"] * 64
    ring_pos = 0
    reset = 0
    while len(out) < length:
        flags = src.byte()
        command, argument = flags & 3, flags >> 2
        if reset <= 0:
            reset = 4096
        count = 1 if command == 2 else argument + 1
        # Stored streams can finish with a padded command spanning beyond the
        # declared byte count. A command is at most 128 bytes, and only its
        # declared output prefix is retained.
        if command == 0:
            previous = ((b"\xff\xff" if invert else b"\0\0")
                        if reset == 4096 else bytes(out[-2:]))
            out.extend(previous * count)
        elif command == 1:
            _backcopy(out, 2 * (src.byte() + 1), count * 2)
        elif command == 2:
            out.extend(ring[argument])
        else:
            block = bytes(src.take(count * 2))
            if invert:
                block = bytes(v ^ 255 for v in block)
            out.extend(block)
            for word in range(count):
                ring[ring_pos] = block[2 * word:2 * word + 2]
                ring_pos = (ring_pos + 1) & 63
        reset -= count
    return bytes(out[:length])


def decode_lenzu(data: bytes | memoryview) -> bytes:
    src = Reader(data)
    magic = bytes(src.take(32))
    if magic != b"LenZuCompressor\0" + b"1\0\0\x000\0\0\0" + bytes(8):
        raise AssetError("unsupported LenZu signature/version")
    length, high, low, _ = struct.unpack("<4I", src.take(16))
    _limit(length)
    _, raw_bits, min_bits, upper, low_bits, base = src.take(6)
    alphabet_bits = max(raw_bits, min_bits)
    if not (3 <= raw_bits < 16 and 3 <= min_bits < 16 and
            0 <= low_bits < upper < 16 and
            alphabet_bits >= upper - low_bits and 2 <= base < 9):
        raise AssetError("unsupported LenZu options")
    alphabet_size = 1 << alphabet_bits
    # Existing HuneX files use a byte for the sparse alphabet count/index.
    index_width = ((alphabet_bits + 7) // 8 + 7) // 8
    count = src.uint(index_width) or alphabet_size
    if count > alphabet_size:
        raise AssetError("LenZu alphabet count exceeds range")
    if alphabet_size * 4 < (((alphabet_bits + 7) // 8) + 4) * count:
        weights = [(symbol, src.uint()) for symbol in range(alphabet_size)]
    else:
        weights = [(src.uint(index_width), src.uint()) for _ in range(count)]
    if len({symbol for symbol, _ in weights}) != len(weights):
        raise AssetError("duplicate Huffman symbols")
    if any(symbol >= alphabet_size for symbol, _ in weights):
        raise AssetError("LenZu Huffman symbol exceeds alphabet")
    root = _tree(weights, inverted=True)
    bits, output = Bits(src, msb=True), bytearray()
    while len(output) < length:
        reference = bits.read()
        count = _symbol(bits, root)
        if reference:
            count += base
            distance = (_symbol(bits, root) << low_bits) | bits.read(low_bits)
            distance += base
            if count > length - len(output):
                raise AssetError("LenZu reference exceeds declared size")
            _backcopy(output, distance, count)
        else:
            count += 1
            if count > length - len(output):
                raise AssetError("LenZu literal exceeds declared size")
            output.extend(bits.read(8) for _ in range(count))
    crc = 0
    multipliers = (233, 277, 311, 433)
    for index, value in enumerate(output):
        crc = ((crc + value) * multipliers[index & 3]) & 0xffffffffffffffff
    if crc != (high << 32) | low:
        raise AssetError("LenZu checksum mismatch")
    return bytes(output)


def _alpha(value: int) -> int:
    return 255 if value >= 128 else (value << 1) | (value >> 6)


def _palette(data: bytes | memoryview, *, swizzle: bool = False) -> bytes:
    if len(data) % 4:
        raise AssetError("palette must contain RGBA records")
    colors = [bytes((data[i], data[i + 1], data[i + 2], _alpha(data[i + 3])))
              for i in range(0, len(data), 4)]
    if swizzle:
        if len(colors) != 256:
            raise AssetError("swizzled palette is not 256 entries")
        for start in range(0, 256, 32):
            colors[start + 8:start + 24] = (colors[start + 16:start + 24] +
                                          colors[start + 8:start + 16])
    return b"".join(colors)


def _mzp_members(data: bytes | memoryview) -> list[memoryview]:
    src = Reader(data)
    if bytes(src.take(6)) != b"mrgd00":
        raise AssetError("not MZP")
    count = src.uint(2)
    table = src.take(count * 8)
    base = src.pos
    members = []
    for offset in range(0, len(table), 8):
        sector, byte, span, low = struct.unpack_from("<4H", table, offset)
        if byte >= 2048:
            raise AssetError("non-canonical MZP byte offset")
        lower = max(0, (span - 1) * 2048 - byte + 1) if span else 0
        upper = span * 2048 - byte if span else 0
        size = low + max(0, (lower - low + 65535) // 65536) * 65536
        if not lower <= size <= upper:
            raise AssetError("inconsistent MZP sector count/length")
        start = base + sector * 2048 + byte
        if start > len(data) or size > len(data) - start:
            raise AssetError("MZP member exceeds container")
        members.append(src.data[start:start + size])
    return members


def decode_mzp(data: bytes | memoryview):
    """Return (Pillow RGBA image, metadata), retaining the format's dimensions."""
    import numpy as np
    from PIL import Image
    members = _mzp_members(data)
    if not members or len(members[0]) < 16:
        raise AssetError("missing MZP image descriptor")
    w, h, tw, th, nx, ny, kind, depth, border = struct.unpack_from("<7H2B", members[0])
    if min(w, h, tw, th, nx, ny) < 1 or border * 2 >= min(tw, th):
        raise AssetError("invalid MZP geometry")
    if len(members) != 1 + nx * ny:
        raise AssetError("MZP tile count mismatch")
    width, height = w - 2 * border * (nx - 1), h - 2 * border * (ny - 1)
    if min(width, height) < 1:
        raise AssetError("MZP borders consume output")
    _limit(width * height, MAX_PIXELS)
    _limit(tw * th * 4)
    canvas = np.zeros((height, width, 4), dtype=np.uint8)
    palette = None
    if kind == 1:
        if depth not in (1, 0x11, 0x91):
            raise AssetError(f"unimplemented MZP indexed depth {depth:#x}")
        if len(members[0]) < 16 + 1024:
            raise AssetError("truncated MZP palette")
        palette = np.frombuffer(_palette(members[0][16:1040],
                                       swizzle=depth in (0x11, 0x91)),
                                dtype=np.uint8).reshape(256, 4)
    elif (kind, depth) not in ((0x0b, 0x14), (0x0c, 0x11)):
        raise AssetError(f"unimplemented MZP image mode {kind:#x}/{depth:#x}")
    for tile_index in range(nx * ny):
        raw = decode_mzx(members[tile_index + 1])
        pixels = tw * th
        if kind == 1:
            if len(raw) != pixels:
                raise AssetError("indexed tile length mismatch")
            tile = palette[np.frombuffer(raw, dtype=np.uint8)].reshape(th, tw, 4)
        elif kind == 0x0c:
            if len(raw) < 32:
                raise AssetError("truncated HEP tile")
            signature, size, _, _, _, hw, hh, transparency = struct.unpack_from("<8I", raw)
            if (signature != 0x00504548 or hw != tw or hh != th or
                    size != 32 + pixels + 1024 or len(raw) != size):
                raise AssetError("invalid HEP tile geometry/size")
            pal = np.frombuffer(_palette(raw[32 + pixels:]), dtype=np.uint8).reshape(256, 4)
            tile = pal[np.frombuffer(raw, dtype=np.uint8, count=pixels, offset=32)].reshape(th, tw, 4)
        else:
            if len(raw) != pixels * 4:
                raise AssetError("truecolor tile length mismatch")
            packed = np.frombuffer(raw, dtype="<u2", count=pixels)
            fine = np.frombuffer(raw, dtype=np.uint8, count=pixels, offset=pixels * 2)
            tile = np.empty((pixels, 4), dtype=np.uint8)
            tile[:, 0] = ((packed & 0xf800) >> 8) | (fine >> 5)
            tile[:, 1] = ((packed & 0x07e0) >> 3) | ((fine >> 3) & 3)
            tile[:, 2] = ((packed & 0x001f) << 3) | (fine & 7)
            tile[:, 3] = np.frombuffer(raw, dtype=np.uint8, count=pixels, offset=pixels * 3)
            tile = tile.reshape(th, tw, 4)
        x, y = tile_index % nx, tile_index // nx
        sx, sy = (border if x else 0), (border if y else 0)
        dx, dy = x * (tw - 2 * border) + sx, y * (th - 2 * border) + sy
        cw = min(tw - sx - (border if x < nx - 1 else 0), width - dx)
        ch = min(th - sy - (border if y < ny - 1 else 0), height - dy)
        if min(cw, ch) < 0:
            raise AssetError("MZP tile placement exceeds output")
        canvas[dy:dy + ch, dx:dx + cw] = tile[sy:sy + ch, sx:sx + cw]
    return Image.fromarray(canvas), {"width": width, "height": height,
        "storage_width": w, "storage_height": h, "tile_width": tw,
        "tile_height": th, "tile_border": border, "bitmap_type": kind,
        "bitmap_depth": depth}


def decode_cbg(data: bytes | memoryview):
    """Huffman, alternating runs, predictor, BGRA conversion; no resizing."""
    import numpy as np
    from PIL import Image
    src = Reader(data)
    if bytes(src.take(16)) != b"CompressedBG_MT\0":
        raise AssetError("not CBG")
    width, height, stripe, bpp = struct.unpack("<4I", src.take(16))
    if not width or not height or not stripe or bpp not in (8, 24, 32):
        raise AssetError("invalid CBG geometry/depth")
    _limit(width * height, MAX_PIXELS)
    src.take(16)
    count = (height + stripe - 1) // stripe
    offsets = [src.uint() for _ in range(count)] + [len(data)]
    if offsets[0] < src.pos or offsets[-1] > len(data):
        raise AssetError("CBG stripe index exceeds payload")
    channels = bpp // 8
    output = np.empty((height, width, channels), dtype=np.uint8)
    for index in range(count):
        if offsets[index + 1] < offsets[index]:
            raise AssetError("CBG stripe offsets are not ordered")
        part = Reader(src.data[offsets[index]:offsets[index + 1]])
        coded_size = part.uint()
        rows = min(stripe, height - index * stripe)
        expected = rows * width * channels
        _limit(coded_size, expected * 2 + 16)
        weights = [(symbol, part.varint()) for symbol in range(256)]
        root = _tree(weights)
        bits = Bits(part, msb=False)
        runs = Reader(bytes(_symbol(bits, root) for _ in range(coded_size)))
        residual = bytearray()
        zero = False
        while runs.pos < coded_size:
            length = runs.varint()
            if length > expected - len(residual):
                raise AssetError("CBG run exceeds stripe")
            residual.extend(bytes(length) if zero else runs.take(length))
            zero = not zero
        if len(residual) != expected:
            raise AssetError("CBG stripe decoded length mismatch")
        block = np.frombuffer(residual, dtype=np.uint8).reshape(rows, width, channels)
        # Pixels on the same diagonal are independent of one another. This
        # keeps the exact integer predictor while avoiding millions of Python
        # pixel operations when preparing full-resolution background images.
        for diagonal in range(1, width + rows - 1):
            yy = np.arange(max(0, diagonal - width + 1), min(rows, diagonal + 1))
            xx = diagonal - yy
            middle = (yy > 0) & (xx > 0)
            if middle.any():
                my, mx = yy[middle], xx[middle]
                prediction = ((block[my - 1, mx].astype(np.uint16) +
                               block[my, mx - 1].astype(np.uint16)) >> 1)
                block[my, mx] = (block[my, mx].astype(np.uint16) + prediction) & 255
            edge = ~middle
            if edge.any():
                ey, ex = yy[edge], xx[edge]
                py, px = np.maximum(0, ey - 1), np.maximum(0, ex - 1)
                block[ey, ex] = (block[ey, ex].astype(np.uint16) + block[py, px]) & 255
        if channels > 1:
            block[:, :, :3] = block[:, :, 2::-1]
        output[index * stripe:index * stripe + rows] = block
    mode = {1: "L", 3: "RGB", 4: "RGBA"}[channels]
    image = Image.frombytes(mode, (width, height), output.tobytes()).convert("RGBA")
    return image, {"width": width, "height": height,
                   "bits_per_pixel": bpp, "stripe_height": stripe}


def unwrap_audio(data: bytes | memoryview) -> tuple[bytes, dict]:
    if len(data) < 68:
        raise AssetError("truncated HW audio")
    header, payload, unknown, sample_rate, channels = struct.unpack_from("<I4x4I", data)
    if (header != 64 or bytes(data[4:8]) != b"hw  " or
            payload != len(data) - header or bytes(data[64:68]) != b"OggS"):
        raise AssetError("invalid HW/Ogg wrapper")
    return bytes(data[64:]), {"sample_rate": sample_rate, "channels": channels,
        "wrapper_hex": bytes(data[:64]).hex(), "unknown_0c": unknown}


def read_ccit(data: bytes | memoryview) -> dict:
    src = Reader(data)
    count, record_size = src.uint(), src.uint()
    if record_size != 8 or count > 65536:
        raise AssetError("unsupported CCIT character table")
    chars = []
    for _ in range(count):
        key, index = src.uint(), src.uint()
        raw = key.to_bytes(4, "big").lstrip(b"\0")
        try:
            char = raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise AssetError("invalid CCIT UTF-8 key") from exc
        chars.append({"character": char, "key": key, "index": index})
    coordinates = src.uint()
    metrics = []
    if coordinates:
        if coordinates > 65536 or src.uint() != 20:
            raise AssetError("unsupported CCIT coordinate table")
        metrics = [dict(zip(("page", "x", "y", "width", "height"),
                           struct.unpack("<5I", src.take(20)))) for _ in range(coordinates)]
    if src.pos != len(data):
        raise AssetError("unparsed bytes after CCIT tables")
    for char in chars:
        if char["index"] < len(metrics):
            char["metrics"] = metrics[char["index"]]
    return {"schema_version": 1, "character_count": count,
            "coordinate_count": coordinates, "characters": chars,
            "implicit_metrics": None if coordinates else
                "No coordinates in this table; runtime atlas layout must be verified."}
