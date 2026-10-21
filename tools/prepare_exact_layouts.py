"""Read exact resource extents/origins from the previously pinned owned PE table."""
from pathlib import Path
import argparse,hashlib,struct,re,json
SHA="1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"
def extract(path):
    b=Path(path).read_bytes()
    if hashlib.sha256(b).hexdigest()!=SHA:raise ValueError("Native PE fingerprint differs")
    pe=struct.unpack_from("<I",b,0x3c)[0];n=struct.unpack_from("<H",b,pe+6)[0];optional=struct.unpack_from("<H",b,pe+20)[0]
    def read(rva,size):
        for i in range(n):
            vs,va,rs,rp=struct.unpack_from("<4I",b,pe+24+optional+i*40+8)
            if va<=rva and size<=rs-(rva-va) and rva-va<rs:return b[rp+rva-va:rp+rva-va+size]
        raise ValueError("Native table range outside file-backed PE section")
    def name(pointer):
        raw=read(pointer-0x140000000,256).split(b"\0",1)[0];s=raw.decode("ascii").lower()
        if not re.fullmatch(r"[a-z0-9_]+\.mzp",s):raise ValueError("Invalid native image key")
        return s[:-4]
    rows={}
    for i in range(10856):
        row=read(0x2f5308+i*72,72);primary,alias=struct.unpack_from("<QQ",row)
        if not primary and not alias:continue
        key=name(alias or primary);w,h=struct.unpack_from("<HH",row,0x24);x,y=struct.unpack_from("<ii",row,0x2c)
        if key in rows or not w or not h:raise ValueError("Invalid native layout row")
        rows[key]=(w,h,x,y)
    if len(rows)!=10821:raise ValueError("Native layout table count differs")
    return rows
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--binary",type=Path,required=True);p.add_argument("--output",type=Path,required=True);a=p.parse_args();rows=extract(a.binary)
    data=(f"WOHEXACTLAYOUTS1 {SHA} {len(rows)}\n"+"".join(k+" "+" ".join(map(str,v))+"\n"for k,v in sorted(rows.items()))).encode("ascii")
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data);print(json.dumps(dict(rows=len(rows),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())))
