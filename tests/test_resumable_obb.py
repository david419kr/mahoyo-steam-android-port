import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import resumable_obb as packer


class ResumableObbTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.source=self.root/'source';self.source.mkdir()
        (self.source/'assets.json').write_text('{"schema_version":1,"assets":{}}')
        (self.source/'images').mkdir();(self.source/'images/a.png').write_bytes(bytes(range(256))*100)
        (self.source/'images/b.png').write_bytes(b'last image'*1000)
        self.output=self.root/'main.1.org.mahoyo.port.obb'

    def tearDown(self):self.temp.cleanup()

    def build(self,**kwargs):
        return packer.build(packer.directory_members(self.source),self.output,edition='vanilla',variant='avif',source_fingerprint='synthetic',**kwargs)

    def test_normal_and_completed_output_reuse(self):
        receipt=self.build();self.assertEqual(receipt['entries'],3)
        with zipfile.ZipFile(self.output) as z:
            self.assertEqual(z.read('images/a.png'),(self.source/'images/a.png').read_bytes())
            self.assertEqual(z.testzip(),None)
            info=json.loads(z.read('_obb/info.json'));self.assertEqual(info['edition'],'vanilla')
        self.assertEqual(self.build()['obbSha256'],receipt['obbSha256'])

    def test_member_checkpoint_resume_has_no_duplicate_entries(self):
        with self.assertRaises(InterruptedError):self.build(interrupt_after=2)
        self.assertFalse(self.output.exists())
        receipt=self.build();self.assertEqual(receipt['resumedMembers'],2)
        with zipfile.ZipFile(self.output) as z:
            self.assertEqual(len(z.infolist()),5);self.assertIsNone(z.testzip())

    def test_corrupt_completed_payload_is_rebuilt_from_first_bad_member(self):
        with self.assertRaises(InterruptedError):self.build(interrupt_after=2)
        journal=self.output.with_suffix('.obb.members.jsonl');row=json.loads(journal.read_text().splitlines()[1])
        partial=self.output.with_suffix('.obb.partial')
        with partial.open('r+b') as f:f.seek(row['data']+2);f.write(b'corrupt')
        result=self.build();self.assertEqual(result['resumedMembers'],1)
        with zipfile.ZipFile(self.output) as z:self.assertIsNone(z.testzip())

    def test_torn_final_journal_record_keeps_prior_members(self):
        with self.assertRaises(InterruptedError):self.build(interrupt_after=2)
        journal=self.output.with_suffix('.obb.members.jsonl');rows=journal.read_bytes().splitlines(keepends=True)
        journal.write_bytes(rows[0]+rows[1][:12])
        result=self.build();self.assertEqual(result['resumedMembers'],1)

    def test_changed_inputs_do_not_mix_with_existing_checkpoint(self):
        with self.assertRaises(InterruptedError):self.build(interrupt_after=1)
        (self.source/'images/a.png').write_bytes(b'different')
        with self.assertRaisesRegex(ValueError,'inputs/profile changed'):self.build()

    def test_existing_encoded_obb_can_supply_common_members(self):
        self.build();second=self.root/'korean.obb'
        with zipfile.ZipFile(self.output) as z:
            members=packer.zip_members(z)
            extra=self.root/'translated';extra.write_bytes(b'Korean image')
            members['images/ko/a.png']=packer.FileMember(extra)
            translated=self.root/'ctd';translated.write_bytes(b'Korean text')
            members['text/ko/script_text_en.ctd']=packer.FileMember(translated)
            manifest=self.root/'korean-manifest.json';manifest.write_text(json.dumps({'edition':{'edition':'korean'},'assets':{},'overrides':{'ko':{'script_text_en.ctd':{'path':'text/ko/script_text_en.ctd'}}}}))
            members['assets.json']=packer.FileMember(manifest)
            result=packer.build(members,second,edition='korean',variant='avif',source_fingerprint='synthetic-korean')
        self.assertEqual(result['entries'],5)
        with zipfile.ZipFile(second) as z:
            self.assertEqual(z.read('images/ko/a.png'),b'Korean image');self.assertIsNone(z.testzip())

    def test_wrong_resume_profile_rejected(self):
        with self.assertRaises(InterruptedError):self.build(interrupt_after=1)
        with self.assertRaises(ValueError):
            packer.build(packer.directory_members(self.source),self.output,edition='vanilla',variant='original',source_fingerprint='synthetic')

    def test_content_identity_survives_metadata_file_rewrite(self):
        import hashlib
        def members():return {name:packer.FileMember(member.path,hashlib.sha256(member.path.read_bytes()).hexdigest()) for name,member in packer.directory_members(self.source).items()}
        with self.assertRaises(InterruptedError):packer.build(members(),self.output,edition='vanilla',variant='avif',source_fingerprint='synthetic',interrupt_after=2)
        manifest=self.source/'assets.json';manifest.write_bytes(manifest.read_bytes())
        result=packer.build(members(),self.output,edition='vanilla',variant='avif',source_fingerprint='synthetic')
        self.assertEqual(result['resumedMembers'],2);self.assertTrue(result['validation']['crcChecked'])

    def test_claimed_content_hash_is_checked_while_writing(self):
        members=packer.directory_members(self.source);members['images/a.png']=packer.FileMember(self.source/'images/a.png','0'*64)
        with self.assertRaisesRegex(ValueError,'content checksum'):packer.build(members,self.output,edition='vanilla',variant='avif',source_fingerprint='synthetic')
        self.assertFalse(self.output.exists())

    def test_real_process_kill_resumes_without_a_central_directory(self):
        import subprocess
        code='''import sys,json
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from resumable_obb import build,directory_members
def progress(row):
 if row['stage']=='pack' and row['done']==2:
  print('CHECKPOINT',flush=True);sys.stdin.read(1)
build(directory_members(Path(sys.argv[2])),Path(sys.argv[3]),edition='vanilla',variant='avif',source_fingerprint='synthetic',progress=progress)
'''
        process=subprocess.Popen([sys.executable,'-c',code,str(Path(__file__).resolve().parents[1]/'tools'),str(self.source),str(self.output)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(),'CHECKPOINT')
            process.kill();process.wait(timeout=10)
            result=self.build();self.assertEqual(result['resumedMembers'],2);self.assertTrue(result['validation']['crcChecked'])
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=10)
            process.stdin.close();process.stdout.close();process.stderr.close()

    def test_manifest_cannot_claim_wrong_edition_or_missing_assets(self):
        with self.assertRaisesRegex(ValueError,'edition mismatch'):
            packer.build(packer.directory_members(self.source),self.output,edition='korean',variant='avif',source_fingerprint='synthetic')
        (self.source/'assets.json').write_text(json.dumps({'assets':{'a':{'path':'images/missing.png'}}}))
        with self.assertRaisesRegex(ValueError,'declared asset'):self.build()


if __name__=='__main__':unittest.main()
