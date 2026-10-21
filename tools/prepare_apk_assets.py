"""Generate APK artwork and layout data from the owner's supported installation."""
from pathlib import Path
import argparse,hashlib,subprocess,sys
from prepare_runtime_metadata import native_image
ROOT=Path(__file__).resolve().parents[1]
def prepare(source):
    source=Path(source).resolve(strict=True)
    binary=native_image(source,ROOT/'build/native')
    output=ROOT/'android/app/build/generated'
    icon=ROOT/'android/app/src/main/res/drawable/launcher_icon.png'
    if not icon.is_file():raise ValueError('Project launcher icon is missing')
    from prepare_steam_profile import prepare as steam_profile
    steam_profile(source,output/'dialogueAssets/steam-profile')
    print('Prepared Steam runtime profile; project icon retained.')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);prepare(p.parse_args().source)
