"""Generate APK artwork and layout data from the owner's supported installation."""
from pathlib import Path
import argparse,hashlib,subprocess,sys
import pefile
from pe_resources import resource
from prepare_runtime_metadata import native_image
from prepare_exact_layouts import extract, SHA
ROOT=Path(__file__).resolve().parents[1]
def prepare(source):
    source=Path(source).resolve(strict=True)
    binary=native_image(source,ROOT/'build/native')
    output=ROOT/'android/app/build/generated'
    dialogue=output/'dialogueAssets/dialogue'
    subprocess.run([sys.executable,'-B',str(ROOT/'tools/prepare_dialogue_art.py'),'--source',str(source),'--output',str(dialogue)],check=True)
    rows=extract(binary)
    (dialogue/'exact-layouts.tsv').write_bytes((f'WOHEXACTLAYOUTS1 {SHA} {len(rows)}\n'+''.join(k+' '+' '.join(map(str,v))+'\n' for k,v in sorted(rows.items()))).encode('ascii'))
    with pefile.PE(str(binary)) as pe:
        channels=[pe.get_data(0x3B4040+(9+i)*256,256) for i in range(3)]
    lut=bytes(v for i in range(256) for v in (channels[0][i],channels[1][i],channels[2][i]))
    if hashlib.sha256(lut).hexdigest()!='7a793f9900385ff43c366a1631dca3b955b19ba0a1ae02b5eca46fd6ea5b8293':raise ValueError('Color table mismatch')
    palette=output/'sourceAssets/color-mode3.rgb';palette.parent.mkdir(parents=True,exist_ok=True);palette.write_bytes(lut)
    icon=resource(source/'WoH.exe',3,1)
    if hashlib.sha256(icon).hexdigest()!='2927a56db178640ca0afd60c88b08dc2691ec18ba2f3e4dd15e233d780a2588e':raise ValueError('Unsupported launcher icon')
    path=ROOT/'android/app/src/main/res/drawable/launcher_icon.png';path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(icon)
    print('Prepared owned-source artwork, cursor, layouts and color table.')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);prepare(p.parse_args().source)
