"""Rebuild Steam 1.1 runtime metadata from owned files, without starting the game."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
from prepare_assets import _atomic_bytes, _json_bytes

TARGET='1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b'
STEAM='7a718cfca3fccafd9d7984e41206473a6152098d6e0f7374c52abf1581b6ad9a'
VERSION=3


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def native_image(source,work,unpacker=None):
    source=Path(source).resolve();work=Path(work).resolve()
    if work==source or work.is_relative_to(source):raise ValueError('Native work must be outside the source installation')
    original=source/'WoH.exe';identity=sha(original)
    if identity not in (TARGET,STEAM):raise ValueError('Unsupported Steam 1.1 executable identity')
    target=work/'WoH.exe.unpacked.exe'
    if target.is_file() and sha(target)==TARGET:return target
    work.mkdir(parents=True,exist_ok=True)
    if identity==TARGET:_atomic_bytes(target,original.read_bytes())
    else:
        unpacker=Path(unpacker or ROOT/'.deps/steamless/Steamless.CLI.exe').resolve(strict=True)
        copied=work/'WoH.exe';_atomic_bytes(copied,original.read_bytes())
        with (work/'unpack.log').open('w',encoding='utf-8') as log:
            subprocess.run([str(unpacker),'--quiet','--keepbind',str(copied)],check=True,stdout=log,stderr=subprocess.STDOUT,
                           creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    if not target.is_file() or sha(target)!=TARGET:raise ValueError('Decoded executable differs from the verified metadata profile')
    if sha(original)!=identity:raise ValueError('Source executable changed during preparation')
    return target


def _wik(pe):
    header='# WIKCHOICELAYOUT1 canvas=1920x1080 units=parent-canvas-pixels sourceUnits=source-pixels initialFocus=-1 monoState=0 colorState=1'
    rows=[struct.unpack('<5i',pe.get_data(0x2846a0+i*20,20)) for i in range(14)]
    if sorted(r[4] for r in rows)!=list(range(14)):raise ValueError('WIK priorities differ')
    lines=[header]
    for i,(x,y,w,h,priority) in enumerate(rows):
        if not(-1920<=x<1920 and -1080<=y<1080 and 0<w<=4096 and 0<h<=4096):raise ValueError('WIK source geometry differs')
        lines.append('\t'.join(map(str,(i,f'wik_sel{i:02}_mono.mzp',f'wik_sel{i:02}.mzp',x,y,w,h,priority,1,1,0,0,w,h))))
    return ('\n'.join(lines)+'\n').encode()


def generate(binary,prepared,output,provenance,progress=None):
    """Metadata only; output/provenance are separate from the immutable prepared tree."""
    binary,prepared,output,provenance=map(lambda p:Path(p).resolve(),(binary,prepared,output,provenance))
    if output==prepared or output.is_relative_to(prepared) or provenance.is_relative_to(prepared):raise ValueError('Metadata output must be separate')
    if sha(binary)!=TARGET:raise ValueError('Native metadata profile mismatch')
    manifest_bytes=(prepared/'assets.json').read_bytes();manifest=json.loads(manifest_bytes)
    identity={'version':VERSION,'nativeSha256':TARGET,'manifestSha256':hashlib.sha256(manifest_bytes).hexdigest()}
    receipt=provenance/'metadata.json'
    if receipt.is_file():
        previous=json.loads(receipt.read_bytes())
        if previous.get('identity')==identity and all((output/name).is_file() and sha(output/name)==digest for name,digest in previous['files'].items()):return previous
    output.mkdir(parents=True,exist_ok=True);provenance.mkdir(parents=True,exist_ok=True)
    stages=['effects','layouts','catalogues','palettes','windows']
    def report(stage):
        if progress:progress({'stage':'metadata','file':stage,'done':stages.index(stage),'total':len(stages)})
    import pefile
    from effect_metadata import extract as effects
    from image_layout import extract as layouts
    from music_metadata import extract as music
    from mask_metadata import extract as masks
    from image_resources import extract as inventory
    from bitmap_recipes import extract as recipes
    from archive_catalog import extract as archive, serialize
    from extra_catalog import extract as extras, write as write_extras
    from color_metadata import extract as palettes
    from prepare_exact_layouts import extract as exact_layouts
    report('effects')
    # Match code75's shipped catalogue. 5714's field26 graphics context is still unreviewed;
    # including it makes loadProperties reject the entire table before the title can start.
    effects(binary,output/'effects.tsv',[i for i in range(1,8373) if i!=5714],provenance/'effects.json',output/'effects-window.tsv',output/'effects-properties.tsv')
    report('layouts')
    layouts(binary,prepared/'assets.json',output/'image-layout.tsv',provenance/'layouts.json',output/'image-origins.tsv')
    inventory(binary,output/'native-image-resources.tsv',provenance/'inventory.json')
    recipe=recipes(binary);_atomic_bytes(provenance/'native-bitmap-records.json',(json.dumps(recipe,sort_keys=True,separators=(',',':'))+'\n').encode('ascii'))
    migration=json.loads((ROOT/'data/steam11-save-migration-profile.json').read_bytes())
    if migration['nativeSha256']!=TARGET or migration['tableSha256']!=recipe['tableSHA256']:raise ValueError('Legacy save migration profile differs')
    selected=bytes.fromhex(migration['selectedRowsLsbBitsetHex'])
    legacy={r['key'][:-4] for r in recipe['records'] if selected[r['rowIndex']//8]&(1<<(r['rowIndex']%8))}
    if len(legacy)!=migration['legacyLayoutRows']:raise ValueError('Legacy save layout membership differs')
    # WH3..21 migration must use the original shipped sparse map; the complete,
    # exact map below is for current drawing and must not alter those old bounds.
    for name in ('image-layout.tsv','image-origins.tsv'):
        rows=(output/name).read_text(encoding='utf-8').splitlines()
        _atomic_bytes(output/name,('\n'.join(row for row in rows if row.split()[0] in legacy)+'\n').encode())
    exact=exact_layouts(binary)
    _atomic_bytes(output/'native-exact-layouts.tsv',(f'WOHEXACTLAYOUTS1 {TARGET} {len(exact)}\n'+''.join(f'{name} {w} {h} {x} {y}\n' for name,(w,h,x,y) in sorted(exact.items()))).encode())
    report('catalogues')
    _atomic_bytes(output/'archive-catalog.tsv',serialize(archive(binary,prepared/'scripts/archive.chs')))
    write_extras(*extras(binary),provenance/'extra')
    for name in ('gallery-catalog.tsv','music-catalog.tsv','checkpoint-map.tsv'):_atomic_bytes(output/name,(provenance/'extra'/name).read_bytes())
    music(binary,output/'music-map.tsv',provenance/'music.json');masks(binary,output/'mask-map.tsv',provenance/'masks.json')
    with pefile.PE(str(binary)) as pe:_atomic_bytes(output/'wik-choice-layout.tsv',_wik(pe))
    report('palettes');palettes(binary,provenance/'color-mode3.rgb',provenance/'palettes.json',output)
    report('windows')
    for side in (20,64,80,128,256,512):
        for kind,prefix in (('normal','alphagradation'),('inv','alphagradation_inv')):
            record=manifest['assets'][f'{prefix}{side:03}.cbg']
            path=(prepared/record['path']).resolve()
            if not path.is_relative_to(prepared) or sha(path)!=record['sha256']:raise ValueError('Window atlas source changed')
            _atomic_bytes(output/f'window-atlas-{kind}-{side:03}.png',path.read_bytes())
    files={p.name:sha(p) for p in sorted(output.iterdir()) if p.is_file()}
    result={'identity':identity,'files':files,'gameExecuted':False,'patchDllExecuted':False}
    _atomic_bytes(receipt,_json_bytes(result))
    if progress:progress({'stage':'metadata','done':len(stages),'total':len(stages),'file':'complete'})
    return result
