"""Export exact full-name Native bitmap records; does not decode/modify assets.

Analytic raw-table schema only, not an Android allocation or save-file schema.
Requires the pinned owned PE; no filename-derived body/variant heuristics.
"""


import argparse


import hashlib


import json


from pathlib import Path


import re


import struct


import pefile


from image_layout import TARGET_SHA256,TABLE_RVA,ROWS,STRIDE,native_name


DOMAIN=re.compile(r'[a-z0-9_]+\.mzp')


def record(index,primary,alias):
    for name in (primary,alias):
        if name is not None and not DOMAIN.fullmatch(name):raise ValueError('Invalid Native bitmap filename')
    if primary is None:
        if alias is not None:raise ValueError('Alias-only row not present in the pinned Native profile')
        return None
    return {'rowIndex':index,'key':alias or primary,'primary':primary,'alias':alias}


def extract(binary):
    raw=binary.read_bytes();digest=hashlib.sha256(raw).hexdigest()
    if digest!=TARGET_SHA256:raise ValueError('Native PE differs from reviewed bitmap profile')
    pe=pefile.PE(data=raw);table=pe.get_data(TABLE_RVA,ROWS*STRIDE)
    if len(table)!=ROWS*STRIDE:raise ValueError('Truncated Native bitmap table')
    records=[];keys=set();reserved=0
    for index in range(ROWS):
        pointers=struct.unpack_from('<QQ',table,index*STRIDE)
        names=[native_name(pe,pointer,index)+'.mzp' if pointer else None for pointer in pointers]
        item=record(index,*names)
        if item is None:reserved+=1;continue
        if item['key'] in keys:raise ValueError('Duplicate exact Native logical key')
        keys.add(item['key']);records.append(item)
    pairs=[item for item in records if item['alias'] is not None]
    if (len(records),reserved,len(pairs))!=(10821,35,3818):raise ValueError('Pinned Native bitmap record counts differ')
    records.sort(key=lambda item:item['key'])
    return {'schema':'woh.native.bitmap-records.v1','targetSHA256':digest,'tableRVA':hex(TABLE_RVA),'stride':STRIDE,'rawRows':ROWS,'namedRows':len(records),'reservedRows':reserved,'pairRows':len(pairs),'singleRows':len(records)-len(pairs),'distinctPairPrimary':len({item['primary'] for item in pairs}),'tableSHA256':hashlib.sha256(table).hexdigest(),'records':records}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--binary',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.binary.resolve()==args.output.resolve():raise ValueError('Output must not overwrite the PE')
    value=extract(args.binary);payload=(json.dumps(value,sort_keys=True,separators=(',',':'))+'\n').encode('ascii')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(payload)
    print(json.dumps({key:value[key] for key in ('targetSHA256','rawRows','namedRows','reservedRows','pairRows','singleRows','distinctPairPrimary','tableSHA256')}|{'outputSHA256':hashlib.sha256(payload).hexdigest(),'outputBytes':len(payload)},indent=2))


