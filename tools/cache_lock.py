"""OS-owned build lock: released automatically after a crash; no stale PID files."""
from contextlib import contextmanager
import os
from pathlib import Path


@contextmanager
def exclusive(folder):
    folder=Path(folder);folder.mkdir(parents=True,exist_ok=True)
    with (folder/'.obb-builder.lock').open('a+b') as handle:
        handle.seek(0,2)
        if handle.tell()==0:handle.write(b'\0');handle.flush()
        handle.seek(0)
        try:
            if os.name=='nt':
                import msvcrt
                msvcrt.locking(handle.fileno(),msvcrt.LK_NBLCK,1)
            else:
                import fcntl
                fcntl.flock(handle.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        except OSError as error:raise RuntimeError('Another OBB build is already using this folder: '+str(folder)) from error
        try:yield
        finally:
            handle.seek(0)
            if os.name=='nt':msvcrt.locking(handle.fileno(),msvcrt.LK_UNLCK,1)
            else:fcntl.flock(handle.fileno(),fcntl.LOCK_UN)
