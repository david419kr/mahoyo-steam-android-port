import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from cache_lock import exclusive


class CacheLockTests(unittest.TestCase):
    def test_second_build_rejected_and_normal_exit_releases_lock(self):
        with tempfile.TemporaryDirectory() as folder:
            with exclusive(folder):
                with self.assertRaisesRegex(RuntimeError,'already using'):
                    with exclusive(folder):pass
            with exclusive(folder):pass

    def test_crashed_process_does_not_leave_a_stale_lock(self):
        with tempfile.TemporaryDirectory() as folder:
            code='import sys,os;sys.path.insert(0,sys.argv[1]);from cache_lock import exclusive\nwith exclusive(sys.argv[2]):os._exit(9)'
            process=subprocess.run([sys.executable,'-c',code,str(ROOT/'tools'),folder],creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            self.assertEqual(process.returncode,9)
            with exclusive(folder):pass

if __name__=='__main__':unittest.main()
