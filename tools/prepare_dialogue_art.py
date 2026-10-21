"""Extract original dialogue decoration/cursor pixels from a user-owned HFA."""
import argparse, hashlib, json
from pathlib import Path
from hfa import read_index, read_at
from asset_codecs import decode_cbg, decode_mzp

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--source",type=Path,required=True);parser.add_argument("--output",type=Path,required=True);args=parser.parse_args()
    archive=args.source/"data00000.hfa";args.output.mkdir(parents=True,exist_ok=True);receipt=[]
    with archive.open("rb") as f:
        entries,_=read_index(f,archive.stat().st_size)
        for name,target in [("txtwindow00.mzp","window.png"),("lineBreak_a.cbg","cursor.png")]:
            entry=[e for e in entries if e.name==name][-1];raw=read_at(f,entry.offset,entry.size,archive.stat().st_size)
            image,_=(decode_mzp(raw) if name.endswith(".mzp") else decode_cbg(raw))
            if name.endswith(".mzp"):
                # Keep the ornament's premultiplied light contribution, remove its black backdrop.
                import numpy as np
                from PIL import Image
                pixels=np.asarray(image,dtype=np.uint16);peak=pixels[:,:,:3].max(2)
                rgb=np.rint(pixels[:,:,:3]*255/np.maximum(peak[:,:,None],1)).astype(np.uint8)
                alpha=np.rint(pixels[:,:,3]*peak/255).astype(np.uint8)
                image=Image.fromarray(np.dstack((rgb,alpha)),"RGBA")
            path=args.output/target;image.save(path,format="PNG",compress_level=9)
            receipt.append(dict(source=name,sourceSha256=hashlib.sha256(raw).hexdigest(),file=target,width=image.width,height=image.height,pngSha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    (args.output/"source.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(receipt))
if __name__=="__main__":main()
