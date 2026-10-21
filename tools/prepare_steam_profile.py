"""Build a version-pinned, small runtime profile; retain original media in HFA files."""
from pathlib import Path
import hashlib,json,shutil
from prepare_assets import build_index,_convert,_atomic_bytes
from prepare_runtime_metadata import generate,native_image,TARGET
from hfa import read_at
ROOT=Path(__file__).resolve().parents[1]
def prepare(source,output):
 source,output=Path(source).resolve(),Path(output).resolve();work=ROOT/'build/steam-profile';raw=work/'inputs';raw.mkdir(parents=True,exist_ok=True)
 index,_=build_index(source);assets={name:{'path':'images/'+name+'.png'} for name in index if name.endswith('.mzp')}
 names=['archive.chs']+[f'{prefix}{side:03}.cbg' for side in (20,64,80,128,256,512) for prefix in ('alphagradation','alphagradation_inv')]
 for name in names:
  item=index[name]
  with item.archive.open('rb') as stream:payload=read_at(stream,item.entry.offset,item.entry.size,item.archive.stat().st_size)
  row=_convert(payload,name,raw);row['sha256']=hashlib.sha256((raw/row['path']).read_bytes()).hexdigest();assets[name]=row
 _atomic_bytes(raw/'assets.json',json.dumps({'assets':assets},sort_keys=True).encode())
 binary=native_image(source,ROOT/'build/native');metadata=work/'meta';receipts=work/'receipts';generate(binary,raw,metadata,receipts)
 output.mkdir(parents=True,exist_ok=True)
 for path in metadata.iterdir():
  if path.is_file():shutil.copyfile(path,output/path.name)
 table=json.loads((receipts/'native-bitmap-records.json').read_bytes())
 _atomic_bytes(output/'bitmap-recipes.json',json.dumps({'schema':1,'target':TARGET,'records':[{k:r[k] for k in ('key','primary','alias')} for r in table['records']]},separators=(',',':')).encode())
 shutil.copyfile(ROOT/'data/steam11-source-profile.json',output/'source-profile.json')
 # Translations are data from the supplied executable; no game program is loaded.
 from prepare_language_metadata import prepare as messages
 messages(source,output/'language-messages.tsv',False)
 files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.iterdir()) if p.is_file() and p.name!='profile.json'}
 _atomic_bytes(output/'profile.json',json.dumps({'schema':1,'appId':2052410,'buildId':13060638,'nativeSha256':TARGET,'steamExeSha256':hashlib.sha256((source/'WoH.exe').read_bytes()).hexdigest(),'files':files},sort_keys=True).encode())
 print('Steam original-resource profile:',len(files),'files')
