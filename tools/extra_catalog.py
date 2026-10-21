"""Pinned CG/music/checkpoint metadata. No media extraction or original UI execution."""


import argparse,hashlib,json,re,struct


from pathlib import Path


import pefile


from effect_metadata import TARGET_SHA256


ROOT=Path(__file__).resolve().parents[1]


COMPLETE=(128,65,73,72,206,129,130,131,132,133,134)


MUSIC_ATLAS={9:'mu_text1_select_ja.cbg',10:'mu_text2_select_ja.cbg',11:'mu_text3_select_ja.cbg'}


def quoted(s):
    if not s or any(ord(c)<32 for c in s):raise ValueError('Invalid metadata string')
    return '"'+s.replace('\\','\\\\').replace('"','\\"')+'"'


def extract(path):
    raw=path.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=TARGET_SHA256:raise ValueError('Unpinned native image')
    pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase;size=pe.OPTIONAL_HEADER.SizeOfImage
    def data(rva,n):
        if not 0<=rva<size or rva+n>size:raise ValueError('Native table bounds')
        out=pe.get_data(rva,n)
        if len(out)!=n:raise ValueError('Truncated native table')
        return out
    def string(ptr,wide=False):
        if not base<=ptr<base+size:raise ValueError('Invalid native string pointer')
        rva=ptr-base;out=bytearray();step=2 if wide else 1
        for i in range(128):
            value=data(rva+i*step,step)
            if value==bytes(step):return out.decode('utf-16-le' if wide else 'ascii').lower()
            out.extend(value)
        raise ValueError('Native string exceeds limit')
    specials=[]
    for group in range(21):
        values=struct.unpack('<6i',data(0x3c5f40+group*24,24));cut=next((i for i,v in enumerate(values) if v<0),6)
        if not cut or any(v!=-1 for v in values[cut:]) or any(not 0<=v<2048 for v in values[:cut]):raise ValueError('Invalid special flag group')
        specials.append(list(values[:cut]))
    gallery=[];names=set()
    for slot,ptr in enumerate(struct.unpack('<420Q',data(0x3c5220,420*8))):
        variants=[]
        for v in range(37):
            row=data(ptr-base+v*56,56);nameptr=struct.unpack_from('<Q',row)[0]
            if not nameptr:break
            name=string(nameptr)
            if not re.fullmatch(r'img\d{4}\.mzp',name) or name in names:raise ValueError('Invalid/duplicate CG name')
            ident=int(name[3:7]);group=-1
            if 184<=ident<184+0xd84:
                leaf=struct.unpack('<I',data(0x1b714+(ident-184)*4,4))[0];code=data(leaf,8)
                if code[:2] in (b'\x31\xdb',b'\x33\xdb'):group=0
                elif code[0]==0xbb:group=struct.unpack_from('<i',code,1)[0]
                elif leaf==0x1b638:group=3
                else:raise ValueError('Unreviewed native CG leaf')
            if not -1<=group<=20:raise ValueError('Invalid native CG group')
            names.add(name);variants.append(dict(index=v,name=name,required=specials[group] if group>=0 else None))
        if variants:gallery.append(dict(slot=slot,variants=variants))
    if len(gallery)!=401 or len(names)!=1314:raise ValueError('CG catalog counts differ')
    music=[]
    for row in range(56):
        fields=struct.unpack('<14i',data(0x57f620+row*56,56));ident=fields[0]
        if not 1<=ident<=65 or fields[1] not in MUSIC_ATLAS or fields[5] not in (0,528) or fields[6]<0 or fields[6]%72 or fields[6]>936:raise ValueError('Music record differs')
        name=string(struct.unpack('<Q',data(0x5806d0+ident*8,8))[0],True)
        if not re.fullmatch(r'[a-z0-9_]+\.hw',name):raise ValueError('Invalid music filename')
        music.append(dict(row=row,id=ident,flag=256+ident,name=name,atlas=MUSIC_ATLAS[fields[1]],x=fields[5],y=fields[6],w=528,h=72))
    if len({m['id'] for m in music})!=56:raise ValueError('Duplicate music ID')
    pointers=struct.unpack('<14Q',data(0x580660,14*8))
    if pointers[0]:raise ValueError('Invalid title checkpoint sentinel')
    checkpoints=[dict(selector=i,target=string(pointers[i],True)) for i in range(1,14)]
    if any(not re.fullmatch(r'teatime[a-z0-9]+\.chs',v['target']) for v in checkpoints) or len({v['target'] for v in checkpoints})!=13:raise ValueError('Invalid checkpoint map')
    return gallery,music,checkpoints


def write(gallery,music,checkpoints,out):
    if len(gallery)!=401 or sum(len(g['variants'])for g in gallery)!=1314 or len(music)!=56 or len(checkpoints)!=13:raise ValueError('Incomplete Extra catalog')
    out.mkdir(parents=True,exist_ok=True)
    cg=[f'WOHGALLERY1 {TARGET_SHA256} 401 1314']
    for g in gallery:
        cg.append(f'G {g["slot"]} {len(g["variants"])}')
        for v in g['variants']:cg.append(f'V {g["slot"]} {v["index"]} {quoted(v["name"])} '+('-' if v['required'] is None else ','.join(map(str,v['required']))))
    tracks=[f'WOHMUSIC1 {TARGET_SHA256} 56']
    for m in music:tracks.append('M '+' '.join(str(m[k]) for k in ('row','id','flag'))+' '+quoted(m['name'])+' '+quoted(m['atlas'])+' '+' '.join(str(m[k]) for k in ('x','y','w','h')))
    cp=[f'WOHCHECKPOINT1 {TARGET_SHA256} 13']+[f'C {c["selector"]} {quoted(c["target"])}' for c in checkpoints]
    hashes={}
    for name,lines in [('gallery-catalog.tsv',cg),('music-catalog.tsv',tracks),('checkpoint-map.tsv',cp)]:
        raw=('\n'.join(lines)+'\n').encode('utf8');(out/name).write_bytes(raw);hashes[name]=hashlib.sha256(raw).hexdigest()
    requested=sorted({v['name'] for g in gallery for v in g['variants']}|{m['name'] for m in music}|set(MUSIC_ATLAS.values())|{c['target'] for c in checkpoints})
    (out/'requested-assets.json').write_text(json.dumps(requested,indent=2)+'\n',encoding='utf8')
    receipt=dict(targetSHA256=TARGET_SHA256,galleryGroups=401,galleryVariants=1314,musicRows=56,checkpointReturnDestinations=13,notASelectableCheckpointCatalog=True,completionFlags=COMPLETE,outputSHA256=hashes,requestedAssets=len(requested),originalUiExecuted=False,mediaExtracted=False)
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');return receipt


def main():
    p=argparse.ArgumentParser();p.add_argument('--pe',type=Path,default=ROOT/'build/work/native/WoH.exe.unpacked.exe');p.add_argument('--out',type=Path,default=ROOT/'build/work/extra-frontends/generated');a=p.parse_args()
    print(json.dumps(write(*extract(a.pe),a.out)))


