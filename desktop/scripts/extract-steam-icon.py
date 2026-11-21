"""Read the primary icon from a user-supplied Windows EXE without executing it."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import struct
import sys
import zlib


def extract(executable):
    api = C.WinDLL('kernel32', use_last_error=True)
    api.LoadLibraryExW.argtypes = [W.LPCWSTR, C.c_void_p, W.DWORD]
    api.LoadLibraryExW.restype = C.c_void_p
    api.FreeLibrary.argtypes = [C.c_void_p]
    callback_type = C.WINFUNCTYPE(W.BOOL, C.c_void_p, C.c_void_p, C.c_void_p, C.c_ssize_t)
    api.EnumResourceNamesW.argtypes = [C.c_void_p, C.c_void_p, callback_type, C.c_ssize_t]
    api.FindResourceW.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    api.FindResourceW.restype = C.c_void_p
    api.SizeofResource.argtypes = [C.c_void_p, C.c_void_p]
    api.SizeofResource.restype = W.DWORD
    api.LoadResource.argtypes = [C.c_void_p, C.c_void_p]
    api.LoadResource.restype = C.c_void_p
    api.LockResource.argtypes = [C.c_void_p]
    api.LockResource.restype = C.c_void_p
    module = api.LoadLibraryExW(str(Path(executable).resolve()), None, 0x02 | 0x20)
    if not module:
        raise C.WinError(C.get_last_error())
    try:
        names = []
        @callback_type
        def collect(handle, kind, name, param):
            names.append(name if name <= 65535 else C.wstring_at(name))
            return True
        if not api.EnumResourceNamesW(module, C.c_void_p(14), collect, 0) or not names:
            raise OSError('The executable has no icon group')
        def read(kind, name):
            resource_name = C.c_void_p(name) if isinstance(name, int) else C.cast(C.c_wchar_p(name), C.c_void_p)
            resource = api.FindResourceW(module, resource_name, C.c_void_p(kind))
            if not resource:
                raise C.WinError(C.get_last_error())
            size = api.SizeofResource(module, resource)
            pointer = api.LockResource(api.LoadResource(module, resource))
            if not pointer or not 0 < size <= 16 * 1024 * 1024:
                raise OSError('Invalid icon resource')
            return C.string_at(pointer, size)
        group = read(14, names[0])
        reserved, kind, count = struct.unpack_from('<HHH', group)
        if reserved or kind != 1 or not 1 <= count <= 256 or len(group) != 6 + 14 * count:
            raise ValueError('Invalid icon group directory')
        frames = []
        directory = bytearray(struct.pack('<HHH', 0, 1, count))
        offset = 6 + 16 * count
        for i in range(count):
            width, height, colors, zero, planes, bits, length, identifier = struct.unpack_from('<BBBBHHIH', group, 6 + 14 * i)
            data = read(3, identifier)
            if len(data) != length:
                raise ValueError('Icon image length mismatch')
            directory.extend(struct.pack('<BBBBHHII', width, height, colors, zero, planes, bits, length, offset))
            offset += length
            frames.append((width or 256, height or 256, bits, data))
        return bytes(directory) + b''.join(frame[3] for frame in frames), frames
    finally:
        api.FreeLibrary(module)


def png(frame):
    width, height, bits, data = frame
    if data.startswith(b'\x89PNG\r\n\x1a\n'):
        return data
    header, dib_width, dib_height, planes, depth, compression = struct.unpack_from('<IiiHHI', data)
    if header < 40 or dib_width != width or abs(dib_height) != height * 2 or planes != 1 or depth != 24 or compression:
        raise ValueError('Unsupported Steam icon bitmap format')
    palette = struct.unpack_from('<I', data, 32)[0] * 4
    start = header + palette
    stride = (width * 3 + 3) & ~3
    mask_start = start + stride * height
    mask_stride = ((width + 31) // 32) * 4
    if mask_start + mask_stride * height > len(data):
        raise ValueError('Truncated icon transparency mask')
    rows = bytearray()
    for y in range(height):
        row = height - 1 - y if dib_height > 0 else y
        rows.append(0)
        for x in range(width):
            at = start + row * stride + x * 3
            blue, green, red = data[at:at + 3]
            alpha = 0 if data[mask_start + row * mask_stride + x // 8] & (0x80 >> (x % 8)) else 255
            rows.extend((red, green, blue, alpha))
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')


if __name__ == '__main__':
    executable, output = Path(sys.argv[1]), Path(sys.argv[2])
    if executable.resolve() == output.resolve():
        raise ValueError('The output must not overwrite the source executable')
    ico, frames = extract(executable)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(ico)
    for width, height, bits, data in frames:
        output.with_name(f'launcher-{width}.png').write_bytes(png((width, height, bits, data)))
    print('Extracted original icon sizes:', ', '.join(f'{w}x{h}' for w,h,_,_ in frames))
