"""Validate ZIP64 layout, indexed ranges, declared assets and every member CRC."""
import json
from pathlib import Path
import struct
import time
import zipfile

from build_obb import safe_name


def verify(path,edition=None,progress=None):
    path=Path(path);started=time.monotonic()
    with zipfile.ZipFile(path) as archive,path.open('rb') as raw:
        entries=archive.infolist();names=[e.filename for e in entries];name_set=set(names)
        if len(names)!=len(name_set) or not 3<=len(names)<=100002:raise ValueError('Duplicate/invalid OBB member count')
        if any(not safe_name(n) for n in names):raise ValueError('Unsafe OBB member name')
        info=json.loads(archive.read('_obb/info.json'));index=archive.read('_obb/index.tsv')
        import hashlib
        if info.get('format')!='WOHOBB1' or info.get('dataVersion')!=1 or info.get('indexSha256')!=hashlib.sha256(index).hexdigest():raise ValueError('Invalid OBB profile/index hash')
        if edition is not None and info.get('edition','vanilla')!=edition:raise ValueError('OBB edition mismatch')
        lines=index.decode('utf-8').splitlines()
        if not lines or lines[0]!='WOHOBB1':raise ValueError('Invalid OBB index')
        indexed={};previous_end=0
        for line in lines[1:]:
            fields=line.split('\t')
            if len(fields)!=4:raise ValueError('Invalid OBB index row')
            name,offset,size,crc=fields;offset=int(offset);size=int(size);crc=int(crc,16)
            if name not in name_set or name in indexed or name.startswith('_obb/'):raise ValueError('Unexpected indexed member')
            entry=archive.getinfo(name)
            if entry.compress_type!=zipfile.ZIP_STORED or entry.flag_bits&1 or entry.file_size!=size or entry.CRC!=crc:raise ValueError('OBB directory/index mismatch')
            raw.seek(entry.header_offset);header=raw.read(30)
            if len(header)!=30 or header[:4]!=b'PK\x03\x04':raise ValueError('Invalid member local header')
            name_size,extra_size=struct.unpack_from('<HH',header,26)
            if offset!=entry.header_offset+30+name_size+extra_size or offset<previous_end or size<0 or offset+size>archive.start_dir:raise ValueError('OBB member range overlap/bounds')
            previous_end=offset+size;indexed[name]=(offset,size,crc)
        if set(names)-{'_obb/index.tsv','_obb/info.json'}!=set(indexed) or len(indexed)!=info.get('entries'):raise ValueError('Incomplete OBB resource index')
        manifest_bytes=archive.read('assets.json')
        if hashlib.sha256(manifest_bytes).hexdigest()!=info.get('manifestSha256'):raise ValueError('OBB manifest checksum mismatch')
        manifest=json.loads(manifest_bytes)
        if manifest.get('edition',{}).get('edition','vanilla')!=info.get('edition','vanilla'):raise ValueError('Manifest edition mismatch')
        catalogue=list(manifest.get('assets',{}).values())
        for group in manifest.get('overrides',{}).values():catalogue.extend(group.values())
        for record in catalogue:
            for key in ('path','metadata_path'):
                if key in record and record[key] not in indexed:raise ValueError('Missing declared resource: '+record[key])
        checked=0;total=sum(e.file_size for e in entries)
        for i,entry in enumerate(entries,1):
            with archive.open(entry) as stream:
                while block:=stream.read(1024*1024):checked+=len(block)
            if progress:progress({'stage':'verify-output','done':i,'total':len(entries),'bytes':checked,'totalBytes':total,'seconds':time.monotonic()-started})
        return {'members':len(indexed),'crcChecked':True,'indexedRangesChecked':True,'bytesRead':checked,'seconds':time.monotonic()-started}
