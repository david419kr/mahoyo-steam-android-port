"""Resumable compact overlays for common art and Korean additions."""
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from pathlib import Path
import argparse
import hashlib
import io
import json
import math
import os
import time

from PIL import Image, ImageChops, features
from prepare_assets import _atomic_bytes, _json_bytes

VERSION = 1


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load_journal(path):
    result = {}
    if not path.exists():
        return result
    lines = path.read_bytes().splitlines(keepends=True)
    complete = 0
    for index, line in enumerate(lines):
        try:
            row = json.loads(line)
            result[row['path']] = row
            complete += len(line)
        except (ValueError, KeyError, UnicodeDecodeError):
            if index != len(lines) - 1:
                raise ValueError('Corrupt completed compression journal')
            with path.open('r+b') as stream:
                stream.truncate(complete)
            break
    if complete and path.read_bytes()[-1:] != b'\n':
        with path.open('ab') as stream:
            stream.write(b'\n')
    return result


def psnr(original, decoded):
    import numpy as np
    a, b = np.asarray(original), np.asarray(decoded)
    if a.shape != b.shape or not np.array_equal(a[:, :, 3], b[:, :, 3]):
        raise ValueError('AVIF changed dimensions/alpha')
    total = 0.
    count = 0
    for y in range(0, original.height, 128):
        before, after = a[y:y+128].astype(np.float32), b[y:y+128].astype(np.float32)
        error = (before[:, :, :3] - after[:, :, :3]) * (before[:, :, 3:4] / 255.)
        total += float(np.sum(error * error, dtype=np.float64));count += error.size
    return 999. if total == 0 else 10 * math.log10(255**2 / (total / count))


def encode(source, output, name, record, protected, previous=None, namespace='ko'):
    relative = record['path']
    prefix='images/'+(namespace+'/' if namespace else '')
    if namespace not in ('','ko') or not relative.startswith(prefix) or relative.count('/')!=(2 if namespace else 1) or '\\' in relative or ':' in relative or any(p in ('', '.', '..') for p in relative.split('/')):
        raise ValueError('Unsafe image path')
    path = source / relative
    original = path.read_bytes()
    digest = sha(original)
    if record.get('sha256') and digest != record['sha256']:
        raise ValueError('Prepared image differs from its manifest: ' + relative)
    destination = output / 'selected' / relative
    if not destination.resolve().is_relative_to((output / 'selected').resolve()):
        raise ValueError('Selected image escapes output tree')
    is_protected = name in protected or record.get('source', {}).get('archive') == 'mahoyo_KR1.dat'
    if previous and previous['sourceSha256'] == digest and previous.get('protected', False) == is_protected:
        if not previous['replaced']:
            if destination.exists():destination.unlink()
            return previous
        if destination.is_file() and sha(destination.read_bytes()) == previous['outputSha256']:
            return previous
    started = time.monotonic()
    with Image.open(io.BytesIO(original)) as image:
        image.load();metadata = {k: image.info[k] for k in ('icc_profile', 'exif', 'xmp') if image.info.get(k)}
        image = image.convert('RGBA')
    pixels = image.tobytes()
    best, encoding = original, 'PNG'
    if max(image.size) <= 16383:
        buffer = io.BytesIO()
        image.save(buffer, 'WEBP', lossless=True, quality=75, method=4, exact=True, **metadata)
        candidate = buffer.getvalue()
        with Image.open(io.BytesIO(candidate)) as decoded:
            if decoded.size != image.size or decoded.convert('RGBA').tobytes() != pixels:
                raise ValueError('Lossless RGBA mismatch: ' + relative)
        if len(candidate) < len(best):
            best, encoding = candidate, 'WebP lossless'
    eligible = (not is_protected and name.endswith('.mzp') and not name.startswith(('font', 'rule', 'noise', 'black', 'white', 'fallnote', 's_'))
                and not metadata and min(image.size) >= 256 and image.width*image.height >= 262144 and max(image.size) <= 16383)
    red, green, blue, _ = image.split()
    if eligible and ImageChops.difference(red, green).getbbox() is None and ImageChops.difference(red, blue).getbbox() is None:
        eligible = False
    trials = []
    chosen_quality, chosen_psnr = None, None
    if eligible:
        for quality in (85, 90, 95, 100):
            buffer = io.BytesIO()
            image.save(buffer, 'AVIF', quality=quality, subsampling='4:4:4', speed=6, max_threads=1,
                       advanced={'alpha:lossless': '1'}, icc_profile=b'', exif=b'', xmp=b'')
            candidate = buffer.getvalue()
            with Image.open(io.BytesIO(candidate)) as decoded:
                metric = psnr(image, decoded.convert('RGBA'))
            trials.append({'quality': quality, 'bytes': len(candidate), 'psnr': metric})
            if metric >= 40:
                if len(candidate) < len(best):
                    best, encoding, chosen_quality, chosen_psnr = candidate, 'AVIF', quality, metric
                break
    replaced = best != original
    if replaced:
        _atomic_bytes(destination, best)
    elif destination.exists():
        destination.unlink()
    return {'path': relative, 'sourceSha256': digest, 'sourceBytes': len(original), 'size': list(image.size),
            'rgbaSha256': sha(pixels), 'replaced': replaced, 'encoding': encoding, 'outputBytes': len(best),
            'outputSha256': sha(best), 'protected': is_protected, 'alphaExact': True,
            'quality': chosen_quality, 'psnr': chosen_psnr, 'trials': trials, 'seconds': time.monotonic()-started}


def optimize(source, output, *, workers=2, progress=None, namespace='ko'):
    source, output = Path(source).resolve(), Path(output).resolve()
    if source == output or source.is_relative_to(output) or output.is_relative_to(source):
        raise ValueError('Source and optimization output must be separate')
    if namespace not in ('','ko') or not 1 <= workers <= (16 if not namespace else 4):
        raise ValueError('Use bounded encoder workers and a known namespace')
    manifest_bytes = (source / 'assets.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    rows = manifest.get('overrides', {}).get('ko', {}) if namespace else manifest['assets']
    images = {name: row for name, row in rows.items() if row['kind'] == 'image'}
    protected = {name for name, row in images.items() if name in manifest['assets'] and
                 (row['width'], row['height']) != (manifest['assets'][name].get('width'), manifest['assets'][name].get('height'))}
    if not namespace:
        from optimize_avif import catalogue
        eligible,_=catalogue(source)
        protected.update(name for name,row in images.items() if row['path'] not in eligible)
    identity = {'version': VERSION, 'manifestSha256': sha(manifest_bytes), 'webPVersion': features.version('webp'),
                'avifVersion': features.version('avif'), 'qualitySteps': [85, 90, 95, 100],
                'alphaExact': True, 'minimumAlphaWeightedPsnr': 40, 'protectedDimensionChanges': sorted(protected)}
    identity['namespace']=namespace
    identity_path = output / 'identity.json'
    if identity_path.exists():
        previous=json.loads(identity_path.read_text());previous.setdefault('namespace','ko')
        # Every source/output is still checked by SHA. A new manifest or location alone
        # must not discard otherwise identical encoded assets shared by both editions.
        if any(previous.get(k)!=v for k,v in identity.items() if k not in ('manifestSha256','protectedDimensionChanges')):
            raise ValueError('Compression settings changed; choose a separate output')
    output.mkdir(parents=True, exist_ok=True)
    _atomic_bytes(identity_path, _json_bytes(identity))
    journal = output / 'images.jsonl';old = load_journal(journal);completed = {};jobs = iter(sorted(images));pending = set()
    pool = ThreadPoolExecutor(max_workers=workers)
    started = time.monotonic()
    def submit():
        name = next(jobs, None)
        if name is not None:
            pending.add(pool.submit(encode, source, output, name, images[name], protected, old.get(images[name]['path']),namespace))
    with journal.open('a', encoding='utf-8') as log:
        try:
            for _ in range(workers*2): submit()
            while pending:
                done, rest = wait(pending, return_when=FIRST_COMPLETED);pending = rest
                for future in done:
                    row = future.result();completed[row['path']] = row
                    log.write(json.dumps(row, separators=(',', ':'))+'\n');log.flush()
                    if progress:progress({'stage': 'compress-ko' if namespace else 'compress', 'done': len(completed), 'total': len(images),
                                          'file': row['path'], 'encoding': row['encoding'], 'seconds': time.monotonic()-started})
                    submit()
        finally:
            for future in pending: future.cancel()
            pool.shutdown(wait=True, cancel_futures=True)
    # A corrected Native recipe can replace a raw patch path with a composite path.
    # Remove only stale files recorded in our own journal, within this selection tree.
    selected = (output / 'selected').resolve()
    for relative in old.keys() - completed.keys():
        stale = (selected / relative).resolve()
        if not stale.is_relative_to(selected): raise ValueError('Unsafe stale encoded path')
        if stale.is_file(): stale.unlink()
    summary = {**identity, 'images': len(completed), 'sourceBytes': sum(r['sourceBytes'] for r in completed.values()),
               'outputBytes': sum(r['outputBytes'] for r in completed.values()),
               'encodings': {kind: sum(r['encoding'] == kind for r in completed.values()) for kind in ('PNG', 'WebP lossless', 'AVIF')},
               'seconds': time.monotonic()-started, 'overlay': str(output / 'selected')}
    _atomic_bytes(output / 'summary.json', _json_bytes(summary))
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__);parser.add_argument('source', type=Path);parser.add_argument('output', type=Path);parser.add_argument('--workers', type=int, default=2)
    args = parser.parse_args();print(json.dumps(optimize(args.source, args.output, workers=args.workers,
        progress=lambda row: print(json.dumps(row), flush=True)), indent=2))
