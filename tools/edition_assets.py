"""Verified Steam 1.1 / Korean 1.2 inputs and a separate, resumable Korean namespace.

Base prepared files are hard-linked read-only; overlays never overwrite base assets.
No Windows game, patch DLL, Android SDK or decompiler is executed by this module.
"""
from __future__ import annotations

import argparse
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, wait
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import time

from asset_codecs import AssetError
from hfa import read_at, read_index
from prepare_assets import Source, _atomic_bytes, _convert, _json_bytes, _video, build_index, canonical_name

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / 'data/steam11-source-profile.json'
VERSION = 1
LANGUAGES = {'vanilla': ['ja', 'en', 'zh-Hans', 'zh-Hant'], 'korean': ['ko', 'ja', 'zh-Hans', 'zh-Hant']}


def infer_edition(source):
    source=Path(source)
    return 'korean' if any((source/name).exists() for name in ('patch_KR','VERSION.dll','mahoyo_kr.dll')) else 'vanilla'


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def detect_source(source: Path, *, profile: dict | None = None, progress=None) -> dict:
    source = Path(source).resolve(strict=True)
    profile = profile if profile is not None else json.loads(PROFILE.read_text(encoding='utf-8'))
    patch = source / 'patch_KR'
    edition = infer_edition(source)
    if edition=='korean' and not patch.is_dir():raise AssetError('Korean patch loader detected but patch_KR resources are missing')
    expected = profile['archives']
    if {p.name for p in source.glob('*.hfa')} != {r['name'] for r in expected}:
        raise AssetError('Unsupported or incomplete Steam 1.1 archive set')
    inputs = [(source, r) for r in expected]
    if edition == 'korean':
        inputs.extend((patch, r) for r in profile['koreanPatch']['files'])
    identities = [];stats=[]
    for i, (directory, row) in enumerate(inputs, 1):
        path = directory / row['name']
        before=path.stat() if path.exists() else None
        if not path.is_file() or path.is_symlink() or path.stat().st_size != row['bytes']:
            raise AssetError('Missing/changed source: ' + str(path))
        if 'indexSha256' in row:
            with path.open('rb') as stream:
                entries, meta = read_index(stream, path.stat().st_size)
            if meta['index_sha256'] != row['indexSha256'] or len(entries) != row['entries']:
                raise AssetError('Unsupported archive index: ' + str(path))
        actual = sha(path)
        if actual != row['sha256']:
            raise AssetError('Unsupported/corrupt source content: ' + str(path))
        after=path.stat()
        if (before.st_size,before.st_mtime_ns,before.st_ctime_ns)!=(after.st_size,after.st_mtime_ns,after.st_ctime_ns):raise AssetError('Source changed while checking: '+str(path))
        identities.append({'name': path.relative_to(source).as_posix(), 'sha256': actual, 'bytes': row['bytes']})
        stats.append({'name':path.relative_to(source).as_posix(),'bytes':after.st_size,'mtimeNs':after.st_mtime_ns,'ctimeNs':after.st_ctime_ns})
        if progress:
            progress({'stage': 'verify', 'done': i, 'total': len(inputs), 'file': path.name})
    fingerprint = hashlib.sha256(json.dumps(identities, sort_keys=True).encode()).hexdigest()
    return {'schema': 1, 'edition': edition, 'gameVersion': '1.1',
            'patchVersion': '1.2' if edition == 'korean' else None,
            'languages': LANGUAGES[edition], 'fallback': 'ko' if edition == 'korean' else 'en',
            'sourceFingerprint': fingerprint, 'inputs': identities,'inputStats':stats}


def patch_members(source: Path, base_names: set[str]) -> list[tuple[Source, str]]:
    result = []
    used = set()
    for archive in sorted((source / 'patch_KR').glob('mahoyo_KR[123].dat')):
        with archive.open('rb') as stream:
            entries, _ = read_index(stream, archive.stat().st_size)
            for i, entry in enumerate(entries):
                name = canonical_name(entry.name)
                if name.endswith('.new'):
                    stem = name[:-4]
                    candidates = [stem + ext for ext in ('.cbg', '.mzp', '.ctd', '.ccit') if stem + ext in base_names]
                    if len(candidates) == 1:
                        logical = candidates[0]
                    elif not candidates and re.fullmatch(r'font_en_(?:italic_|font3_)?\d+', stem):
                        logical = stem + '.mzp'
                    else:
                        raise AssetError('Ambiguous/unknown patch alias: ' + entry.name)
                else:
                    logical = name
                if logical in used:
                    raise AssetError('Duplicate Korean resource: ' + logical)
                used.add(logical)
                result.append((Source(archive, i, entry), logical))
    return result


def _convert_member(job):
    item, name, output = job
    before = item.archive.stat()
    if name.endswith('.mp4'):
        record = _video(item, name, output, namespace='ko')
    else:
        if item.entry.size > 256 * 1024 * 1024:
            raise AssetError('Patch member exceeds decoding budget: ' + name)
        with item.archive.open('rb') as stream:
            raw = read_at(stream, item.entry.offset, item.entry.size, before.st_size)
        record = _convert(raw, name, output, namespace='ko')
        record['source_sha256'] = hashlib.sha256(raw).hexdigest()
    after = item.archive.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise AssetError('Source changed during conversion: ' + str(item.archive))
    path = output / record['path']
    record.update(bytes=path.stat().st_size, sha256=sha(path),
                  source={'archive': item.archive.name, 'index': item.index, 'offset': item.entry.offset, 'size': item.entry.size},
                  converter_version=VERSION)
    return name, record


def _records(path: Path) -> dict:
    records = {}
    if path.exists():
        lines = path.read_bytes().splitlines(keepends=True)
        complete = 0
        for number, line in enumerate(lines):
            try:
                row = json.loads(line)
                name = canonical_name(row['name'])
                record = row['record']
                relative = Path(record['path'])
                if relative.is_absolute() or '..' in relative.parts or '\\' in record['path'] or relative.parts[:2] not in [(f, 'ko') for f in ('images', 'scripts', 'text', 'fonts', 'audio', 'video')]:
                    raise AssetError('Unsafe Korean journal path')
                records[row['name']] = row['record']
                complete += len(line)
            except (ValueError, KeyError, UnicodeDecodeError):
                # A crash may leave only the final journal line incomplete.
                if number != len(lines) - 1:
                    raise AssetError('Corrupt completed patch journal')
                with path.open('r+b') as stream:
                    stream.truncate(complete)
                break
        if complete and path.read_bytes()[-1:] != b'\n':
            with path.open('ab') as stream:
                stream.write(b'\n')
    return records


def compose_korean(records, raw_source, output, recipes, progress=None):
    """Apply Korean file substitutions BEFORE Native primary/patch composition.

    A substituted primary also changes aliases whose patches were not translated.
    Raw conversion outputs stay immutable so a restart never composes twice.
    """
    from bitmap_composition import GEOMETRY_FIELDS, key_replace
    import numpy as np
    from PIL import Image
    raw_source, output = Path(raw_source), Path(output)
    raw = json.loads((raw_source / 'assets.json').read_bytes())['assets']
    original = dict(records)
    affected = [r for r in recipes if r['alias'] and
                (r['key'] in original or r['primary'] in original)]

    def read(name):
        row = original.get(name, raw.get(name))
        root = output if name in original else raw_source
        if not row or row.get('kind') != 'image' or 'nativeComposite' in row:
            raise AssetError('Korean composition needs raw image: ' + name)
        payload = (root / row['path']).read_bytes()
        if hashlib.sha256(payload).hexdigest() != row['sha256']:
            raise AssetError('Korean composition source changed: ' + name)
        with Image.open(io.BytesIO(payload)) as image:
            if image.size != (row['width'], row['height']):
                raise AssetError('Korean composition geometry changed: ' + name)
            return np.array(image.convert('RGBA')), row

    for done, recipe in enumerate(affected, 1):
        name = recipe['key']
        primary, parent = read(recipe['primary'])
        patch, child = read(recipe['alias'])
        pixels = key_replace(primary, patch)
        row = dict(child)
        for field in GEOMETRY_FIELDS:
            row.pop(field, None)
            if field in parent: row[field] = parent[field]
        relative = f'images/ko/{name}.composite.png'
        provenance = {'rowIndex': recipe['rowIndex'], 'primary': recipe['primary'],
                      'patch': recipe['alias'], 'primaryInputSHA256': parent['sha256'],
                      'patchInputSHA256': child['sha256'], 'operation': 'native-alpha-key-replace',
                      'outputPixelSHA256': hashlib.sha256(pixels.tobytes()).hexdigest()}
        buffer = io.BytesIO()
        Image.fromarray(pixels).save(buffer, 'PNG', compress_level=3, optimize=False)
        payload = buffer.getvalue()
        row.update(path=relative, bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest(),
                   nativeComposite=provenance)
        destination = output / relative
        if not destination.is_file() or sha(destination) != row['sha256']:
            _atomic_bytes(destination, payload)
        records[name] = row
        if progress: progress({'stage': 'compose-ko', 'done': done, 'total': len(affected), 'file': name})
    return records


def prepare_edition(source: Path, prepared: Path, output: Path, *, workers=2, progress=None, profile=None, verified_source=None, raw_source=None, recipes=None) -> dict:
    source, prepared, output = Path(source).resolve(), Path(prepared).resolve(), Path(output).resolve()
    if (output == prepared or output.is_relative_to(prepared) or prepared.is_relative_to(output)
            or output == source or output.is_relative_to(source) or source.is_relative_to(output)):
        raise AssetError('Edition output must be separate from source and base prepared trees')
    if not 1 <= workers <= 4:
        raise AssetError('Use 1..4 bounded decoding workers')
    # The integrated CLI has already verified every input hash before conversion.
    edition = verified_source if verified_source is not None else detect_source(source, profile=profile, progress=progress)
    if edition.get('edition') not in LANGUAGES or not edition.get('sourceFingerprint'):raise AssetError('Invalid verified source profile')
    base_bytes = (prepared / 'assets.json').read_bytes()
    manifest = json.loads(base_bytes)
    identity = {'version': VERSION, 'edition': edition['edition'],
                'sourceFingerprint': edition['sourceFingerprint'],
                'baseManifestSha256': hashlib.sha256(base_bytes).hexdigest()}
    marker = output / 'meta/edition-source.json'
    if marker.exists():
        if json.loads(marker.read_text(encoding='utf-8')) != identity:
            raise AssetError('Edition inputs changed; choose a separate output directory')
    elif output.exists() and any(output.iterdir()):
        raise AssetError('Output contains unowned files')
    output.mkdir(parents=True, exist_ok=True)
    _atomic_bytes(marker, _json_bytes(identity))
    # Link only asset folders, never a mutable assets.json or the input installation.
    for folder in ('images', 'audio', 'video', 'scripts', 'text', 'fonts', 'meta'):
        root = prepared / folder
        if not root.exists():
            continue
        for directory, dirs, files in os.walk(root, followlinks=False):
            (output/Path(directory).relative_to(prepared)).mkdir(parents=True,exist_ok=True)
            for child in dirs + files:
                p = Path(directory) / child
                if p.is_symlink() or p.is_junction():
                    raise AssetError('Base resource links are unsupported: ' + str(p))
            for name in files:
                src = Path(directory) / name
                relative = src.relative_to(prepared)
                dest = output / relative
                if dest.exists():
                    if not os.path.samefile(src, dest) and sha(src) != sha(dest):
                        raise AssetError('Base edition resource changed: ' + str(relative))
                    continue
                try:
                    os.link(src, dest)
                except OSError:
                    shutil.copyfile(src, dest)
    records = {}
    journal = output / 'meta/korean-conversion.jsonl'
    reused = 0
    started = time.monotonic()
    if edition['edition'] == 'korean':
        base_index, _ = build_index(source)
        members = patch_members(source, set(base_index))
        cached = _records(journal)
        jobs = []
        for item, name in members:
            row = cached.get(name)
            if row and row.get('converter_version') == VERSION:
                p = output / row['path']
                if p.is_file() and sha(p) == row['sha256']:
                    extra = row.get('metadata_path')
                    if not extra or (output / extra).is_file():
                        records[name] = row
                        reused += 1
                        continue
            jobs.append((item, name, output))
        pool = ProcessPoolExecutor(max_workers=workers)
        pending = set()
        queue = iter(jobs)
        with journal.open('a', encoding='utf-8') as log:
            try:
                for _ in range(workers * 2):
                    job = next(queue, None)
                    if job is not None:
                        pending.add(pool.submit(_convert_member, job))
                while pending:
                    done, pending = wait(pending, return_when=FIRST_COMPLETED)
                    for future in done:
                        name, row = future.result()
                        records[name] = row
                        log.write(json.dumps({'name': name, 'record': row}, separators=(',', ':')) + '\n')
                        log.flush()
                        if progress:
                            progress({'stage': 'convert-ko', 'done': len(records), 'total': len(members),
                                      'reused': reused, 'file': name, 'seconds': time.monotonic() - started})
                        job = next(queue, None)
                        if job is not None:
                            pending.add(pool.submit(_convert_member, job))
            finally:
                for future in pending:
                    future.cancel()
                pool.shutdown(wait=True, cancel_futures=True)
                log.flush()
        if manifest.get('nativeBitmapCandidate'):
            if raw_source is None or recipes is None:
                raise AssetError('Korean edition requires raw Native composition inputs')
            from bitmap_composition import load_recipes
            table, _ = load_recipes(Path(recipes))
            records = compose_korean(records, raw_source, output, table['records'], progress)
        manifest['overrides'] = {'ko': records}
    manifest['edition'] = edition
    _atomic_bytes(output / 'assets.json', _json_bytes(manifest))
    _atomic_bytes(output / 'meta/edition.json', _json_bytes(edition))
    _atomic_bytes(output / 'meta/game-edition.txt', ('WOHEDITION1\n' + edition['edition'] + '\n').encode())
    lines = ['WOHLANGASSETS1']
    dimensions = ['WOH_IMAGE_DIMENSIONS1']
    changed = []
    for name, row in sorted(records.items()):
        width, height = row.get('width', 0), row.get('height', 0)
        lines.append('\t'.join(map(str, ('ko', name, row['path'], width, height))))
        if row['kind'] == 'image':
            dimensions.append(f'{name} {width} {height}')
            old = manifest['assets'].get(name)
            if old and (old.get('width'), old.get('height')) != (width, height):
                changed.append({'name': name, 'old': [old.get('width'), old.get('height')], 'new': [width, height]})
    _atomic_bytes(output / 'meta/language-assets.tsv', ('\n'.join(lines) + '\n').encode())
    if len(dimensions) > 1:
        _atomic_bytes(output / 'meta/ko-image-dimensions.tsv', ('\n'.join(dimensions) + '\n').encode())
    _atomic_bytes(output / 'meta/korean-dimension-changes.json', _json_bytes(changed))
    return {'edition': edition['edition'], 'overrides': len(records), 'reused': reused,
            'dimensionChanges': len(changed), 'seconds': time.monotonic() - started,
            'manifest': str(output / 'assets.json')}


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--prepared', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--raw-prepared', type=Path)
    parser.add_argument('--recipes', type=Path)
    args = parser.parse_args()
    progress = lambda row: print(json.dumps(row, ensure_ascii=False), flush=True)
    if args.prepared and args.output:
        result = prepare_edition(args.source, args.prepared, args.output, workers=args.workers, progress=progress,
                                 raw_source=args.raw_prepared, recipes=args.recipes)
    elif not args.prepared and not args.output:
        result = detect_source(args.source, progress=progress)
    else:
        parser.error('--prepared and --output must be supplied together')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
