"""Pack supplied bytes into a seekable ZIP64 OBB; never modify prepared input data."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, stat, time, zipfile

FOLDERS = {'audio', 'fonts', 'images', 'meta', 'scripts', 'text', 'video'}

def safe_name(name):
    return bool(name) and not any(c in name for c in '\\\t\r\n\0:') and not name.startswith('/') and all(p not in ('', '.', '..') for p in name.split('/'))

def engine_file(name):
    return name == 'assets.json' or name.split('/', 1)[0] in {'scripts', 'text', 'meta', 'fonts'}

def build(source, output, overlays=(), variant=None):
    source, output = Path(source).resolve(), Path(output).resolve()
    if output.is_relative_to(source) or output.exists() or output.with_suffix('.obb.partial').exists():
        raise ValueError('Output must be new and outside the input directory')
    manifest = source / 'assets.json'
    if not manifest.is_file(): raise ValueError('Prepared assets.json is required')
    paths = []
    for base, dirs, files in os.walk(source, followlinks=False):
        for name in dirs + files:
            path = Path(base) / name
            if path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0):
                raise ValueError('Source links/reparse points are not supported: ' + str(path))
        for name in files:
            path = Path(base) / name; relative = path.relative_to(source).as_posix()
            if not safe_name(relative) or (relative != 'assets.json' and ('/' not in relative or relative.split('/', 1)[0] not in FOLDERS)):
                raise ValueError('Unexpected asset path: ' + relative)
            paths.append((relative, path))
    # Overlays replace named members only; unmodified media is streamed from source.
    originals=dict(paths); replacements={}
    for overlay in overlays:
        overlay=Path(overlay).resolve()
        if overlay==source or output.is_relative_to(overlay):raise ValueError('Overlay/output must be separate from source')
        for base, dirs, files in os.walk(overlay,followlinks=False):
            for name in dirs+files:
                path=Path(base)/name
                if path.is_symlink() or getattr(path.lstat(),'st_file_attributes',0)&getattr(stat,'FILE_ATTRIBUTE_REPARSE_POINT',0):raise ValueError('Overlay links are not supported')
            for name in files:
                path=Path(base)/name; relative=path.relative_to(overlay).as_posix()
                if (relative not in originals and relative!='meta/image-dimensions.tsv') or not safe_name(relative):raise ValueError('Unknown overlay member: '+relative)
                replacements[relative]=path
    paths=sorted((name,replacements.get(name,path)) for name,path in paths)
    paths.extend((name,path) for name,path in replacements.items() if name not in originals);paths.sort()
    minimum_apk = 55 if replacements else 53
    for name, path in paths:
        if name.startswith('images/'):
            with path.open('rb') as image: header = image.read(32)
            if header[4:8] == b'ftyp' and any(header[i:i+4] == b'avif' for i in (8,16,20,24,28)):
                minimum_apk = 58
                break
    manifest=replacements.get('assets.json',manifest)
    variant=variant or ('optimized-overlay' if replacements else 'original-resources')
    if not variant or len(variant)>128:raise ValueError('Variant name must contain 1..128 characters')
    if not paths or len(paths) > 100000: raise ValueError('Invalid asset count')
    output.parent.mkdir(parents=True, exist_ok=True)
    partial = output.with_suffix('.obb.partial'); rows = ['WOHOBB1\n']; total = engine_bytes = 0
    started = time.monotonic()
    try:
        with partial.open('xb') as raw, zipfile.ZipFile(raw, 'w', compression=zipfile.ZIP_STORED, allowZip64=True) as archive:
            for relative, path in paths:
                before = path.stat(); info = zipfile.ZipInfo(relative); info.compress_type = zipfile.ZIP_STORED
                info.external_attr = (stat.S_IFREG | 0o444) << 16
                with path.open('rb') as original, archive.open(info, 'w', force_zip64=True) as entry:
                    offset = raw.tell(); shutil.copyfileobj(original, entry, 1024 * 1024)
                after = path.stat()
                if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or info.file_size != before.st_size:
                    raise ValueError('Input changed while packing: ' + relative)
                rows.append(f'{relative}\t{offset}\t{info.file_size}\t{info.CRC:08x}\n')
                total += info.file_size
                if engine_file(relative): engine_bytes += info.file_size
            index = ''.join(rows).encode('utf8')
            if len(index) > 16 * 1024 * 1024 or engine_bytes > 256 * 1024 * 1024:
                raise ValueError('Runtime metadata/index budget exceeded')
            info = {'format': 'WOHOBB1', 'dataVersion': 1, 'entries': len(paths), 'bytes': total,
                    'engineBytes': engine_bytes, 'indexSha256': hashlib.sha256(index).hexdigest(),
                    'manifestSha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
                    'sourceManifestSha256':hashlib.sha256((source/'assets.json').read_bytes()).hexdigest(),
                    'minimumApkVersionCode':minimum_apk,'variant':variant,'overlayMembers':len(replacements)}
            archive.writestr('_obb/index.tsv', index)
            archive.writestr('_obb/info.json', json.dumps(info, sort_keys=True).encode('utf8'))
        with zipfile.ZipFile(partial) as check:
            if len(check.infolist()) != len(paths) + 2 or any(e.compress_type != zipfile.ZIP_STORED for e in check.infolist()):
                raise ValueError('Unexpected OBB directory')
            if check.read('_obb/index.tsv') != index: raise ValueError('Index readback mismatch')
        partial.replace(output)
    except BaseException:
        if partial.exists(): partial.unlink()
        raise
    digest = hashlib.sha256()
    with output.open('rb') as file:
        for block in iter(lambda: file.read(4 * 1024 * 1024), b''): digest.update(block)
    return {**info, 'output': str(output), 'obbBytes': output.stat().st_size, 'obbSha256': digest.hexdigest(),
            'compression': 'container ZIP_STORED/ZIP64; supplied member bytes unchanged by packer', 'seconds': time.monotonic() - started}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('source', type=Path); parser.add_argument('output', type=Path)
    parser.add_argument('--overlay',type=Path,action='append',default=[])
    parser.add_argument('--variant')
    args = parser.parse_args(); receipt = build(args.source, args.output,args.overlay,args.variant)
    args.output.with_suffix('.obb.build.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf8')
    print(json.dumps(receipt, indent=2))
