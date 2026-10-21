"""Seekable ZIP64 packaging with per-member durable resume and payload CRC verification."""
from __future__ import annotations
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import time
import zipfile
import zlib

from build_obb import engine_file, safe_name
from prepare_assets import _atomic_bytes, _json_bytes


@dataclass(frozen=True)
class FileMember:
    path: Path
    content_sha256: str | None = None

    @property
    def size(self):return self.path.stat().st_size
    def identity(self):
        info=self.path.stat()
        return ['sha256',info.st_size,self.content_sha256] if self.content_sha256 else ['file',info.st_size,info.st_mtime_ns]
    def open(self):return self.path.open('rb')


@dataclass(frozen=True)
class ZipMember:
    archive: zipfile.ZipFile
    name: str

    @property
    def size(self):return self.archive.getinfo(self.name).file_size
    def identity(self):
        info=self.archive.getinfo(self.name);return ['zip',info.file_size,info.CRC]
    def open(self):return self.archive.open(self.name)


def directory_members(root):
    root=Path(root).resolve();result={}
    for directory,dirs,files in os.walk(root,followlinks=False):
        for name in dirs+files:
            p=Path(directory)/name
            if p.is_symlink() or p.is_junction():raise ValueError('Resource links/reparse points are unsupported')
        for name in files:
            p=Path(directory)/name;relative=p.relative_to(root).as_posix()
            if relative.startswith('_obb/'):continue
            if not safe_name(relative) or relative!='assets.json' and relative.split('/',1)[0] not in {'images','scripts','text','meta','fonts','audio','video'}:
                raise ValueError('Unexpected resource member: '+relative)
            if relative.endswith(('.tmp','.partial')):raise ValueError('Incomplete source conversion: '+relative)
            result[relative]=FileMember(p)
    return result


def zip_members(archive):
    result={}
    for entry in archive.infolist():
        if entry.filename.startswith('_obb/'):continue
        if not safe_name(entry.filename) or entry.compress_type!=zipfile.ZIP_STORED or entry.flag_bits&1 or entry.is_dir():
            raise ValueError('Unsupported base OBB member')
        if entry.filename in result:raise ValueError('Duplicate base OBB member')
        result[entry.filename]=ZipMember(archive,entry.filename)
    return result


def _zip_info(name, row=None):
    info=zipfile.ZipInfo(name);info.compress_type=zipfile.ZIP_STORED
    info.external_attr=(stat.S_IFREG|0o444)<<16;info.create_system=3
    if row is not None:
        info.header_offset=row['header'];info.file_size=info.compress_size=row['size'];info.CRC=row['crc']
        info.extract_version=info.create_version=45
    return info


def _valid_member(stream,row,total):
    if any(type(row.get(k)) is not int or not 0<=row[k]<=2**63-1 for k in ('header','data','size','end','crc')) or row['crc']>0xffffffff:return False
    if not safe_name(row['name']) or row['header']<0 or row['data']<row['header'] or row['end']!=row['data']+row['size'] or row['end']>total:return False
    stream.seek(row['header']);header=stream.read(30)
    if len(header)!=30:return False
    signature,version,flags,method,clock,date,crc,compressed,size,name_size,extra_size=struct.unpack('<IHHHHHIIIHH',header)
    if signature!=0x04034b50 or method!=zipfile.ZIP_STORED or flags&9 or crc!=row['crc']:return False
    name=stream.read(name_size);extra=stream.read(extra_size)
    if name!=row['name'].encode('utf-8') or stream.tell()!=row['data']:return False
    if compressed==0xffffffff or size==0xffffffff:
        at=0;lengths=None
        while at+4<=len(extra):
            tag,n=struct.unpack_from('<HH',extra,at);at+=4
            if at+n>len(extra):return False
            if tag==1 and n>=16:lengths=struct.unpack_from('<QQ',extra,at)
            at+=n
        if lengths!=(row['size'],row['size']):return False
    elif (compressed,size)!=(row['size'],row['size']):return False
    actual=0;remaining=row['size']
    while remaining:
        block=stream.read(min(remaining,4*1024*1024))
        if not block:return False
        actual=zlib.crc32(block,actual);remaining-=len(block)
    return actual&0xffffffff==row['crc']


def build(members, output, *, edition, variant, source_fingerprint, progress=None, interrupt_after=None):
    output=Path(output).resolve();output.parent.mkdir(parents=True,exist_ok=True)
    if edition not in ('vanilla','korean') or variant not in ('original','avif'):raise ValueError('Unknown OBB profile')
    if 'assets.json' not in members or not 1<=len(members)<=100000:raise ValueError('Missing/oversized resource catalogue')
    with members['assets.json'].open() as stream:manifest_bytes=stream.read(64*1024*1024+1)
    if len(manifest_bytes)>64*1024*1024:raise ValueError('Manifest budget')
    manifest=json.loads(manifest_bytes)
    declared=manifest.get('edition',{}).get('edition','vanilla')
    if declared!=edition:raise ValueError('Manifest/OBB edition mismatch')
    if edition=='korean' and 'script_text_en.ctd' not in manifest.get('overrides',{}).get('ko',{}):raise ValueError('Korean text override missing')
    catalogue=list(manifest.get('assets',{}).values())
    for group in manifest.get('overrides',{}).values():catalogue.extend(group.values())
    for record in catalogue:
        for field in ('path','metadata_path'):
            if field in record and (not safe_name(record[field]) or record[field] not in members):raise ValueError('Missing/unsafe declared asset: '+str(record[field]))
    names=sorted(members)
    if any(not safe_name(n) or n.startswith('_obb/') for n in names):raise ValueError('Unsafe resource catalogue')
    for name,member in members.items():
        if isinstance(member,FileMember) and (member.path.resolve()==output or output.is_relative_to(member.path.resolve())):raise ValueError('OBB output overlaps source')
    descriptors={n:members[n].identity() for n in names}
    identity={'schema':'WOH_PACK_RESUME1','edition':edition,'variant':variant,'sourceFingerprint':source_fingerprint,'members':descriptors}
    partial=output.with_suffix(output.suffix+'.partial');state=output.with_suffix(output.suffix+'.state.json');journal=output.with_suffix(output.suffix+'.members.jsonl');receipt=output.with_suffix(output.suffix+'.build.json')
    if output.exists():
        if not receipt.is_file():raise ValueError('Existing OBB has no completed receipt')
        old=json.loads(receipt.read_text());digest=hashlib.sha256(output.read_bytes()).hexdigest() if output.stat().st_size<16*1024*1024 else _hash(output)
        if old.get('identity')!=identity or old.get('obbSha256')!=digest:raise ValueError('Existing output belongs to different or damaged work')
        if state.exists() and json.loads(state.read_text())==identity:
            state.unlink()
            if journal.exists():journal.unlink()
        return old
    complete=[]
    if partial.exists():
        if not state.is_file() or not journal.is_file():raise ValueError('Partial OBB has no owned resume metadata')
        previous=json.loads(state.read_text())
        if previous!=identity:raise ValueError('OBB inputs/profile changed; choose separate output')
        lines=journal.read_bytes().splitlines();total=partial.stat().st_size
        with partial.open('rb') as stream:
            for index,line in enumerate(lines):
                try:
                    row=json.loads(line)
                    if index>=len(names) or row['name']!=names[index] or row['source']!=descriptors[row['name']]:break
                    if row['header']!=(complete[-1]['end'] if complete else 0) or not _valid_member(stream,row,total):break
                    complete.append(row)
                except (ValueError,KeyError,TypeError,struct.error):break
        committed_end=complete[-1]['end'] if complete else 0
        with partial.open('r+b') as stream:stream.truncate(committed_end)
        _atomic_bytes(journal,b''.join((json.dumps(row,separators=(',',':'))+'\n').encode() for row in complete))
    else:
        if state.exists() or journal.exists():raise ValueError('Unowned OBB work files without partial payload')
        _atomic_bytes(state,_json_bytes(identity));_atomic_bytes(journal,b'');partial.touch(exist_ok=False)
    started=time.monotonic();resumed=len(complete);total_bytes=sum(m.size for m in members.values());packed_bytes=sum(r['size'] for r in complete)
    last_sync=started;sync_bytes=packed_bytes
    with partial.open('r+b') as raw,journal.open('ab') as log:
        # Pinned Python ZipFile appends at EOF when the truncated stream has no directory.
        # Reconstruct the prior central-directory records from validated local headers.
        with zipfile.ZipFile(raw,'a',compression=zipfile.ZIP_STORED,allowZip64=True) as archive:
            if archive.filelist:raise ValueError('Unexpected directory after resume truncation')
            for row in complete:
                info=_zip_info(row['name'],row);archive.filelist.append(info);archive.NameToInfo[info.filename]=info
            for name in names[resumed:]:
                member=members[name];before=member.identity();info=_zip_info(name);expected=getattr(member,'content_sha256',None);digest=hashlib.sha256() if expected else None
                with member.open() as original,archive.open(info,'w',force_zip64=True) as encoded:
                    offset=raw.tell()
                    while True:
                        block=original.read(1024*1024)
                        if not block:break
                        encoded.write(block)
                        if digest:digest.update(block)
                if digest and digest.hexdigest()!=expected:raise ValueError('Source content checksum changed: '+name)
                if member.identity()!=before or info.file_size!=member.size:raise ValueError('Source changed while packing: '+name)
                raw.flush()
                row={'name':name,'header':info.header_offset,'data':offset,'size':info.file_size,'crc':info.CRC,'end':raw.tell(),'source':before}
                log.write((json.dumps(row,separators=(',',':'))+'\n').encode());log.flush();complete.append(row);packed_bytes+=row['size']
                if packed_bytes-sync_bytes>=16*1024*1024 or time.monotonic()-last_sync>=1:
                    os.fsync(raw.fileno());os.fsync(log.fileno());last_sync=time.monotonic();sync_bytes=packed_bytes
                if progress:progress({'stage':'pack','done':len(complete),'total':len(names),'bytes':packed_bytes,'totalBytes':total_bytes,'resumed':resumed,'seconds':time.monotonic()-started})
                if interrupt_after is not None and len(complete)>=interrupt_after:raise InterruptedError('Requested checkpoint interruption')
            index=('WOHOBB1\n'+''.join(f"{r['name']}\t{r['data']}\t{r['size']}\t{r['crc']:08x}\n" for r in complete)).encode()
            engine_bytes=sum(r['size'] for r in complete if engine_file(r['name']))
            if len(index)>16*1024*1024 or engine_bytes>256*1024*1024:raise ValueError('OBB engine metadata budget')
            manifest_sha=hashlib.sha256(manifest_bytes).hexdigest()
            info={'format':'WOHOBB1','dataVersion':1,'edition':edition,'variant':variant,'entries':len(names),'bytes':total_bytes,'engineBytes':engine_bytes,
                  'indexSha256':hashlib.sha256(index).hexdigest(),'manifestSha256':manifest_sha,'minimumApkVersionCode':76,'sourceFingerprint':source_fingerprint}
            archive.writestr('_obb/index.tsv',index);archive.writestr('_obb/info.json',json.dumps(info,sort_keys=True).encode())
    with zipfile.ZipFile(partial) as archive:
        if len(archive.infolist())!=len(names)+2 or archive.read('_obb/index.tsv')!=index:raise ValueError('OBB directory validation failed')
    from verify_obb import verify
    validation=verify(partial,edition,progress)
    result={**info,'identity':identity,'validation':validation,'obbBytes':partial.stat().st_size,'obbSha256':_hash(partial),'resumedMembers':resumed,'seconds':time.monotonic()-started}
    _atomic_bytes(receipt,_json_bytes(result))
    partial.replace(output)
    state.unlink();journal.unlink()
    return result


def _hash(path):
    with Path(path).open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()
