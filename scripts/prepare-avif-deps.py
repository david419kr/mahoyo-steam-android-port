"""Stage source-version-pinned ARM64 codec packages and record local build hashes."""
from pathlib import Path
import argparse,hashlib,json,shutil,struct
ROOT=Path(__file__).resolve().parents[1]
PACKAGES=('libavif','dav1d','libyuv')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def arm64_archive(path):
    raw=path.read_bytes()
    if raw[:8]!=b'!<arch>\n':raise ValueError('Not a static archive: '+str(path))
    offset=8;count=0
    while offset<len(raw):
        header=raw[offset:offset+60]
        if len(header)!=60 or header[58:]!=b'`\n':raise ValueError('Truncated archive')
        size=int(header[48:58]);data=raw[offset+60:offset+60+size]
        if len(data)!=size:raise ValueError('Truncated archive member')
        if data.startswith(b'\x7fELF'):
            if data[4:6]!=b'\x02\x01' or struct.unpack_from('<H',data,18)[0]!=183:raise ValueError('Codec object is not AArch64 ELF64')
            count+=1
        offset+=60+size+(size%2)
    if not count:raise ValueError('No AArch64 objects in static archive')
def stage(packages,output):
    packages,output=Path(packages).resolve(),Path(output).resolve()
    if output==packages or output.is_relative_to(packages):raise ValueError('Stage outside the input packages')
    profile=json.loads((ROOT/'data/android-avif-deps.lock.json').read_bytes());notices=[];libraries={}
    for name in PACKAGES:
        source=packages/(name+'_arm64-android')
        spdx=json.loads((source/'share'/name/'vcpkg.spdx.json').read_bytes())
        if not spdx['name'].startswith(name+':arm64-android@'+profile['versions'][name]+' '):raise ValueError('Codec version differs: '+name)
        lib={'libavif':'avif','libyuv':'yuv'}.get(name,name);library=source/'lib'/('lib'+lib+'.a');arm64_archive(library)
        dest=output/'lib'/library.name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(library,dest);libraries[lib]=digest(dest)
        shutil.copytree(source/'include',output/'include',dirs_exist_ok=True)
        notices.append(name+'\n'+(source/'share'/name/'copyright').read_text(encoding='utf8'))
    for relative,expected in profile['stagedHeaders'].items():
        if digest(output/'include'/relative)!=expected:raise ValueError('Codec header mismatch: '+relative)
    lock={'stagedLibraries':libraries,'stagedHeaders':profile['stagedHeaders']}
    (output/'lock.json').write_text(json.dumps(lock,indent=2)+'\n',encoding='utf8')
    assets=ROOT/'android/app/src/main/assets';assets.mkdir(parents=True,exist_ok=True)
    (assets/'third-party-avif.txt').write_text('\n\n'.join(notices),encoding='utf8')
    return {'abi':profile['abi'],'versions':profile['versions'],'libraries':libraries}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--packages',type=Path,required=True);p.add_argument('--output',type=Path,default=ROOT/'.deps/android/arm64-v8a');a=p.parse_args();print(json.dumps(stage(a.packages,a.output),indent=2))
