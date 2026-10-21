"""AVIF 4:4:4 art overlay using the native art catalogue, exact alpha and >=40dB.

UI/masks/LUTs remain in the separately verified lossless overlay. Candidates
are selected only if smaller than that lossless baseline. Original data is read
only, and the selected overlay uses hard links to avoid duplicating candidates.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse,hashlib,io,json,math,os,re,sys,time

def sha(data):return hashlib.sha256(data).hexdigest()
def atomic(path,data):
    path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_suffix(path.suffix+'.partial');tmp.write_bytes(data);tmp.replace(path)
def load_records(path):
    rows={}
    if path.exists():
        for line in path.read_text(encoding='utf8').splitlines():
            row=json.loads(line);rows[row['path']]=row
    return rows

def catalogue(source):
    native=source/'meta/native-image-resources.tsv';gallery=source/'meta/gallery-catalog.tsv';mask=source/'meta/mask-map.tsv'
    native_text=native.read_text();gallery_text=gallery.read_text()
    if not native_text.startswith('WOH_NATIVE_IMAGES1 ') or not gallery_text.startswith('WOHGALLERY1 '):raise ValueError('Expected verified native art catalogues')
    names=set(native_text.splitlines()[1:])|set(re.findall(r'^V \d+ \d+ "([^"]+)"',gallery_text,re.M))
    excluded={line.split()[1] for line in mask.read_text().splitlines() if line and not line.startswith('#')}
    names={n for n in names if n.endswith('.mzp') and n not in excluded and re.fullmatch(r'[a-z0-9_]+\.mzp',n) and not n.startswith(('font','rule','noise','black','white','fallnote','s_'))}
    return {f'images/{n}.png' for n in names},{p.name:sha(p.read_bytes()) for p in (native,gallery,mask)}

def psnr_rgba(a,b):
    import numpy as np
    original=np.asarray(a);decoded=np.asarray(b)
    if original.shape!=decoded.shape or not np.array_equal(original[:,:,3],decoded[:,:,3]):raise ValueError('AVIF alpha/dimensions changed')
    squared=0.;count=0
    # Chunked float arrays bound peak memory for unusually large CG atlases.
    for y in range(0,a.height,128):
        x=original[y:y+128].astype(np.float32);z=decoded[y:y+128].astype(np.float32)
        error=(x[:,:,:3]-z[:,:,:3])*(x[:,:,3:4]/255.)
        squared+=float(np.sum(error*error,dtype=np.float64));count+=error.size
    return 999. if squared==0 else 10*math.log10(255**2/(squared/count))

def encode(source,cache,relative,old=None):
    from PIL import Image,ImageChops
    path=source/relative;fingerprint=path.stat();target=cache/relative
    if old and old['sourceBytes']==fingerprint.st_size and old['sourceMtimeNs']==fingerprint.st_mtime_ns and (not old.get('candidate') or target.is_file() and sha(target.read_bytes())==old['candidateSha256']):return old
    original=path.read_bytes();row={'path':relative,'sourceBytes':len(original),'sourceMtimeNs':fingerprint.st_mtime_ns,'sourceSha256':sha(original),'candidate':False};started=time.monotonic()
    with Image.open(io.BytesIO(original)) as decoded:
        if decoded.info.get('icc_profile') or decoded.info.get('exif') or decoded.info.get('xmp'):return {**row,'reason':'Color/profile metadata retained losslessly'}
        image=decoded.convert('RGBA')
    if min(image.size)<256 or image.width*image.height<262144 or max(image.size)>16383:return {**row,'reason':'Small or oversized native bitmap retained losslessly'}
    r,g,b,_=image.split()
    if ImageChops.difference(r,g).getbbox() is None and ImageChops.difference(r,b).getbbox() is None:return {**row,'reason':'Grayscale/mask-like bitmap retained losslessly'}
    trials=[]
    for quality in (85,90,95,100):
        stream=io.BytesIO();image.save(stream,'AVIF',quality=quality,subsampling='4:4:4',speed=6,max_threads=1,advanced={'alpha:lossless':'1'},icc_profile=b'',exif=b'',xmp=b'')
        data=stream.getvalue()
        with Image.open(io.BytesIO(data)) as check:converted=check.convert('RGBA')
        psnr=psnr_rgba(image,converted);trials.append({'quality':quality,'bytes':len(data),'psnr':psnr})
        if psnr>=40:
            atomic(target,data);row.update(candidate=True,candidateSha256=sha(data),candidateBytes=len(data),quality=quality,psnr=psnr,alphaExact=True,size=list(image.size),decodedRgbaSha256=sha(converted.tobytes()));break
    after=path.stat()
    if (fingerprint.st_size,fingerprint.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):raise ValueError('Source changed: '+relative)
    return {**row,'trials':trials,'seconds':time.monotonic()-started}

def optimize(source,output,baseline_records,vendor,workers=4):
    source,output,vendor=Path(source).resolve(),Path(output).resolve(),Path(vendor).resolve()
    if source==output or output.is_relative_to(source) or source.is_relative_to(output):raise ValueError('Source/output trees must be separate')
    if not 1<=workers<=16:raise ValueError('Use 1..16 single-threaded encoders')
    sys.path.insert(0,str(vendor));import pillow_avif
    from pillow_avif import AvifImagePlugin
    AvifImagePlugin.DEFAULT_MAX_THREADS=1
    eligible,provenance=catalogue(source);manifest=json.loads((source/'assets.json').read_text(encoding='utf8'))
    actual={v['path'] for v in manifest['assets'].values() if v.get('kind')=='image'};paths=sorted(eligible&actual)
    identity={'version':1,'manifestSha256':sha((source/'assets.json').read_bytes()),'catalogues':provenance,'pluginVersion':pillow_avif.__version__,'codecVersions':AvifImagePlugin._avif.codec_versions(),'settings':{'qualitySteps':[85,90,95,100],'subsampling':'4:4:4','speed':6,'max_threads':1,'decodeThreads':1,'alpha:lossless':'1','minimumAlphaWeightedPsnr':40}}
    output.mkdir(parents=True,exist_ok=True);ident=output/'identity.json'
    if ident.exists() and json.loads(ident.read_text())!=identity:raise ValueError('AVIF identity changed; choose new output')
    atomic(ident,(json.dumps(identity,indent=2)+'\n').encode());cache=output/'candidates';cache.mkdir(exist_ok=True);journal=output/'images.jsonl';old=load_records(journal);rows=[];started=last=time.monotonic()
    with journal.open('a',encoding='utf8') as log,ThreadPoolExecutor(max_workers=workers) as pool:
        for i,row in enumerate(pool.map(lambda n:encode(source,cache,n,old.get(n)),paths),1):
            rows.append(row);log.write(json.dumps(row,separators=(',',':'))+'\n');log.flush()
            if time.monotonic()-last>=15 or i==len(paths):log.flush();print(json.dumps({'done':i,'total':len(paths),'sourceBytes':sum(x['sourceBytes'] for x in rows),'candidateBytes':sum(x.get('candidateBytes',0) for x in rows),'seconds':time.monotonic()-started}),flush=True);last=time.monotonic()
    baseline=load_records(Path(baseline_records));missing=set(paths)-baseline.keys()
    if missing:raise ValueError(f'Lossless baseline still incomplete for {len(missing)} art images; rerun to select verified candidates')
    selected=output/'selected';selected.mkdir(exist_ok=True);chosen=[]
    for row in rows:
        if row.get('candidate') and row['candidateBytes']<baseline[row['path']]['outputBytes']:
            target=selected/row['path'];target.parent.mkdir(parents=True,exist_ok=True)
            if not target.exists():os.link(cache/row['path'],target)
            if sha(target.read_bytes())!=row['candidateSha256']:raise ValueError('Selected AVIF mismatch')
            chosen.append({**row,'baselineBytes':baseline[row['path']]['outputBytes']})
    summary={**identity,'eligible':len(paths),'selected':len(chosen),'baselineBytes':sum(x['baselineBytes'] for x in chosen),'outputBytes':sum(x['candidateBytes'] for x in chosen),'minimumPsnr':min((x['psnr'] for x in chosen),default=None),'alphaExact':all(x['alphaExact'] for x in chosen),'selectedOverlay':str(selected),'seconds':time.monotonic()-started}
    atomic(output/'selected.json',(json.dumps(chosen,indent=2)+'\n').encode());atomic(output/'summary.json',(json.dumps(summary,indent=2)+'\n').encode());return summary
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--lossless-records',type=Path,required=True);p.add_argument('--vendor',type=Path,required=True);p.add_argument('--workers',type=int,default=4)
    a=p.parse_args();print(json.dumps(optimize(a.source,a.output,a.lossless_records,a.vendor,a.workers),indent=2))
