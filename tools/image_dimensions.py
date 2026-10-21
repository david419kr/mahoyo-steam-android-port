"""Record prepared bitmap dimensions for runtime metadata."""
from pathlib import Path
import argparse,struct
def write(source,overlay):
    source,overlay=Path(source).resolve(),Path(overlay).resolve()
    if source==overlay or overlay.is_relative_to(source):raise ValueError('Output must be outside source')
    rows=['WOH_IMAGE_DIMENSIONS1\n']
    for path in sorted((source/'images').glob('*.png')):
        with path.open('rb') as f:header=f.read(24)
        if len(header)!=24 or header[:8]!=b'\x89PNG\r\n\x1a\n' or header[12:16]!=b'IHDR':raise ValueError('Expected PNG: '+str(path))
        w,h=struct.unpack('>II',header[16:24]);name=path.name[:-4]
        if not 0<w<=65535 or not 0<h<=65535 or any(c in name for c in '\\/:\t\n\r'):raise ValueError('Invalid image dimensions/name')
        rows.append(f'{name}\t{w}\t{h}\n')
    path=overlay/'meta/image-dimensions.tsv';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(''.join(rows),encoding='utf8',newline='\n');return len(rows)-1
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('overlay',type=Path);a=p.parse_args();print(write(a.source,a.overlay))
