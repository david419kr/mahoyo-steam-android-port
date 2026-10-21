"""Create a separate HEVC CRF16 overlay; keep original audio/logo unchanged."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,time
from fractions import Fraction

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(4*1024*1024),b''):h.update(block)
    return h.hexdigest()
def probe(ffprobe,path):return json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams','-show_format','-of','json',str(path)],text=True))
KEYS=('width','height','pix_fmt','nb_frames','duration','start_time','time_base','r_frame_rate','avg_frame_rate','color_range','color_space','color_transfer','color_primaries','chroma_location')

def timing_arguments(stream):
    unit=Fraction(stream['time_base'])
    if unit.numerator!=1 or not 1<=unit.denominator<=1000000000:raise ValueError('Unreviewed source video time base')
    return ['-fps_mode','passthrough','-video_track_timescale',str(unit.denominator)]
def encode(source,output,ffmpeg='ffmpeg',ffprobe='ffprobe',threads=4,namespace='',progress=None):
    source,output=Path(source).resolve(),Path(output).resolve()
    if source==output or output.is_relative_to(source) or source.is_relative_to(output):raise ValueError('Source/output trees must be separate')
    if namespace not in ('','ko'):raise ValueError('Unsupported video namespace')
    if not 1<=threads<=8:raise ValueError('Use 1..8 encoder threads')
    output.mkdir(parents=True,exist_ok=True);overlay=output/'hevc16';video=Path('video')/namespace;(overlay/video).mkdir(parents=True,exist_ok=True);rows=[]
    version=subprocess.check_output([ffmpeg,'-version'],text=True).splitlines()[0]
    paths=sorted((source/video).glob('opening_*.mp4'))
    for ordinal,path in enumerate(paths,1):
        target=overlay/video/path.name;receipt=output/((namespace+'-' if namespace else '')+path.stem+'.json');before=probe(ffprobe,path);streams=before['streams']
        if len(streams)!=1 or streams[0]['codec_name']!='h264':raise ValueError('Expected original silent H264 opening')
        original=sha(path)
        if target.exists():
            if not receipt.exists():raise ValueError('Existing video has no completed receipt')
            row=json.loads(receipt.read_text());
            if row['sourceSha256']!=original or row['outputSha256']!=sha(target):raise ValueError('Video identity changed')
            rows.append(row)
            if progress:progress({'stage':'video','done':ordinal,'total':len(paths),'file':path.name+' (cached)'})
            continue
        partial=target.with_suffix('.partial.mp4');started=time.monotonic()
        command=[ffmpeg,'-hide_banner','-loglevel','warning','-nostdin','-y','-i',str(path),'-map','0:v:0','-an','-c:v','libx265','-preset','medium','-crf','16','-pix_fmt','yuv420p','-tag:v','hvc1','-x265-params',f'pools={threads}:frame-threads=2:log-level=error','-threads',str(threads),'-map_metadata','0',*timing_arguments(streams[0]),'-movflags','+faststart',str(partial)]
        try:
            if progress:command[-1:-1]=['-progress','pipe:1','-nostats']
            with (output/(path.stem+'.encode.log')).open('w') as log:
                if not progress:subprocess.run(command,check=True,stdout=log,stderr=log)
                else:
                    with subprocess.Popen(command,stdout=subprocess.PIPE,stderr=log,text=True,encoding='utf-8',creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0)) as process:
                        try:
                            for line in process.stdout:
                                key,_,value=line.strip().partition('=')
                                if key=='frame':progress({'stage':'video','done':int(value),'total':int(streams[0]['nb_frames']),'file':f'{path.name} ({ordinal}/{len(paths)})','seconds':time.monotonic()-started})
                            if process.wait():raise subprocess.CalledProcessError(process.returncode,command)
                        except BaseException:process.kill();process.wait();raise
            if progress:progress({'stage':'video-check','done':0,'total':1,'file':path.name})
            after=probe(ffprobe,partial);a=streams[0];b=after['streams'][0]
            if len(after['streams'])!=1 or b['codec_name']!='hevc' or any(a.get(k)!=b.get(k) for k in KEYS):raise ValueError('Video timing/geometry/color metadata changed: '+path.name+' '+repr({k:(a.get(k),b.get(k)) for k in KEYS if a.get(k)!=b.get(k)}))
            result=subprocess.run([ffmpeg,'-hide_banner','-loglevel','info','-nostdin','-i',str(path),'-i',str(partial),'-filter_complex','[0:v][1:v]ssim','-an','-f','null','-'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True,check=True)
            (output/(path.stem+'.ssim.log')).write_text(result.stderr)
            match=re.search(r'All:([0-9.]+)',result.stderr)
            if not match or float(match[1])<.985:raise ValueError('Video SSIM below review threshold')
            if sha(path)!=original:raise ValueError('Source video changed')
            row={'path':(video/path.name).as_posix(),'sourceSha256':original,'outputSha256':sha(partial),'sourceBytes':path.stat().st_size,'outputBytes':partial.stat().st_size,'ssim':float(match[1]),'preserved':{k:a.get(k) for k in KEYS},'sourceCodec':'h264','outputCodec':'hevc hvc1 Main 8-bit','settings':{'crf':16,'preset':'medium','threads':threads},'encoder':version,'seconds':time.monotonic()-started,'deviceDecodeValidated':False}
            partial.replace(target);receipt.write_text(json.dumps(row,indent=2)+'\n');rows.append(row)
            if progress:progress({'stage':'video-check','done':1,'total':1,'file':path.name})
            else:print(json.dumps(row),flush=True)
        except BaseException:
            if partial.exists():partial.unlink()
            raise
    summary={'videos':rows,'sourceBytes':sum(r['sourceBytes'] for r in rows),'outputBytes':sum(r['outputBytes'] for r in rows),'overlay':str(overlay),'unchanged':'All audio and logo.mp4; no dimensions/fps/frame count/timestamps/color metadata changes'}
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');return summary
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--ffmpeg',default='ffmpeg');p.add_argument('--ffprobe',default='ffprobe');p.add_argument('--threads',type=int,default=4);p.add_argument('--namespace',choices=['','ko'],default='')
    a=p.parse_args();print(json.dumps(encode(a.source,a.output,a.ffmpeg,a.ffprobe,a.threads,a.namespace),indent=2))
