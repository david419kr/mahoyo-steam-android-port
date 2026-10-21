"""Actual tiny HFA conversion, checkpoint resume and native composition checks."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT/'tests'))
import cli_assets
from test_edition_assets import archive
from test_asset_codecs import cbg


class CliAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.source=self.root/'source';self.source.mkdir();self.output=self.root/'raw';self.state=self.root/'state'
        archive(self.source/'data.hfa',[(f'image{i}.cbg',cbg(1,1,bytes((20+i,)))) for i in range(6)])
    def tearDown(self):self.temp.cleanup()
    def prepare(self,**extra):return cli_assets.prepare_all(self.source,self.output,self.state,'synthetic',workers=1,**extra)

    def test_real_hfa_convert_then_reuse(self):
        result=self.prepare();self.assertEqual(result['assets'],6);self.assertEqual(result['reused'],0)
        self.assertEqual(self.prepare()['reused'],6)
        from PIL import Image
        with Image.open(self.output/'images/image0.cbg.png') as image:self.assertGreater(image.width,0)

    def test_interrupt_and_torn_journal_resume_completed_members(self):
        def interrupt(row):
            if row['done']==2:raise InterruptedError('controlled checkpoint')
        with self.assertRaises(InterruptedError):self.prepare(progress=interrupt)
        journal=self.state/'converted.jsonl'
        with journal.open('ab') as stream:stream.write(b'{"name":"torn')
        result=self.prepare();self.assertEqual(result['assets'],6);self.assertGreaterEqual(result['reused'],2)

    def test_only_corrupt_output_is_regenerated(self):
        self.prepare();path=self.output/'images/image3.cbg.png';good=path.read_bytes();path.write_bytes(b'corrupt')
        self.assertEqual(self.prepare()['reused'],5);self.assertEqual(path.read_bytes(),good)

    def test_changed_profile_and_source_overlap_are_rejected(self):
        self.prepare()
        with self.assertRaises(ValueError):cli_assets.prepare_all(self.source,self.output,self.state,'different',workers=1)
        with self.assertRaises(ValueError):cli_assets.prepare_all(self.source,self.source/'output',self.state,'synthetic',workers=1)

    def test_native_composition_is_exact_and_resumable(self):
        from PIL import Image
        source=self.root/'bitmaps';(source/'images').mkdir(parents=True);manifest={'schema_version':1,'assets':{}}
        for name,pixels in [('base.mzp',[(1,2,3,0),(10,20,30,255)]),('patch.mzp',[(4,5,6,128),(99,99,99,0)])]:
            path=source/'images'/(name+'.png');image=Image.new('RGBA',(2,1));image.putdata(pixels);image.save(path)
            manifest['assets'][name]={'kind':'image','path':'images/'+name+'.png','width':2,'height':1,'bytes':path.stat().st_size,'sha256':cli_assets.sha(path)}
        (source/'assets.json').write_text(json.dumps(manifest));before=(source/'images/patch.mzp.png').read_bytes()
        recipe=self.root/'recipes.json';recipe.write_text('{}');table={'targetSHA256':'t'*64,'records':[{'rowIndex':1,'key':'patch.mzp','primary':'base.mzp','alias':'patch.mzp'}]}
        output=self.root/'composed';state=self.root/'composition-state'
        with mock.patch('bitmap_composition.load_recipes',return_value=(table,'a'*64)):
            self.assertEqual(cli_assets.compose_all(source,output,state,recipe,workers=1)['pairs'],1)
            self.assertEqual(cli_assets.compose_all(source,output,state,recipe,workers=1)['reused'],1)
        with Image.open(output/'images/patch.mzp.png') as image:self.assertEqual(image.convert('RGBA').tobytes(),bytes((4,5,6,128,10,20,30,255)))
        self.assertEqual((source/'images/patch.mzp.png').read_bytes(),before)
        self.assertTrue(json.loads((output/'assets.json').read_bytes())['nativeBitmapCandidate']['stageReady'])

if __name__=='__main__':unittest.main()
