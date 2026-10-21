"""Run the real dependency bootstrap against a tiny local Git origin, without network."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SteamDependencyPatchTest(unittest.TestCase):
    def test_bootstrap_is_repeatable_and_preserves_unrecognized_edits(self):
        shell = shutil.which('pwsh')
        if not shell or not shutil.which('git'):
            self.skipTest('PowerShell and Git required')
        patch = json.loads((ROOT/'dependencies/steam.json').read_text())['androidDownloaderPatch']
        replacements = patch['replacements']
        original = '\n'.join(row['before'] for row in replacements)+'\n'
        changed = original
        for row in replacements:
            changed = changed.replace(row['before'], row['after'])
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            origin, seed, workspace = base/'origin with spaces.git', base/'seed', base/'builder workspace'
            def git(*args, cwd=None):
                return subprocess.check_output(['git', *map(str, args)], cwd=cwd, text=True, stderr=subprocess.STDOUT).strip()
            git('init', '--bare', origin)
            git('init', '-b', 'main', seed)
            source = seed/patch['path'];source.parent.mkdir(parents=True);source.write_text(original, encoding='utf-8', newline='\n')
            (seed/'README').write_text('fixture\n')
            git('add', '.', cwd=seed)
            git('-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','-m','fixture',cwd=seed)
            commit = git('rev-parse','HEAD',cwd=seed)
            git('push',origin,'HEAD:refs/heads/main',cwd=seed)
            (workspace/'scripts').mkdir(parents=True)
            (workspace/'dependencies').mkdir()
            shutil.copyfile(ROOT/'scripts/prepare-steam.ps1',workspace/'scripts/prepare-steam.ps1')
            profile = {'repository':str(origin), 'commit':commit, 'androidDownloaderPatch':{
                'path':patch['path'], 'baseSha256':hashlib.sha256(original.encode()).hexdigest(),
                'patchedSha256':hashlib.sha256(changed.encode()).hexdigest(), 'replacements':replacements}}
            manifest = workspace/'dependencies/steam.json';manifest.write_text(json.dumps(profile),encoding='utf-8')
            def run():
                return subprocess.run([shell,'-NoProfile','-NonInteractive','-File',str(workspace/'scripts/prepare-steam.ps1')],capture_output=True,text=True)
            first = run();self.assertEqual(first.returncode,0,first.stdout+first.stderr)
            target = workspace/'.deps/JavaSteam'/patch['path']
            self.assertEqual(target.read_text(),changed)
            self.assertEqual(git('rev-parse','HEAD',cwd=workspace/'.deps/JavaSteam'),commit)
            repeat = run();self.assertEqual(repeat.returncode,0,repeat.stdout+repeat.stderr)
            target.write_text(changed+'// user edit\n',encoding='utf-8')
            failed = run();self.assertNotEqual(failed.returncode,0)
            self.assertEqual(target.read_text(),changed+'// user edit\n')
            target.write_text(changed,encoding='utf-8')
            unrelated = workspace/'.deps/JavaSteam/README';unrelated.write_text('user content\n')
            self.assertNotEqual(run().returncode,0)
            self.assertEqual(unrelated.read_text(),'user content\n')


if __name__ == '__main__':
    unittest.main()
