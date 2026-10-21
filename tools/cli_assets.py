"""Bounded, resumable full conversion and native bitmap composition for the OBB CLI."""
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, ThreadPoolExecutor, wait
from copy import deepcopy
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import time

from asset_codecs import AssetError
from build_obb import safe_name
from hfa import read_at
from prepare_assets import CONVERTER_VERSION, _atomic_bytes, _convert, _json_bytes, _video, build_index


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def read_records(path):
    result={}
    if not path.is_file():return result
    lines=path.read_bytes().splitlines(keepends=True);complete=0
    for index,line in enumerate(lines):
        try:row=json.loads(line);name=row['name'];record=row['record'];relative=record['path']
        except (ValueError,KeyError,TypeError):
            if index!=len(lines)-1:raise ValueError('Corrupt completed conversion journal')
            with path.open('r+b') as stream:stream.truncate(complete)
            break
        if not safe_name(name) or '/' in name or not safe_name(relative) or relative.split('/')[0] not in {'images','scripts','text','fonts','audio','video'}:
            raise ValueError('Unsafe conversion journal path')
        result[name]=record;complete+=len(line)
    if complete and path.read_bytes()[-1:]!=b'\n':
        with path.open('ab') as stream:stream.write(b'\n')
    return result


def outside(source,output):
    source,output=Path(source).resolve(),Path(output).resolve()
    if source==output or source.is_relative_to(output) or output.is_relative_to(source):raise ValueError('Input and output trees must be separate')
    return source,output


def reject_links(root):
    if not root.exists():return
    pending=[root]
    while pending:
        with os.scandir(pending.pop()) as entries:
            for entry in entries:
                if entry.is_symlink():raise ValueError('Working/output folders cannot contain links: '+entry.path)
                if entry.is_dir(follow_symlinks=False):
                    if os.path.isjunction(entry.path):raise ValueError('Working/output folders cannot contain junctions: '+entry.path)
                    pending.append(entry.path)


def valid_record(root,record,checked_tree=False):
    if not isinstance(record,dict):return False
    relative=record.get('path','')
    if not safe_name(relative):return False
    original=root/relative;path=original if checked_tree else original.resolve()
    if not path.is_relative_to(root) or not checked_tree and original.is_symlink() or not path.is_file():return False
    if path.stat().st_size!=record.get('bytes') or sha(path)!=record.get('sha256'):return False
    if 'metadata_path' in record:
        if not safe_name(record['metadata_path']):return False
        extra=root/record['metadata_path'];extra=extra if checked_tree else extra.resolve()
        if not extra.is_relative_to(root) or not extra.is_file() or sha(extra)!=record.get('metadata_sha256'):return False
    return True


def _source(item):
    return {'archive':item.archive.name,'index':item.index,'offset':item.entry.offset,'size':item.entry.size,'archive_size':item.archive.stat().st_size}


def _convert_job(item,name,output):
    before=item.archive.stat()
    if name.endswith('.mp4'):record=_video(item,name,output)
    else:
        if item.entry.size>256*1024*1024:raise AssetError('Nonvideo input budget: '+name)
        with item.archive.open('rb') as stream:raw=read_at(stream,item.entry.offset,item.entry.size,before.st_size)
        record=_convert(raw,name,output);record['source_sha256']=hashlib.sha256(raw).hexdigest()
    after=item.archive.stat()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):raise AssetError('Input changed while converting '+name)
    path=output/record['path'];record.update(source=_source(item),converter_version=CONVERTER_VERSION,bytes=path.stat().st_size,sha256=sha(path))
    if 'metadata_path' in record:record['metadata_sha256']=sha(output/record['metadata_path'])
    return name,record


def _checkpoint_identity(state,identity):
    state.mkdir(parents=True,exist_ok=True);path=state/'identity.json'
    if path.exists() and json.loads(path.read_bytes())!=identity:raise ValueError('Conversion cache belongs to different inputs/settings')
    _atomic_bytes(path,_json_bytes(identity))


def prepare_all(source,output,state,fingerprint,*,workers=2,progress=None):
    source,output=outside(source,output);state=Path(state).resolve()
    if not 1<=workers<=4:raise ValueError('Use 1..4 conversion workers')
    if output.is_relative_to(state) or state.is_relative_to(output) or state.is_relative_to(source):raise ValueError('Separate conversion journal required')
    reject_links(output)
    index,archives=build_index(source)
    _checkpoint_identity(state,{'schema':1,'sourceFingerprint':fingerprint,'converter':CONVERTER_VERSION})
    output.mkdir(parents=True,exist_ok=True);manifest_path=output/'assets.json'
    manifest=json.loads(manifest_path.read_bytes()) if manifest_path.is_file() else {'schema_version':1,'design_size':[1920,1080],'assets':{}}
    if manifest.get('nativeBitmapCandidate'):raise ValueError('Raw conversion output cannot be an already composed tree')
    manifest['source_archives']=archives;manifest['archive_precedence']='last row wins; later sorted HFA wins (cross-HFA order provisional)'
    journal=state/'converted.jsonl';cached=dict(manifest['assets']);cached.update(read_records(journal));records={};jobs=[];started=time.monotonic()
    for checked,(name,item) in enumerate(sorted(index.items()),1):
        previous=cached.get(name);expected=_source(item)
        if previous and previous.get('converter_version')==CONVERTER_VERSION and all(previous.get('source',{}).get(k)==v for k,v in expected.items()) and valid_record(output,previous,True):records[name]=previous
        else:jobs.append((item,name,output))
        if progress and (checked%100==0 or checked==len(index)):progress({'stage':'cache','done':checked,'total':len(index),'file':name,'seconds':time.monotonic()-started})
    def save_manifest():
        manifest['assets']=dict(sorted(records.items()));_atomic_bytes(manifest_path,_json_bytes(manifest))
    save_manifest()
    reused=len(records)
    if progress:progress({'stage':'convert','done':reused,'total':len(index),'reused':reused,'seconds':time.monotonic()-started})
    pending=set();queue=iter(jobs);last=time.monotonic();since=0;pool=ProcessPoolExecutor(max_workers=workers)
    def submit():
        job=next(queue,None)
        if job is not None:pending.add(pool.submit(_convert_job,*job))
    with journal.open('a',encoding='utf-8') as log:
        try:
            for _ in range(workers*2):submit()
            while pending:
                done,rest=wait(pending,return_when=FIRST_COMPLETED);pending=rest
                for future in done:
                    name,record=future.result();records[name]=record;since+=1
                    log.write(json.dumps({'name':name,'record':record},separators=(',',':'))+'\n');log.flush()
                    if since>=256 or time.monotonic()-last>=15:
                        save_manifest();os.fsync(log.fileno());since=0;last=time.monotonic()
                    if progress:progress({'stage':'convert','done':len(records),'total':len(index),'reused':reused,'file':name,'seconds':time.monotonic()-started})
                    submit()
        finally:
            for future in pending:future.cancel()
            pool.shutdown(wait=True,cancel_futures=True);save_manifest()
    return {'assets':len(records),'reused':reused,'manifest':str(manifest_path)}


def link_base(source,output,excluded=()):
    source,output=outside(source,output);excluded=set(excluded)
    reject_links(source);reject_links(output)
    for folder in ('images','audio','video','scripts','text','fonts','meta'):
        root=source/folder
        if not root.exists():continue
        for parent,dirs,files in os.walk(root,followlinks=False):
            (output/Path(parent).relative_to(source)).mkdir(parents=True,exist_ok=True)
            for name in files:
                src=Path(parent)/name;relative=src.relative_to(source).as_posix()
                if relative in excluded:continue
                dest=output/relative
                if dest.is_file():
                    if not os.path.samefile(src,dest) and sha(src)!=sha(dest):
                        temporary=dest.with_name(dest.name+'.repair.tmp');shutil.copyfile(src,temporary);temporary.replace(dest)
                else:
                    try:os.link(src,dest)
                    except OSError:shutil.copyfile(src,dest)


def _compose_job(source,output,manifest,recipe,authority,target):
    from bitmap_composition import GEOMETRY_FIELDS, _read_image, key_replace
    from PIL import Image
    name=recipe['key'];primary,primary_record,primary_sha=_read_image(source,manifest,recipe['primary']);patch,patch_record,patch_sha=_read_image(source,manifest,recipe['alias'])
    pixels=key_replace(primary,patch);buffer=io.BytesIO();Image.fromarray(pixels).save(buffer,'PNG',compress_level=3,optimize=False);raw=buffer.getvalue();record=deepcopy(patch_record)
    for field in GEOMETRY_FIELDS:
        record.pop(field,None)
        if field in primary_record:record[field]=primary_record[field]
    record.update(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),nativeComposite={
        'logicalKey':name,'rowIndex':recipe['rowIndex'],'primary':recipe['primary'],'patch':recipe['alias'],
        'primaryInputSHA256':primary_sha,'patchInputSHA256':patch_sha,'oldPatchWH':[patch.shape[1],patch.shape[0]],
        'newPrimaryWH':[primary.shape[1],primary.shape[0]],'outputPixelSHA256':hashlib.sha256(pixels.tobytes()).hexdigest(),
        'outputFileSHA256':hashlib.sha256(raw).hexdigest(),'nativeAuthoritySHA256':authority,'nativeTargetSHA256':target})
    _atomic_bytes(output/record['path'],raw);return name,record


def compose_all(source,output,state,recipes,*,workers=2,progress=None):
    from bitmap_composition import GEOMETRY_FIELDS, load_recipes
    source,output=outside(source,output);state=Path(state).resolve()
    if not 1<=workers<=4 or state.is_relative_to(source) or state.is_relative_to(output):raise ValueError('Invalid composition workers/journal')
    table,authority=load_recipes(Path(recipes));manifest_bytes=(source/'assets.json').read_bytes();manifest=json.loads(manifest_bytes)
    if manifest.get('nativeBitmapCandidate'):raise ValueError('Native composition requires raw inputs')
    pairs={r['key']:r for r in table['records'] if r['alias'] is not None}
    identity={'schema':1,'rawManifestSha256':hashlib.sha256(manifest_bytes).hexdigest(),'recipesSha256':sha(recipes),'authority':authority}
    state.mkdir(parents=True,exist_ok=True);marker=state/'identity.json'
    if marker.is_file():
        old_identity=json.loads(marker.read_bytes())
        if any(old_identity.get(k)!=identity[k] for k in ('schema','recipesSha256','authority')):raise ValueError('Native composition settings changed')
    # A manifest ordering/font-metadata change must not invalidate identical image pairs.
    # Each reused pair below is checked against both input hashes and all geometry fields.
    _atomic_bytes(marker,_json_bytes(identity));output.mkdir(parents=True,exist_ok=True)
    reject_links(output)
    link_base(source,output,{manifest['assets'][name]['path'] for name in pairs})
    result=deepcopy(manifest);journal=state/'composed.jsonl';previous=read_records(journal);records={};jobs=[];started=time.monotonic()
    for checked,(name,recipe) in enumerate(sorted(pairs.items()),1):
        cached=previous.get(name);provenance=cached.get('nativeComposite',{}) if cached else {}
        primary=manifest['assets'][recipe['primary']]
        geometry_same=cached and all(cached.get(field)==primary.get(field) for field in GEOMETRY_FIELDS)
        if cached and geometry_same and provenance.get('primaryInputSHA256')==primary['sha256'] and provenance.get('patchInputSHA256')==manifest['assets'][recipe['alias']]['sha256'] and valid_record(output,cached,True):records[name]=cached
        else:jobs.append(recipe)
        if progress and (checked%100==0 or checked==len(pairs)):progress({'stage':'cache','done':checked,'total':len(pairs),'file':name,'seconds':time.monotonic()-started})
    pending=set();queue=iter(jobs);pool=ThreadPoolExecutor(max_workers=workers);reused=len(records)
    def submit():
        recipe=next(queue,None)
        if recipe is not None:pending.add(pool.submit(_compose_job,source,output,manifest,recipe,authority,table['targetSHA256']))
    with journal.open('a',encoding='utf-8') as log:
        try:
            for _ in range(workers*2):submit()
            while pending:
                done,rest=wait(pending,return_when=FIRST_COMPLETED);pending=rest
                for future in done:
                    name,record=future.result();records[name]=record;log.write(json.dumps({'name':name,'record':record},separators=(',',':'))+'\n');log.flush()
                    if progress:progress({'stage':'compose','done':len(records),'total':len(pairs),'reused':reused,'file':name,'seconds':time.monotonic()-started})
                    submit()
        finally:
            for future in pending:future.cancel()
            pool.shutdown(wait=True,cancel_futures=True)
    result['assets'].update(records);result['nativeBitmapCandidate']={'schema':1,'stageReady':True,'selectedPairs':len(pairs),'composed':len(records),'nativeAuthoritySHA256':authority,'recipeTableSHA256':sha(recipes)}
    geometry=f"WOHCOMPOSITEGEOMETRY1 {table['targetSHA256']} {authority} {len(records)}\n"
    for name,record in sorted(records.items()):
        p=record['nativeComposite'];geometry+=f'"{name}" {p["oldPatchWH"][0]} {p["oldPatchWH"][1]} {p["newPrimaryWH"][0]} {p["newPrimaryWH"][1]}\n'
    _atomic_bytes(output/'meta/native-composite-geometry.tsv',geometry.encode('ascii'));_atomic_bytes(output/'assets.json',_json_bytes(result))
    return {'pairs':len(records),'reused':reused,'manifest':str(output/'assets.json')}
