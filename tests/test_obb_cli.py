import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import obb_cli as cli


class ObbCliTests(unittest.TestCase):
    def test_only_declared_assets_enter_obb_not_raw_composition_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'images/ko').mkdir(parents=True)
            (root/'images/ko/face.mzp.png').write_bytes(b'raw patch')
            final=root/'images/ko/face.mzp.composite.png';final.write_bytes(b'final composed')
            (root/'assets.json').write_text(json.dumps({'assets':{},'overrides':{'ko':{'face.mzp':{
                'path':final.relative_to(root).as_posix(),'sha256':cli.sha(final)}}}}))
            members=cli._members(root)
            self.assertIn('images/ko/face.mzp.composite.png',members)
            self.assertNotIn('images/ko/face.mzp.png',members)

    def test_public_builder_is_avif_only_and_does_not_prompt(self):
        for language,expected in [('en','approximately 5 GB'),('ko','약 5GB')]:
            rows=[]
            self.assertEqual(cli.select_mode(language,lambda prompt:self.fail('Unexpected mode prompt'),rows.append),'avif')
            text='\n'.join(rows);self.assertIn(expected,text);self.assertNotIn('1)',text);self.assertNotIn('4:4:4',text)

    def test_original_mode_is_no_longer_accepted(self):
        with self.assertRaises(SystemExit):cli.main(['--mode','original'])

    def test_guidance_language_and_explicit_mode_reach_pipeline(self):
        for korean in (False,True):
            with tempfile.TemporaryDirectory() as folder:
                root=Path(folder);source=root/'source';source.mkdir()
                if korean:(source/'patch_KR').mkdir()
                edition='korean' if korean else 'vanilla';output=io.StringIO();verified={'edition':edition,'inputs':[],'sourceFingerprint':'fixture'}
                result={'output':'fixture.obb','obbBytes':5_100_000_000,'obbSha256':'a'*64}
                with mock.patch.object(cli,'check_dependencies'),mock.patch.object(cli,'detect_source',return_value=verified),mock.patch.object(cli,'pipeline',return_value=result) as builder,mock.patch.object(cli.shutil,'disk_usage',return_value=type('Space',(),{'free':100*1024**3})()):
                    self.assertEqual(cli.main([str(source),'--mode','avif','--output',str(root/'out')],stream=output),0)
                self.assertEqual(builder.call_args.kwargs['verified'],verified);self.assertEqual(builder.call_args.args[3],'avif')
                self.assertIn('최종 용량: 5.10GB' if korean else 'Final size: 5.10 GB',output.getvalue())

    def test_output_inside_source_is_rejected_without_source_writes(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder);output=io.StringIO()
            self.assertEqual(cli.main([str(source),'--mode','avif','--output',str(source/'out')],stream=output),1)
            self.assertFalse((source/'out').exists())

    def test_progress_is_readable_in_redirected_output(self):
        output=io.StringIO();report=cli.Progress('ko',output)
        report({'stage':'pack','done':10,'total':10,'bytes':100,'totalBytes':100,'seconds':3})
        self.assertIn('OBB 작성',output.getvalue());self.assertIn('100.0%',output.getvalue());self.assertNotIn('\x1b',output.getvalue())

if __name__=='__main__':unittest.main()
