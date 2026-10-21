"""Check the actual APK's explicit non-game asset and raster allowlist."""
from pathlib import Path
import sys
import zipfile
from io import BytesIO
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def check(path):
    owned = {
        'assets/ui-text.json': ROOT / 'data/ui-text.json',
        'assets/steam-profile/source-profile.json': ROOT / 'data/steam11-source-profile.json',
        'assets/lzma-notice.txt': ROOT / 'android/app/src/main/assets/lzma-notice.txt',
        'assets/iced-x86-notice.txt': ROOT / 'android/app/src/main/assets/iced-x86-notice.txt',
    }
    dependency_assets = {
        'assets/PublicSuffixDatabase.list',
        'assets/dexopt/baseline.prof',
        'assets/dexopt/baseline.profm',
    }
    with zipfile.ZipFile(path) as apk:
        names = apk.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate APK members')
        assets = {name for name in names if name.startswith('assets/') and not name.endswith('/')}
        unexpected = assets - owned.keys() - dependency_assets
        if unexpected or not owned.keys() <= assets:
            raise ValueError(f'APK asset allowlist mismatch: extra={sorted(unexpected)}, missing={sorted(owned.keys() - assets)}')
        for name, source in owned.items():
            if apk.read(name).replace(b'\r\n', b'\n') != source.read_bytes().replace(b'\r\n', b'\n'):
                raise ValueError(f'Bundled project asset differs: {name}')
        rasters = [name for name in names if name.startswith('res/') and name.lower().endswith(('.png', '.jpg', '.jpeg', '.webp', '.gif', '.bmp', '.ico', '.avif'))]
        if len(rasters) != 1:
            raise ValueError(f'Expected only the custom launcher raster: {rasters}')
        actual = Image.open(BytesIO(apk.read(rasters[0]))).convert('RGBA')
        expected = Image.open(ROOT / 'android/app/src/main/res/drawable/launcher_icon.png').convert('RGBA')
        if actual.size != expected.size or actual.getchannel('A').tobytes() != expected.getchannel('A').tobytes():
            raise ValueError('Custom launcher icon dimensions/alpha differ')
        # AAPT may normalize RGB under transparent pixels; compare visible color contributions.
        for channel in 'RGB':
            a = ImageChops.multiply(actual.getchannel(channel), actual.getchannel('A'))
            b = ImageChops.multiply(expected.getchannel(channel), expected.getchannel('A'))
            if ImageChops.difference(a, b).getbbox():
                raise ValueError('Custom launcher icon visible pixels differ')
        if any(name.lower().endswith(('.hfa', '.mzp', '.cbg', '.ctd', '.chs', '.hw', '.obb', '.exe', '.rgb')) for name in names):
            raise ValueError('Original game/legacy archive member found')
    print(f'APK asset audit passed: {len(assets)} allowed assets; custom icon only; no bundled game payloads.')


if __name__ == '__main__':
    check(Path(sys.argv[1]))
