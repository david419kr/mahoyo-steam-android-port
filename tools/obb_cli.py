"""Build a multilingual Mahoyo OBB from an owned Steam 1.1 installation."""
from pathlib import Path
import argparse
from contextlib import ExitStack
import hashlib
import io
import json
import os
import shutil
import sys
import time
import traceback

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
from cli_assets import compose_all, link_base, outside, prepare_all, reject_links, sha, valid_record
from edition_assets import detect_source, infer_edition, prepare_edition
from prepare_assets import _atomic_bytes, _json_bytes
from prepare_runtime_metadata import TARGET, VERSION as METADATA_VERSION, generate, native_image

MODES={
    'en':('Choose a packaging mode','1) Original quality\n   No additional lossy compression · Final size: over 20 GB · Less processing time',
          '2) AVIF compression\n   Final size: approximately 5 GB · Takes longer to create','Select [1/2]: '),
    'ko':('패키징 방식 선택','1) 원본 화질 유지\n   추가 손실 압축 없음 · 최종 용량 20GB 이상 · 상대적으로 빠른 제작',
          '2) AVIF 고효율 압축\n   최종 용량 약 5GB · 제작 시간이 오래 걸림','선택 [1/2]: ')}
STAGES={
    'verify':('Checking source files','원본 파일 확인'),'cache':('Checking cached assets','완료된 자산 확인'),
    'convert':('Converting assets','자산 변환'),'compose':('Composing character images','캐릭터 이미지 합성'),
    'metadata':('Preparing engine data','엔진 데이터 준비'),'convert-ko':('Preparing Korean resources','한국어 리소스 준비'),
    'compose-ko':('Composing Korean character images','한국어 캐릭터 이미지 합성'),
    'compress':('Compressing images','이미지 압축'),'compress-ko':('Compressing Korean images','한국어 이미지 압축'),
    'video':('Compressing video','영상 압축'),'video-check':('Checking video','영상 검증'),'pack':('Packing OBB','OBB 작성'),'verify-output':('Verifying OBB','OBB 검증')}


def check_dependencies(mode):
    import numpy
    import pefile,capstone,unicorn
    from PIL import Image,features
    if sys.maxsize<=2**32:raise ValueError('A 64-bit builder runtime is required')
    for name in ('steam11-source-profile.json','bitmap-composition-profile.json','steam11-save-migration-profile.json'):
        if not (ROOT/'data'/name).is_file():raise ValueError('Builder data is missing: '+name)
    if not (ROOT/'.deps/steamless/Steamless.CLI.exe').is_file():raise ValueError('Bundled executable reader is missing')
    if mode=='avif':
        if not features.check('avif') or not features.check('webp'):raise ValueError('Bundled image encoders are missing')
        sample=Image.new('RGBA',(16,16),(30,80,120,93));buffer=io.BytesIO()
        sample.save(buffer,'AVIF',quality=85,subsampling='4:4:4',max_threads=1,advanced={'alpha:lossless':'1'})
        with Image.open(io.BytesIO(buffer.getvalue())) as decoded:
            if decoded.convert('RGBA').getchannel('A').tobytes()!=sample.getchannel('A').tobytes():raise ValueError('AVIF alpha roundtrip failed')
        if any(not (ROOT/'.deps/media'/name).is_file() for name in ('ffmpeg.exe','ffprobe.exe')):raise ValueError('Bundled video tools are missing')


def select_mode(language,read=input,write=print):
    title,original,avif,prompt=MODES[language];write('\n'+title+'\n\n'+original+'\n\n'+avif+'\n')
    while True:
        value=read(prompt).strip()
        if value in ('1','2'):return 'original' if value=='1' else 'avif'
        write('1 또는 2를 입력해 주세요.' if language=='ko' else 'Enter 1 or 2.')


class Progress:
    def __init__(self,language,stream=sys.stdout):self.language=language;self.stream=stream;self.stage='';self.started=time.monotonic();self.last=0
    def __call__(self,row):
        stage=row.get('stage','');now=time.monotonic();done=row.get('done',0);total=row.get('total',0)
        if stage!=self.stage:self.stage=stage;self.started=now;self.last=0
        if now-self.last<1 and done!=total:return
        self.last=now;elapsed=row.get('seconds',now-self.started);ratio=(row.get('bytes',done)/max(1,row.get('totalBytes',total))) if total else 0
        eta=elapsed*(1-ratio)/ratio if 0<ratio<1 else 0
        label=STAGES.get(stage,(stage,stage))[self.language=='ko'];bar='#'*min(18,int(ratio*18));bar=bar.ljust(18,'-')
        file=str(row.get('file',''));line=f'[{bar}] {ratio:6.1%}  {label}  {done:,}/{total:,}  {int(elapsed)}s'
        if eta:line+=(' · 남은 시간 약 ' if self.language=='ko' else ' · ETA ')+str(int(eta))+'s'
        if file:line+='  '+file[-64:]
        print(line,file=self.stream,flush=True)


def common_identity(source):
    inputs=[r for r in source['inputs'] if '/' not in r['name']]
    value={'schema':1,'native':TARGET,'assetSchema':1,'inputs':inputs}
    return value,hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()[:20]


def _common_valid(common,identity,progress):
    marker=common/'complete.json';game=common/'game'
    if not marker.is_file():return False
    stamp=json.loads(marker.read_bytes())
    if stamp.get('metadataVersion')!=METADATA_VERSION or stamp.get('identity')!=identity or not (game/'assets.json').is_file() or sha(game/'assets.json')!=stamp.get('manifestSha256'):return False
    manifest=json.loads((game/'assets.json').read_bytes());rows=list(manifest['assets'].values());start=time.monotonic()
    for i,row in enumerate(rows,1):
        if not valid_record(game,row,True):return False
        if progress and (i%100==0 or i==len(rows)):progress({'stage':'cache','done':i,'total':len(rows),'seconds':time.monotonic()-start})
    return all((game/'meta'/name).is_file() and sha(game/'meta'/name)==value for name,value in stamp['metadata'].items())


def prepare_common(source,verified,cache,workers,progress):
    identity,key=common_identity(verified);common=cache/'common'/key;game=common/'game'
    binary=native_image(source,common/'native')
    if _common_valid(common,identity,progress):return game
    raw=common/'raw';prepare_all(source,raw,common/'raw-state',key,workers=min(4,workers),progress=progress)
    metadata=common/'metadata';provenance=common/'provenance'
    generate(binary,raw,metadata/'meta',provenance,progress)
    compose_all(raw,game,common/'composition-state',provenance/'native-bitmap-records.json',workers=min(4,workers),progress=progress)
    from image_dimensions import write as dimensions
    from audio_loop_metadata import generate as audio_loops
    dimensions(game,metadata)
    for path in sorted((metadata/'meta').iterdir()):
        if path.is_file():_atomic_bytes(game/'meta'/path.name,path.read_bytes())
    audio_loops(game)
    stamp={'identity':identity,'metadataVersion':METADATA_VERSION,'manifestSha256':sha(game/'assets.json'),'metadata':{p.name:sha(p) for p in sorted((game/'meta').iterdir()) if p.is_file()}}
    _atomic_bytes(common/'complete.json',_json_bytes(stamp));return game


def _font_maps(game):
    from asset_codecs import read_ccit
    manifest=json.loads((game/'assets.json').read_bytes())
    groups=[manifest['assets'],*manifest.get('overrides',{}).values()]
    for group in groups:
        for row in group.values():
            if row.get('kind')!='font-map':continue
            binary=game/row['path']
            if sha(binary)!=row['sha256']:raise ValueError('Font source changed: '+row['path'])
            data=_json_bytes(read_ccit(binary.read_bytes()));target=game/row['metadata_path']
            if not target.is_file() or target.read_bytes()!=data:_atomic_bytes(target,data)
            row['metadata_sha256']=hashlib.sha256(data).hexdigest()
    _atomic_bytes(game/'assets.json',_json_bytes(manifest))


def _members(game):
    from resumable_obb import directory_members, FileMember
    metadata={'edition.json','game-edition.txt','language-assets.tsv','ko-image-dimensions.tsv','language-messages.tsv',
              'native-composite-geometry.tsv','audio-loop-map.tsv','image-dimensions.tsv','native-exact-layouts.tsv',
              'effects.tsv','effects-window.tsv','effects-properties.tsv','image-layout.tsv','image-origins.tsv','native-image-resources.tsv',
              'archive-catalog.tsv','gallery-catalog.tsv','music-catalog.tsv','checkpoint-map.tsv','music-map.tsv','mask-map.tsv','wik-choice-layout.tsv'}
    result={name:member for name,member in directory_members(game).items() if not name.startswith('meta/') or name[5:] in metadata or name.startswith('meta/color') and name.endswith('-rgb.bin') or name.startswith('meta/window-atlas-') and name.endswith('.png')}
    manifest=json.loads((game/'assets.json').read_bytes());hashes={}
    for group in [manifest['assets'],*manifest.get('overrides',{}).values()]:
        for row in group.values():
            hashes[row['path']]=row['sha256']
            if 'metadata_path' in row:hashes[row['metadata_path']]=row['metadata_sha256']
    # Intermediate raw Korean patches remain in the resumable cache, never the OBB.
    return {name:FileMember(member.path,hashes.get(name) or sha(member.path)) for name,member in result.items()
            if name.startswith('meta/') or name=='assets.json' or name in hashes}


def pipeline(source,output,cache,mode,*,workers=2,progress=None,verified=None):
    source,output=outside(source,output);_,cache=outside(source,cache)
    if mode not in ('original','avif') or not 1<=workers<=16:raise ValueError('Invalid packaging mode/worker count')
    reject_links(output)
    if not cache.is_relative_to(output):reject_links(cache)
    verified=verified if verified is not None else detect_source(source,progress=progress)
    for record in verified.get('inputStats',[]):
        current=(source/record['name']).stat()
        if (current.st_size,current.st_mtime_ns,current.st_ctime_ns)!=(record['bytes'],record['mtimeNs'],record['ctimeNs']):raise ValueError('Input changed after source verification: '+record['name'])
    stats={r['name']:(source/r['name']).stat() for r in verified['inputs']};stats['WoH.exe']=(source/'WoH.exe').stat()
    common=prepare_common(source,verified,cache,workers,progress)
    edition=verified['edition'];edition_cache=cache/'editions'/verified['sourceFingerprint'][:20];game=edition_cache/'game'
    if game.is_dir():link_base(common,game)
    prepare_edition(source,common,game,workers=min(4,workers),progress=progress,verified_source=verified,
                    raw_source=common.parent/'raw',recipes=common.parent/'provenance/native-bitmap-records.json')
    from prepare_language_metadata import prepare as messages
    messages(source,game/'meta/language-messages.tsv',edition=='korean');_font_maps(game)
    members=_members(game);overlays=[]
    if mode=='avif':
        from optimize_edition import optimize
        from optimize_video import encode as videos
        from resumable_obb import directory_members, FileMember
        from PIL import features
        codecs={'webp':features.version('webp'),'avif':features.version('avif'),'schema':1}
        codec_key=hashlib.sha256(json.dumps(codecs,sort_keys=True).encode()).hexdigest()[:12]
        common_cache=common.parent/'encoded'/codec_key
        image_result=optimize(common,common_cache/'images',namespace='',workers=workers,progress=progress);overlays.append(Path(image_result['overlay']))
        ffmpeg=ROOT/'.deps/media/ffmpeg.exe';ffprobe=ROOT/'.deps/media/ffprobe.exe'
        if not ffmpeg.is_file() or not ffprobe.is_file():raise ValueError('Bundled video tools are missing')
        if progress:progress({'stage':'video','done':0,'total':4+(edition=='korean')})
        video_result=videos(common,common_cache/'video',str(ffmpeg),str(ffprobe),min(8,workers),progress=progress);overlays.append(Path(video_result['overlay']))
        if edition=='korean':
            ko_images=optimize(game,edition_cache/'encoded'/codec_key/'images',workers=min(4,workers),progress=progress);overlays.append(Path(ko_images['overlay']))
            ko_video=videos(game,edition_cache/'encoded'/codec_key/'video',str(ffmpeg),str(ffprobe),min(8,workers),'ko',progress);overlays.append(Path(ko_video['overlay']))
        for overlay in overlays:
            for name,member in directory_members(overlay).items():
                if name not in members:raise ValueError('Unknown encoded overlay member: '+name)
                members[name]=FileMember(member.path,sha(member.path))
    for name,before in stats.items():
        after=(source/name).stat()
        if (before.st_size,before.st_mtime_ns,before.st_ctime_ns)!=(after.st_size,after.st_mtime_ns,after.st_ctime_ns):raise ValueError('Input changed during preparation: '+name)
    from resumable_obb import build
    package='org.mahoyo.port'+('.kr' if edition=='korean' else '')
    target=output/edition/mode/f'main.1.{package}.obb'
    estimated=sum(m.size for m in members.values())+16*1024*1024
    partial=target.with_suffix('.obb.partial')
    if not target.exists() and shutil.disk_usage(output).free<estimated-(partial.stat().st_size if partial.exists() else 0):raise ValueError('Not enough free space for the final OBB')
    result=build(members,target,edition=edition,variant=mode,source_fingerprint=verified['sourceFingerprint'],progress=progress)
    _atomic_bytes(target.parent/'SHA256SUMS.txt',(result['obbSha256']+'  '+target.name+'\n').encode())
    return {k:result[k] for k in ('edition','variant','obbBytes','obbSha256','resumedMembers','validation')}|{'output':str(target),'package':package}


def main(argv=None,*,read=input,stream=sys.stdout):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',nargs='?',type=Path);parser.add_argument('--mode',choices=['original','avif']);parser.add_argument('--output',type=Path,default=Path.cwd()/'output');parser.add_argument('--cache',type=Path);parser.add_argument('--workers',type=int,default=min(8,max(1,(os.cpu_count() or 2)//2)))
    args=parser.parse_args(argv);language='en';log=None;locks=ExitStack()
    try:
        source=args.source or Path(read('Game folder / 게임 폴더: ').strip().strip('"'));source=source.resolve(strict=True)
        language='ko' if infer_edition(source)=='korean' else 'en'
        print('마법사의 밤 · OBB 작성 도구' if language=='ko' else 'WITCH ON THE HOLY NIGHT · OBB Builder',file=stream)
        mode=args.mode or select_mode(language,read,lambda text:print(text,file=stream))
        _,output=outside(source,args.output);_,cache=outside(source,args.cache or output/'.work')
        check_dependencies(mode)
        from cache_lock import exclusive
        reject_links(output)
        if not cache.is_relative_to(output):reject_links(cache)
        locks.enter_context(exclusive(cache))
        if cache!=output:locks.enter_context(exclusive(output))
        progress=Progress(language,stream);verified=detect_source(source,progress=progress)
        # Conservative measured bounds include raw PNGs, composed SCGs, output and selected encodings.
        cache.mkdir(parents=True,exist_ok=True);output.mkdir(parents=True,exist_ok=True)
        required=65*1024**3 if mode=='original' else 75*1024**3
        reject_links(cache);existing=0;seen=set()
        for parent,_,files in os.walk(cache):
            for name in files:
                stat=(Path(parent)/name).stat();key=(stat.st_dev,stat.st_ino)
                if key not in seen:existing+=stat.st_size;seen.add(key)
        free=shutil.disk_usage(cache).free
        print(('작업 폴더: ' if language=='ko' else 'Working folder: ')+str(cache),file=stream)
        print((f'최초 작업공간 예상: 약 {required/1024**3:.0f}GiB · 현재 여유: {free/1024**3:.1f}GiB' if language=='ko' else f'First-run space estimate: {required/1024**3:.0f} GiB · Available: {free/1024**3:.1f} GiB'),file=stream)
        if free<max(0,required-existing):raise ValueError('Insufficient estimated working space; choose another cache/output drive')
        log=output/'last-error.log'
        result=pipeline(source,output,cache,mode,workers=args.workers,progress=progress,verified=verified)
        print(('\n완료: ' if language=='ko' else '\nComplete: ')+result['output'],file=stream)
        print((f'최종 용량: {result["obbBytes"]/1e9:.2f}GB ({result["obbBytes"]:,}바이트)' if language=='ko' else f'Final size: {result["obbBytes"]/1e9:.2f} GB ({result["obbBytes"]:,} bytes)'),file=stream)
        print('SHA-256: '+result['obbSha256'],file=stream);return 0
    except (KeyboardInterrupt,EOFError):
        print('\n중단했습니다. 같은 명령을 다시 실행하면 완료된 항목부터 재개합니다.' if language=='ko' else '\nStopped. Run the same command to resume completed work.',file=stream);return 130
    except Exception as error:
        if log:
            try:log.write_text(traceback.format_exc(),encoding='utf-8')
            except OSError:pass
        print(('\n오류: 원본 파일·출력 경로·여유 공간을 확인해 주세요. ' if language=='ko' else '\nError: check the source files, output paths and free space. ')+str(error),file=stream)
        if log:print(('상세 기록: ' if language=='ko' else 'Details: ')+str(log),file=stream)
        return 1
    finally:locks.close()


if __name__=='__main__':raise SystemExit(main())
