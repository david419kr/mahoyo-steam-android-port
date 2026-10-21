"""Extract shipped menu translations as data; never load a game EXE or patch DLL."""
import argparse
import csv
import io
from pathlib import Path

from asset_codecs import decode_lenzu
from pe_resources import resource
from prepare_assets import _atomic_bytes


def parse(payload):
    messages={}
    for row in csv.reader(io.StringIO(payload.decode('utf-8-sig'))):
        if not row:continue
        if len(row)!=5 or not row[0].startswith('SYSMES_') or row[0] in messages or len(row[0])>128 or any(len(s)>4096 for s in row):
            raise ValueError('Unexpected original menu translation row')
        messages[row[0]]=row[1:]
    if len(messages)!=375 or any(f'SYSMES_ARCHIVE_{i:03}' not in messages for i in range(73)):
        raise ValueError('Incomplete Steam 1.1 menu translation table')
    return messages


def read_messages(source,korean=False):
    source=Path(source)
    original=parse(decode_lenzu(resource(source/'WoH.exe','TEXT',5)))
    result={key:dict(zip(('ja','en','zh-Hans','zh-Hant'),values)) for key,values in original.items()}
    if korean:
        raw=(source/'patch_KR/mahoyo_kr.dll').read_bytes();marker=b'\xef\xbb\xbfSYSMES_CONF_000,'
        at=raw.find(marker)
        if at<0 or raw.find(marker,at+1)>=0:raise ValueError('Missing/ambiguous Korean menu table')
        end=raw.find(b'\0',at)
        if end<0 or end-at>1024*1024:raise ValueError('Korean menu table bounds')
        patched=parse(raw[at:end])
        if patched.keys()!=original.keys():raise ValueError('Korean menu key set differs')
        for key,values in patched.items():result[key]['ko']=values[1]
    return result


def quoted(text):
    return '"'+text.replace('\\','\\\\').replace('"','\\"')+'"'


def prepare(source,output,korean=False):
    messages=read_messages(source,korean)
    rows=['WOHMESSAGES1']
    for key,translations in sorted(messages.items()):
        for language,text in sorted(translations.items()):rows.append(key+' '+language+' '+quoted(text))
    _atomic_bytes(Path(output),('\n'.join(rows)+'\n').encode())
    return {'messages':len(messages),'languages':5 if korean else 4,'output':str(output)}


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('source',type=Path);parser.add_argument('output',type=Path);parser.add_argument('--korean',action='store_true');args=parser.parse_args()
    print(prepare(args.source,args.output,args.korean))
