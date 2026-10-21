"""Extract the pinned native Archive catalog; outputs stay local, never game text in Git.
Only the original data initializer runs in an isolated CPU; GUI/media do not run.
"""


from pathlib import Path


import argparse,csv,hashlib,io,json,struct,sys


ROOT=Path(__file__).resolve().parents[1]


sys.path.insert(0,str(ROOT/'tools'))


from metadata_cpu import Native,TARGET_SHA256


from asset_codecs import decode_lenzu


import pefile


from unicorn import UC_HOOK_CODE


from unicorn.x86_const import (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,
    UC_X86_REG_R9,UC_X86_REG_RAX,UC_X86_REG_RSP,UC_X86_REG_RIP)


from unicorn.x86_const import UC_X86_REG_RBX,UC_X86_REG_R15


class CatalogNative(Native):
    def __init__(self,raw):
        super().__init__(raw)
        self.uc.mem_map(0x20000000,0x100000)
        self.heap=0x20000000;self.flags={};self.atexit=0
        for rva in (0x143b0,0x17da04):
            self.uc.hook_add(UC_HOOK_CODE,self.hook,begin=self.base+rva,end=self.base+rva)
    def q(self,a):return struct.unpack('<Q',self.uc.mem_read(a,8))[0]
    def w(self,a,v):self.uc.mem_write(a,struct.pack('<Q',v))
    def ret(self,value=0):
        sp=self.uc.reg_read(UC_X86_REG_RSP);self.uc.reg_write(UC_X86_REG_RAX,value)
        self.uc.reg_write(UC_X86_REG_RIP,self.q(sp));self.uc.reg_write(UC_X86_REG_RSP,sp+8)
    def hook(self,uc,addr,size,user):
        if addr==self.base+0x17da04:
            self.atexit+=1;self.ret();return
        dest=uc.reg_read(UC_X86_REG_RCX);first=uc.reg_read(UC_X86_REG_RDX);end=uc.reg_read(UC_X86_REG_R8)
        offset=dest-self.base-0x3c3a60-0x38
        if offset<0 or offset%0x50 or offset//0x50>=73 or not 0<end-first<=64 or (end-first)%4:
            raise ValueError('Unexpected bounded vector constructor')
        row=offset//0x50;raw=uc.mem_read(first,end-first);flags=list(struct.unpack('<'+'I'*(len(raw)//4),raw))
        if row in self.flags:raise ValueError('Duplicate flags initializer')
        self.flags[row]=flags;heap=self.heap;self.heap+=len(raw)+16
        uc.mem_write(heap,bytes(raw));uc.mem_write(dest,struct.pack('<QQQ',heap,heap+len(raw),heap+len(raw)))
        self.ret(dest)
    def u32(self,a):return struct.unpack('<I',self.uc.mem_read(a,4))[0]
    def wide(self,a):
        if not self.base<=a<self.base+0x600000:raise ValueError('Unexpected native string pointer')
        data=bytearray()
        for i in range(1024):
            b=self.uc.mem_read(a+i*2,2)
            if b==b'\0\0':return data.decode('utf-16-le')
            data.extend(b)
        raise ValueError('Unterminated native string')


def sha(b):return hashlib.sha256(b).hexdigest()


def extract(pe_path,archive):
    raw=pe_path.read_bytes()
    native=CatalogNative(raw);native.call(0x1e50,{})
    if len(native.flags)!=73 or native.atexit!=1:raise ValueError('Unexpected native initializer')
    pe=native.pe
    selected=[]
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if str(t.name)!='TEXT':continue
        for e in t.directory.entries:
            if e.id!=5:continue
            for lang in e.directory.entries:
                d=lang.data.struct;selected.append((lang.id,d.OffsetToData,pe.get_data(d.OffsetToData,d.Size)))
    if len(selected)!=1 or selected[0][0]!=1041:raise ValueError('Localization resource differs')
    lang,rva,payload=selected[0];decoded=decode_lenzu(payload)
    rows=list(csv.reader(io.StringIO(decoded.decode('utf-8-sig'))));messages={}
    for row in rows:
        if not row:continue
        if len(row)!=5:raise ValueError('Unexpected locale row width')
        if row[0] in messages:raise ValueError('Duplicate original locale key')
        messages[row[0]]=dict(zip(('ja','en','zc','zt'),row[1:]))
    script=archive.read_bytes()
    if sha(script)!='67bff02a80cb6e75f67285085f1a15b129730adea5eaf338121af3028ada23a0':raise ValueError('Original Archive identity differs')
    code=48;width={0:1,1:1,2:1,3:1,8:1,9:1,10:1,0x19:1,0x7b:3,0x7f:2}
    ins=[];pc=0;source_line=0;end=20124
    while pc<end:
        op=struct.unpack_from('<I',script,code+pc)[0];n=width.get(op,0)
        args=list(struct.unpack_from('<'+'I'*n,script,code+pc+4)) if n else []
        if op==0x7f:source_line=args[1]
        ins.append({'pc':pc,'op':op,'args':args,'source':source_line});pc+=4+n*4
    if pc!=end:raise ValueError('Archive instruction/source-data boundary differs')
    def literal(offset):
        first=code+offset;last=script.index(0,first);return script[first:last].decode('utf8')
    dispatch={}
    for i in range(len(ins)-7):
        chunk=ins[i:i+7]
        if [s['op'] for s in chunk]!=[0,0,0xea,0,0x30,1,0x19]:continue
        if chunk[0]['args']!=[8] or chunk[1]['args']!=[1] or chunk[-1]['args']!=[0]:continue
        chapter_type=chunk[3]['args'][0];skip=chunk[5]['args'][0]
        body=[s for s in ins if chunk[-1]['pc']<s['pc']<skip]
        targets=[];writes={}
        for j,s in enumerate(body):
            if s['op']==0xf3:
                prev=body[j-1]
                if prev['op']!=3:raise ValueError('Nonliteral target')
                targets.append((s,literal(prev['args'][0]).lower()))
            if s['op']==0xe9:
                prev=body[j-4:j]
                if [r['op'] for r in prev]!=[0,3,0,0] or prev[-1]['args']!=[0]:
                    raise ValueError('Nonconstant local assignment in Archive branch')
                writes[str(prev[2]['args'][0])]=prev[0]['args'][0]
        if len(targets)!=1 or chapter_type in dispatch or set(writes)!={'24','25','26'}:
            raise ValueError('Original Archive branch shape differs')
        s,target=targets[0]
        if not target.endswith('.chs'):target+='.chs'
        dispatch[chapter_type]={'target':target,'sourceLine':s['source'],'branchPc':chunk[0]['pc'],
            'f3Pc':s['pc'],'falseTargetPc':skip,'entryLocalVariables':writes}
    if len(dispatch)!=73:raise ValueError('Expected73 verified comparison/replace branches')

    entries=[]
    for i in range(73):
        a=native.base+0x3c3a60+i*0x50;chapter=native.u32(a+0x28);story=native.u32(a+0x2c)
        key=native.wide(native.q(a+8));preview=native.wide(native.q(a+0x18)).lower();group=native.u32(a)
        if key!=f'SYSMES_ARCHIVE_{i:03}' or story!=i+1 or group>=20:raise ValueError('Invalid catalog identity')
        if chapter not in dispatch or key not in messages:raise ValueError('Missing original dispatch/label')
        d=dispatch[chapter]
        if d['entryLocalVariables']['26']!=story:raise ValueError('Story variable mismatch')
        entries.append(dict(storyId=story,group=group,chapterType=chapter,readGlobalFlag=native.u32(a+0x30),
            requiredGlobalFlags=native.flags[i],label=messages[key]['ja'],preview=preview,**d))
    if native.u32(native.base+0x3c3a60+73*0x50)!=0xffffffff:raise ValueError('Native sentinel differs')
    return entries


HEADERS=['010','015','020','030','040','050','055','060','070','080','085','090','extra','100','110','120','130','extra','extra','130']


def quoted(s):
    if any(ord(c)<32 for c in s):raise ValueError('Control character in catalog')
    return '"'+s.replace('\\','\\\\').replace('"','\\"')+'"'


def serialize(entries):
    if len(entries)!=73 or {e['storyId'] for e in entries}!=set(range(1,74)):raise ValueError('Expected73 unique entries')
    out=[f'WOHARCHIVE1 {TARGET_SHA256} 73 20']
    for i,h in enumerate(HEADERS):out.append(f'G {i} "archive_{h}_ja.cbg"')
    for e in entries:
        gates=e['requiredGlobalFlags']
        if not gates or any(not 0<=f<2048 for f in gates):raise ValueError('Invalid native gates')
        fields=[str(e[k]) for k in ('storyId','group','chapterType','readGlobalFlag')]
        fields += [','.join(map(str,gates))]+[quoted(e[k]) for k in ('label','preview','target')]
        fields += [str(e['entryLocalVariables'][k]) for k in ('24','25','26')]
        out.append('E '+' '.join(fields))
    return ('\n'.join(out)+'\n').encode('utf8')


def main():
    p=argparse.ArgumentParser();p.add_argument('--pe',type=Path,default=ROOT/'build/work/native/WoH.exe.unpacked.exe')
    p.add_argument('--archive',type=Path,default=ROOT/'build/work/assets_decoded/archive.chs')
    p.add_argument('--out',type=Path,default=ROOT/'build/work/archive-native/generated');a=p.parse_args()
    entries=extract(a.pe,a.archive);data=serialize(entries);a.out.mkdir(parents=True,exist_ok=True)
    (a.out/'archive-catalog.tsv').write_bytes(data)
    names=sorted({e['preview'] for e in entries}|{f'archive_{h}_ja.cbg' for h in HEADERS})
    (a.out/'requested-assets.json').write_text(json.dumps(names,indent=2)+'\n',encoding='utf8')
    receipt=dict(originalSHA256=TARGET_SHA256,archiveSHA256=hashlib.sha256(a.archive.read_bytes()).hexdigest(),
        entries=73,groups=20,requestedImageCount=len(names),catalogSHA256=hashlib.sha256(data).hexdigest(),
        initializerRVA='0x1e50',recordRVA='0x3c3a60',recordStride=80,uiExecuted=False,mediaExtracted=False)
    (a.out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print(json.dumps(receipt))


