"""Write reviewed HW loop metadata from a prepared assets.json, without decoding audio."""


import argparse


import json


from pathlib import Path


import struct


def header_fields(record):
    header = bytes.fromhex(record['wrapper_hex'])
    if len(header) != 64 or header[4:8] != b'hw  ' or struct.unpack_from('<I', header)[0] != 64:
        raise ValueError('Invalid64byte native HW wrapper')
    payload, count, rate, channels, flag, start, end = struct.unpack_from('<7I', header, 8)
    if payload != record['bytes'] or not count or not 0 < rate <= 768000 or not 0 < channels <= 8:
        raise ValueError('Invalid HW sample/payload metadata')
    return count, rate, channels, flag, start, end


def generate(game):
    manifest = json.loads((game / 'assets.json').read_text(encoding='utf-8'))
    if manifest.get('schema_version') != 1:
        raise ValueError('Expected prepared schema1 assets.json')
    rows = ['#name sampleCount sampleRate channels wrapperLoop loopStart loopEnd']
    count = partial = 0
    for name, record in sorted(manifest['assets'].items()):
        if record.get('kind') != 'audio':
            continue
        if not name.endswith('.hw') or any(c.isspace() for c in name) or any(c in name for c in '/\\:'):
            raise ValueError('Invalid canonical HW name')
        fields = header_fields(record)
        rows.append(name + '\t' + '\t'.join(str(n) for n in fields))
        count += 1
        partial += bool(fields[-2] or fields[-1])
    target = game / 'meta/audio-loop-map.tsv'
    target.parent.mkdir(parents=True, exist_ok=True)
    pending = target.with_suffix('.tsv.tmp')
    pending.write_text('\n'.join(rows) + '\n', encoding='utf-8')
    pending.replace(target)
    return {'audioRecords': count, 'nonzeroLoopBounds': partial, 'output': str(target)}


def self_test():
    raw = bytearray(64)
    struct.pack_into('<I4s7I', raw, 0, 64, b'hw  ', 99, 480000, 48000, 2, 0, 0, 0)
    record = {'wrapper_hex': raw.hex(), 'bytes': 99}
    assert header_fields(record) == (480000, 48000, 2, 0, 0, 0)
    struct.pack_into('<III', raw, 24, 1, 1200, 2400)
    assert header_fields(dict(record, wrapper_hex=raw.hex())) == (480000, 48000, 2, 1, 1200, 2400)
    try:
        header_fields(dict(record, bytes=100))
    except ValueError:
        pass
    else:
        raise AssertionError('Mismatched wrapper accepted')
    print('HW loop metadata self-test passed')


