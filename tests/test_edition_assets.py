"""Real bounded conversion/resume on tiny original and Korean HFA fixtures."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import edition_assets as editions
from asset_codecs import AssetError
from hfa import read_index
from prepare_assets import _convert
from test_asset_codecs import cbg


def archive(path, members):
    index = bytearray()
    payload = bytearray()
    for name, data in members:
        index += name.encode().ljust(96, b'\0') + struct.pack('<II', len(payload), len(data)) + bytes(24)
        payload += data
    path.write_bytes(b'HUNEXGGEFA10' + struct.pack('<I', len(members)) + index + payload)


def identity(path):
    result = dict(name=path.name, bytes=path.stat().st_size, sha256=editions.sha(path))
    if path.suffix != '.dll':
        with path.open('rb') as stream:
            entries, meta = read_index(stream, path.stat().st_size)
        result.update(indexSha256=meta['index_sha256'], entries=len(entries))
    return result


class EditionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'; self.source.mkdir()
        self.base = self.root / 'base'; self.base.mkdir()
        self.output = self.root / 'edition'
        script = b'HunexCompiledScriptVer1.00\0' + struct.pack('<IIII', 12, 0, 0, 0x1b)
        members = [('picture.mzp', cbg(1, 1, b'\x14')), ('script_text_en.ctd', b'English\r\n'), ('start.chs', script)]
        archive(self.source / 'data00000.hfa', members)
        records = {name: _convert(raw, name, self.base) for name, raw in members}
        self.manifest = json.dumps({'schema_version': 1, 'assets': records}).encode()
        (self.base / 'assets.json').write_bytes(self.manifest)
        self.patch = self.source / 'patch_KR'; self.patch.mkdir()
        (self.patch / 'mahoyo_kr.dll').write_bytes(b'fixture identity only, never executed')
        archive(self.patch / 'mahoyo_KR1.dat', [('picture.new', cbg(1, 1, b'\x37')), ('script_text_en.new', '한국어\r\n'.encode())])
        archive(self.patch / 'mahoyo_KR2.dat', [('start.chs', script)])
        archive(self.patch / 'mahoyo_KR3.dat', [('FONT_en_001.new', cbg(1, 1, b'\x45'))])
        self.profile = {'archives': [identity(self.source / 'data00000.hfa')],
                        'koreanPatch': {'files': [identity(p) for p in sorted(self.patch.iterdir())]}}

    def tearDown(self):
        self.temp.cleanup()

    def test_korean_substitutions_precede_native_composition_including_primary_only(self):
        import numpy as np
        from PIL import Image
        raw = self.root/'raw'; (raw/'images').mkdir(parents=True)
        (self.output/'images/ko').mkdir(parents=True)
        def image(root, path, pixels):
            Image.fromarray(np.array(pixels, dtype=np.uint8)).save(root/path)
            file = root/path
            with Image.open(file) as img: width,height=img.size
            return dict(kind='image',path=path,width=width,height=height,
                        sha256=editions.sha(file),bytes=file.stat().st_size)
        red=[90,0,0,255];green=[0,70,0,255];partial=[0,0,60,1];zero=[0,0,0,0]
        primary=image(raw,'images/base.png',[[red,red,red],[red,red,red]])
        patch=image(raw,'images/face.png',[[partial,zero]])
        (raw/'assets.json').write_text(json.dumps({'assets':{'base.mzp':primary,'face.mzp':patch}}))
        ko_primary=image(self.output,'images/ko/base.png',[[green,green,green],[green,green,green]])
        recipe=dict(key='face.mzp',alias='face.mzp',primary='base.mzp',rowIndex=7)
        # An untranslated alias still uses the translated primary. Alpha=1 replaces,
        # rather than alpha-blending, and untouched pixels retain the primary.
        rows=editions.compose_korean({'base.mzp':ko_primary},raw,self.output,[recipe])
        self.assertEqual((rows['face.mzp']['width'],rows['face.mzp']['height']),(3,2))
        with Image.open(self.output/rows['face.mzp']['path']) as final:
            self.assertEqual(np.array(final).tolist(),[[partial,green,green],[green,green,green]])
        # A translated patch is substituted too, without modifying either raw input.
        ko_patch=image(self.output,'images/ko/face.png',[[zero,red]])
        original=editions.sha(self.output/ko_patch['path'])
        again=editions.compose_korean({'base.mzp':ko_primary,'face.mzp':ko_patch},raw,self.output,[recipe])
        self.assertEqual(editions.sha(self.output/ko_patch['path']),original)
        with Image.open(self.output/again['face.mzp']['path']) as final:
            self.assertEqual(final.getpixel((0,0)),tuple(green))
            self.assertEqual(final.getpixel((1,0)),tuple(red))

    def prepare(self):
        return editions.prepare_edition(self.source, self.base, self.output, workers=1, profile=self.profile)

    def test_source_profiles_and_defaults(self):
        result = editions.detect_source(self.source, profile=self.profile)
        self.assertEqual((result['edition'], result['fallback']), ('korean', 'ko'))
        self.assertEqual(result['languages'], ['ko', 'ja', 'zh-Hans', 'zh-Hant'])
        for file in self.patch.iterdir(): file.unlink()
        self.patch.rmdir()
        result = editions.detect_source(self.source, profile=self.profile)
        self.assertEqual((result['edition'], result['fallback']), ('vanilla', 'en'))

    def test_incomplete_patch_does_not_become_vanilla(self):
        (self.patch / 'mahoyo_KR2.dat').unlink()
        with self.assertRaises(AssetError): editions.detect_source(self.source, profile=self.profile)

    def test_patch_loader_without_resource_directory_is_not_vanilla(self):
        import shutil
        shutil.rmtree(self.patch);(self.source/'VERSION.dll').write_bytes(b'synthetic loader indicator')
        with self.assertRaisesRegex(AssetError,'resources are missing'):editions.detect_source(self.source,profile=self.profile)

    def test_payload_corruption_is_detected_without_index_change(self):
        p = self.patch / 'mahoyo_KR2.dat'; b = bytearray(p.read_bytes()); b[-1] ^= 1; p.write_bytes(b)
        with self.assertRaisesRegex(AssetError, 'content'): editions.detect_source(self.source, profile=self.profile)

    def test_namespace_keeps_original_and_includes_new_font(self):
        result = self.prepare()
        self.assertEqual(result['overrides'], 4)
        self.assertEqual((self.base / 'assets.json').read_bytes(), self.manifest)
        m = json.loads((self.output / 'assets.json').read_text())
        self.assertEqual(m['assets']['picture.mzp']['path'], 'images/picture.mzp.png')
        self.assertEqual(m['overrides']['ko']['picture.mzp']['path'], 'images/ko/picture.mzp.png')
        self.assertEqual((self.output / 'text/ko/script_text_en.ctd').read_text(encoding='utf-8'), '한국어\n')
        self.assertIn('font_en_001.mzp', m['overrides']['ko'])
        self.assertTrue((self.output / 'meta/language-assets.tsv').read_text().startswith('WOHLANGASSETS1\n'))

    def test_resume_reuses_complete_work_and_repairs_output(self):
        self.prepare()
        self.assertEqual(self.prepare()['reused'], 4)
        (self.output / 'images/ko/picture.mzp.png').write_bytes(b'broken')
        self.assertEqual(self.prepare()['reused'], 3)

    def test_truncated_final_journal_line_can_resume_repeatedly(self):
        self.prepare()
        with (self.output / 'meta/korean-conversion.jsonl').open('ab') as stream: stream.write(b'{"name":"unfinished')
        self.assertEqual(self.prepare()['reused'], 4)
        self.assertEqual(self.prepare()['reused'], 4)

    def test_changed_base_cannot_reuse_different_edition_work(self):
        self.prepare()
        (self.base / 'assets.json').write_bytes(self.manifest + b' ')
        with self.assertRaisesRegex(AssetError, 'inputs changed'): self.prepare()

    def test_source_output_overlap_rejected(self):
        with self.assertRaises(AssetError):
            editions.prepare_edition(self.source, self.base, self.source / 'result', profile=self.profile)

    def test_unsafe_alias_rejected(self):
        archive(self.patch / 'mahoyo_KR3.dat', [('unknown.new', b'anything')])
        with self.assertRaisesRegex(AssetError, 'unknown patch alias'):
            editions.patch_members(self.source, {'picture.mzp'})


if __name__ == '__main__':
    unittest.main()
