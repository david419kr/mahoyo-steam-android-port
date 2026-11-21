"""Compile the shared Android game code against the Windows platform adapters."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
shared = ROOT / 'build/windows/shared/org/mahoyo/port'
shared.mkdir(parents=True, exist_ok=True)
for source in (ROOT / 'android/app/src/main/java/org/mahoyo/port').glob('*.java'):
    code = source.read_text(encoding='utf-8')
    # Android has one Buffer overload; desktop OpenGL has several typed overloads.
    code = re.sub(r'(GLES30\.glTexImage2D\([^;]*?,)null(\))', r'\1(java.nio.ByteBuffer)null\2', code)
    # The desktop JSON library makes JSONException unchecked; Android's API does not.
    code = code.replace('JSONException|RuntimeException', 'RuntimeException')
    (shared / source.name).write_text(code, encoding='utf-8')
assets = ROOT / 'build/windows/assets'
(assets/'steam-profile').mkdir(parents=True, exist_ok=True)
(assets/'steam-profile/source-profile.json').write_bytes((ROOT/'data/steam11-source-profile.json').read_bytes())
(assets/'ui-text.json').write_bytes((ROOT/'data/ui-text.json').read_bytes())
classes = ROOT / 'build/windows/classes'
classes.mkdir(parents=True, exist_ok=True)
for old in classes.rglob('*.class'):
    if not old.resolve().is_relative_to((ROOT / 'build/windows').resolve()):
        raise RuntimeError('Compiled class outside the Windows build directory')
    old.unlink()
java = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
args = [str(java/'bin/javac.exe'), '-encoding', 'UTF-8', '-Xmaxerrs', '200',
        '-cp', str(ROOT/'.deps/windows/lib/*'), '-sourcepath',
        str(ROOT/'desktop/src')+';'+str(ROOT/'build/windows/shared'),
        '-d', str(classes), str(ROOT/'desktop/src/org/mahoyo/port/DesktopMain.java'),
        str(ROOT/'desktop/src/org/mahoyo/port/SaveSyncMain.java')]
with (ROOT/'.local/windows/javac.log').open('w', encoding='utf-8') as log:
    result = subprocess.run(args, cwd=ROOT, stdout=log, stderr=log)
if result.returncode:
    lines = (ROOT/'.local/windows/javac.log').read_text(encoding='utf-8').splitlines()
    for i, line in enumerate(lines):
        if 'error:' in line or 'symbol:' in line or 'location:' in line or 'both method' in line:
            print(line)
else:
    print('Windows Java compilation passed')
raise SystemExit(result.returncode)
