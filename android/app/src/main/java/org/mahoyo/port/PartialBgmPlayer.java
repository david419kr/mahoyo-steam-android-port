package org.mahoyo.port;

import android.media.*;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import org.json.JSONObject;
import java.io.File;
import java.nio.ByteBuffer;
import java.util.Set;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BiConsumer;
import java.util.zip.CRC32;

/** Vorbis intro once, then exact source-frame PCM loops without codec seeks. */
final class PartialBgmPlayer {
    static final class LoopSpec {
        static final int MAX_PCM_BYTES=48*1024*1024;
        final long sampleCount;
        final int sampleRate,channels,startFrame,endFrame,loopBytes;
        LoopSpec(long count,int rate,int channels,long start,long end){
            if(count<=0||count>Integer.MAX_VALUE||rate<8000||rate>192000||(channels!=1&&channels!=2))throw new IllegalArgumentException("invalid-loop-format");
            if(end==0||end>=count)end=count;
            if(start<0||start>=end)throw new IllegalArgumentException("invalid-loop-bounds");
            long bytes=(end-start)*channels*2L;if(bytes<=0||bytes>MAX_PCM_BYTES)throw new IllegalArgumentException("partial-loop-pcm-cap");
            sampleCount=count;sampleRate=rate;this.channels=channels;startFrame=(int)start;endFrame=(int)end;loopBytes=(int)bytes;
        }
        static LoopSpec fromJson(JSONObject value){return new LoopSpec(integer(value,"sampleCount"),(int)integer(value,"sampleRate"),(int)integer(value,"channels"),integer(value,"startFrame"),integer(value,"endFrame"));}
        private static long integer(JSONObject value,String key){double v=value.optDouble(key,Double.NaN);if(!Double.isFinite(v)||v!=Math.floor(v)||v<0||v>Integer.MAX_VALUE)throw new IllegalArgumentException("invalid-loop-"+key);return (long)v;}
        int frameBytes(){return channels*2;}
        void checkFormat(int rate,int count,int encoding){if(rate!=sampleRate||count!=channels||encoding!=AudioFormat.ENCODING_PCM_16BIT)throw new IllegalArgumentException("decoded-format-mismatch");}
        long sourceFrame(long head){return head<endFrame?head:startFrame+(head-endFrame)%(endFrame-startFrame);}
    }
    /** Buffer boundaries can cross S and E; every offset remains whole-frame. */
    static final class PcmWindow {
        final LoopSpec spec;long decodedFrames,usedFrames;
        PcmWindow(LoopSpec spec){this.spec=spec;}
        int accept(int bytes){if(bytes<0||bytes%spec.frameBytes()!=0)throw new IllegalArgumentException("unaligned-decoded-pcm");long n=bytes/spec.frameBytes(),before=decodedFrames;decodedFrames+=n;if(decodedFrames>spec.sampleCount)throw new IllegalArgumentException("decoded-past-sample-count");int used=(int)Math.max(0,Math.min(n,spec.endFrame-before));usedFrames+=used;return used*spec.frameBytes();}
        int cacheOffset(long before,int usedBytes){return (int)Math.min(usedBytes,Math.max(0,spec.startFrame-before)*spec.frameBytes());}
        int cacheBytes(long before,int usedBytes){return usedBytes-cacheOffset(before,usedBytes);}
    }
    static final class CacheBudget {
        static final int MAX_BYTES=96*1024*1024;
        private static final AtomicInteger used=new AtomicInteger();
        static void reserve(int bytes){for(;;){int before=used.get();if(bytes<=0||bytes>LoopSpec.MAX_PCM_BYTES||bytes>MAX_BYTES-before)throw new IllegalStateException("partial-audio-cache-budget");if(used.compareAndSet(before,before+bytes))return;}}
        static void release(int bytes){used.addAndGet(-bytes);}
        static int bytes(){return used.get();}
    }
    private static final class Session {
        final Object lock=new Object();final String id;final GameAsset file;final LoopSpec spec;final long began=SystemClock.elapsedRealtime();
        volatile boolean canceled,failed;
        volatile long decodedFrames,usedFrames,queuedFrames,decodeMs,firstPcmMs=-1,firstPlayMs=-1;
        volatile long cacheCrc32=-1,queuedLoopFrames,firstLoopCrc32=-1,firstLoopCrcFrames;
        final CRC32 firstLoopCrc=new CRC32();
        volatile int cachedBytes,ringBytes,threadPriority;
        volatile String phase="decoding",codec="",padding="",failure="";
        volatile AudioTrack track;
        Future<?> task;boolean readyPosted,readyDelivered,allowed;int controlSerial;
        float left=1,right=1,appliedLeft=Float.NaN,appliedRight=Float.NaN;
        Session(String id,GameAsset file,LoopSpec spec){this.id=id;this.file=file;this.spec=spec;}
    }
    /** A retiring stream must retain its producer as well as its AudioTrack. */
    final class Playback {
        private final Session session;private Playback(Session value){session=value;}
        void setVolume(float left,float right){volume(session,left,right);}
        void setPlaybackAllowed(boolean value){allow(session,value);}
        boolean active(){return !session.canceled&&!session.failed&&session.track!=null;}
        void stop(){stopSession(session);}
    }
    private final Handler main=new Handler(Looper.getMainLooper());
    private final BiConsumer<String,String> onError;
    //Incoming+retiring only. A third producer waits for a canceled worker to exit.
    private final ThreadPoolExecutor workers=new ThreadPoolExecutor(2,2,10,TimeUnit.SECONDS,new ArrayBlockingQueue<>(2),r->{Thread t=new Thread(r,"WoH.AudioStream");t.setDaemon(true);return t;});
    private final Set<Session> sessions=ConcurrentHashMap.newKeySet();
    private volatile Session current;private Session last;private volatile boolean closed;
    private boolean allowed;private float left=1,right=1;
    PartialBgmPlayer(BiConsumer<String,String> onError){this.onError=onError;workers.allowCoreThreadTimeOut(true);}
    void start(String id,File file,LoopSpec spec){start(id,GameAsset.loose(file),spec);}
    void start(String id,GameAsset file,LoopSpec spec){if(closed||(current!=null&&current.id.equals(id)))return;stop();Session s=new Session(id,file,spec);s.allowed=allowed;s.left=left;s.right=right;current=last=s;sessions.add(s);s.task=workers.submit(()->produce(s));}
    void setPlaybackAllowed(boolean value){allowed=value;Session s=current;if(s!=null)allow(s,value);}
    void setVolume(float left,float right){this.left=unit(left);this.right=unit(right);Session s=current;if(s!=null)volume(s,left,right);}
    private static float unit(float value){return Float.isFinite(value)?Math.max(0,Math.min(1,value)):0;}
    Playback detachPlayback(){Session s=current;current=null;if(s==null)return null;synchronized(s.lock){if(s.readyDelivered&&!s.canceled&&!s.failed&&s.track!=null)return new Playback(s);}stopSession(s);return null;}
    void stop(){Session s=current;current=null;if(s!=null)stopSession(s);}
    void close(){if(closed)return;closed=true;current=null;for(Session s:sessions.toArray(new Session[0]))stopSession(s);workers.shutdown();}
    private void requireActive(Session s){if(closed||s.canceled||Thread.currentThread().isInterrupted())throw new CancellationException();}
    private void stopSession(Session s){
        synchronized(s.lock){if(s.canceled)return;s.canceled=true;if(!s.failed)s.phase="stopped";
            //Break blocking write without freeing memory under the writer.
            if(s.track!=null)try{s.track.pause();s.track.flush();}catch(RuntimeException ignored){}s.lock.notifyAll();}
        if(s.task!=null)s.task.cancel(true);sessions.remove(s);workers.purge();
    }
    private void volume(Session s,float left,float right){synchronized(s.lock){s.left=unit(left);s.right=unit(right);apply(s);}}
    private void allow(Session s,boolean value){synchronized(s.lock){if(s.allowed!=value)s.controlSerial++;s.allowed=value;apply(s);s.lock.notifyAll();}}
    private void apply(Session s){
        if(!s.readyDelivered||s.track==null||s.canceled||s.failed)return;
        try{if(s.left!=s.appliedLeft||s.right!=s.appliedRight){if(s.track.setStereoVolume(s.left,s.right)!=AudioTrack.SUCCESS)throw new IllegalStateException("stream-volume-failed");s.appliedLeft=s.left;s.appliedRight=s.right;}
            if(s.allowed){if(s.track.getPlayState()!=AudioTrack.PLAYSTATE_PLAYING)s.track.play();s.phase="playing";if(s.firstPlayMs<0)s.firstPlayMs=SystemClock.elapsedRealtime()-s.began;}
            else{if(s.track.getPlayState()==AudioTrack.PLAYSTATE_PLAYING)s.track.pause();s.phase="paused";}
        }catch(RuntimeException e){fail(s,reason(e));}
    }
    private boolean awaitPlaying(Session s)throws InterruptedException {boolean waited=false;synchronized(s.lock){while(!s.canceled&&!closed&&(!s.readyDelivered||!s.allowed)){waited=true;s.lock.wait();}}requireActive(s);return waited;}
    private static int optionalInt(MediaFormat f,String n){return f.containsKey(n)?f.getInteger(n):0;}
    private void checkOutput(Session s,MediaFormat f){s.spec.checkFormat(f.getInteger(MediaFormat.KEY_SAMPLE_RATE),f.getInteger(MediaFormat.KEY_CHANNEL_COUNT),f.containsKey(MediaFormat.KEY_PCM_ENCODING)?f.getInteger(MediaFormat.KEY_PCM_ENCODING):AudioFormat.ENCODING_PCM_16BIT);}
    private void createTrack(Session s){
        LoopSpec spec=s.spec;int mask=spec.channels==2?AudioFormat.CHANNEL_OUT_STEREO:AudioFormat.CHANNEL_OUT_MONO,min=AudioTrack.getMinBufferSize(spec.sampleRate,mask,AudioFormat.ENCODING_PCM_16BIT);if(min<=0)throw new IllegalStateException("unsupported-stream-format");
        // CG decoding/GC can outlast a low-latency ring; volume/pause remain immediate.
        int bytes=Math.max(min,spec.sampleRate*spec.frameBytes()*350/1000);bytes+=(-bytes)&(spec.frameBytes()-1);if(bytes>1024*1024)throw new IllegalStateException("unexpected-stream-buffer-size");
        AudioTrack t=new AudioTrack.Builder().setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setAudioFormat(new AudioFormat.Builder().setSampleRate(spec.sampleRate).setChannelMask(mask).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build()).setBufferSizeInBytes(bytes).setTransferMode(AudioTrack.MODE_STREAM).setSessionId(AudioManager.AUDIO_SESSION_ID_GENERATE).build();
        synchronized(s.lock){s.track=t;s.ringBytes=bytes;}if(t.getState()!=AudioTrack.STATE_INITIALIZED)throw new IllegalStateException("uninitialized-stream-track");
    }
    private void write(Session s,ByteBuffer source)throws InterruptedException {
        int blockBytes=Math.min(512*s.spec.frameBytes(),s.ringBytes);
        while(source.hasRemaining()){
            requireActive(s);boolean first=!s.readyPosted;if(!first)awaitPlaying(s);int count=Math.min(blockBytes,source.remaining()),limit=source.limit(),serial;synchronized(s.lock){serial=s.controlSerial;}
            int beforePosition=source.position();source.limit(beforePosition+count);int actual;try{actual=s.track.write(source,count,AudioTrack.WRITE_BLOCKING);}finally{source.limit(limit);}requireActive(s);
            if(actual<0||actual>count||actual%s.spec.frameBytes()!=0)throw new IllegalStateException("invalid-stream-write-"+actual);
            if(actual==0){if(first)throw new IllegalStateException("zero-stream-prefill");synchronized(s.lock){if(serial==s.controlSerial&&s.allowed)throw new IllegalStateException("zero-stream-write");}continue;}
            if(s.queuedFrames>=s.spec.endFrame&&s.firstLoopCrcFrames<2048){int frames=(int)Math.min(actual/s.spec.frameBytes(),2048-s.firstLoopCrcFrames);ByteBuffer bytes=source.duplicate();bytes.position(beforePosition);bytes.limit(beforePosition+frames*s.spec.frameBytes());s.firstLoopCrc.update(bytes);s.firstLoopCrc32=s.firstLoopCrc.getValue();s.firstLoopCrcFrames+=frames;}
            s.queuedFrames+=actual/s.spec.frameBytes();s.queuedLoopFrames=Math.max(0,s.queuedFrames-s.spec.endFrame);
            long prefillFrames=Math.min(s.spec.endFrame,Math.min(s.ringBytes/s.spec.frameBytes(),(s.spec.sampleRate*80L+999)/1000));
            if(first&&s.queuedFrames>=prefillFrames){s.readyPosted=true;s.decodeMs=SystemClock.elapsedRealtime()-s.began;s.phase="prepared";if(!main.post(()->prepared(s)))throw new CancellationException();}
        }
    }
    private void prepared(Session s){
        if(closed||s.canceled||current!=s||s.failed){stopSession(s);return;}
        synchronized(s.lock){if(s.track==null)return;try{if(Build.VERSION.SDK_INT>=31&&s.track.setStartThresholdInFrames((int)s.queuedFrames)<=0)throw new IllegalStateException("stream-start-threshold-failed");}catch(RuntimeException e){fail(s,reason(e));return;}s.readyDelivered=true;apply(s);s.lock.notifyAll();if(s.failed||s.canceled)return;}
        android.util.Log.i("WoH.Audio","Partial BGM ready mode=stream id="+s.id+" firstPcmMs="+s.firstPcmMs+" firstPlayMs="+s.firstPlayMs+" rate="+s.spec.sampleRate+" ringBytes="+s.ringBytes+" loop=["+s.spec.startFrame+","+s.spec.endFrame+") codec="+s.codec+" "+s.padding);
    }
    private void produce(Session s){
        MediaExtractor extractor=null;MediaCodec codec=null;ByteBuffer cache=null;int reserved=0;CRC32 cacheCrc=new CRC32();PcmWindow window=new PcmWindow(s.spec);long lastOutput=SystemClock.elapsedRealtime();
        try{
            try{android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);}catch(SecurityException e){android.util.Log.w("WoH.Audio","Audio thread priority unavailable",e);}
            s.threadPriority=android.os.Process.getThreadPriority(android.os.Process.myTid());
            requireActive(s);extractor=new MediaExtractor();s.file.source(extractor);MediaFormat format=null;
            for(int i=0;i<extractor.getTrackCount();i++){MediaFormat f=extractor.getTrackFormat(i);String mime=f.getString(MediaFormat.KEY_MIME);if(mime!=null&&mime.startsWith("audio/")){format=f;extractor.selectTrack(i);break;}}
            if(format==null||!"audio/vorbis".equals(format.getString(MediaFormat.KEY_MIME)))throw new IllegalArgumentException("partial-bgm-requires-vorbis");
            s.spec.checkFormat(format.getInteger(MediaFormat.KEY_SAMPLE_RATE),format.getInteger(MediaFormat.KEY_CHANNEL_COUNT),AudioFormat.ENCODING_PCM_16BIT);s.padding="delay="+optionalInt(format,"encoder-delay")+",padding="+optionalInt(format,"encoder-padding");format.setInteger(MediaFormat.KEY_PCM_ENCODING,AudioFormat.ENCODING_PCM_16BIT);
            codec=MediaCodec.createDecoderByType(format.getString(MediaFormat.KEY_MIME));s.codec=codec.getName();codec.configure(format,null,null,0);codec.start();boolean inputEnd=false,checked=false;MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();
            while(window.usedFrames<s.spec.endFrame){
                requireActive(s);if(s.readyPosted&&awaitPlaying(s))lastOutput=SystemClock.elapsedRealtime();
                if(!inputEnd){int index=codec.dequeueInputBuffer(10000);if(index>=0){ByteBuffer input=codec.getInputBuffer(index);if(input==null)throw new IllegalStateException("missing-codec-input");input.clear();int bytes=extractor.readSampleData(input,0);if(bytes<0){codec.queueInputBuffer(index,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM);inputEnd=true;}else{if((extractor.getSampleFlags()&MediaExtractor.SAMPLE_FLAG_ENCRYPTED)!=0)throw new IllegalArgumentException("encrypted-partial-bgm");codec.queueInputBuffer(index,0,bytes,extractor.getSampleTime(),0);extractor.advance();}}}
                int index=codec.dequeueOutputBuffer(info,10000);
                if(index==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){checkOutput(s,codec.getOutputFormat());checked=true;}
                else if(index>=0){try{
                    if(!checked){checkOutput(s,codec.getOutputFormat());checked=true;}
                    if(info.size<0)throw new IllegalStateException("invalid-codec-output-range");
                    if(info.size>0&&(info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)==0){
                        if(s.firstPcmMs<0)s.firstPcmMs=SystemClock.elapsedRealtime()-s.began;
                        ByteBuffer output=codec.getOutputBuffer(index);if(output==null||info.offset<0||info.offset>output.capacity()-info.size)throw new IllegalStateException("invalid-codec-output-range");long before=window.decodedFrames;int used=window.accept(info.size);s.decodedFrames=window.decodedFrames;s.usedFrames=window.usedFrames;
                        int cached=window.cacheBytes(before,used);if(cached>0){if(cache==null){CacheBudget.reserve(s.spec.loopBytes);reserved=s.spec.loopBytes;cache=ByteBuffer.allocateDirect(reserved);}ByteBuffer part=output.duplicate();part.clear();part.position(info.offset+window.cacheOffset(before,used));part.limit(part.position()+cached);cacheCrc.update(part.duplicate());cache.put(part);s.cachedBytes=cache.position();}
                        if(used>0){if(s.track==null)createTrack(s);ByteBuffer part=output.duplicate();part.clear();part.position(info.offset);part.limit(info.offset+used);write(s,part);}lastOutput=SystemClock.elapsedRealtime();
                    }
                    if((info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0&&window.usedFrames<s.spec.endFrame)throw new IllegalStateException("decoded-eof-before-loop-end");
                }finally{codec.releaseOutputBuffer(index,false);}}
                if(SystemClock.elapsedRealtime()-lastOutput>10000)throw new IllegalStateException("stream-pcm-timeout");
            }
            if(cache==null||cache.position()!=s.spec.loopBytes)throw new IllegalStateException("incomplete-loop-cache");s.cacheCrc32=cacheCrc.getValue();codec.release();codec=null;extractor.release();extractor=null;cache.flip();android.util.Log.i("WoH.Audio","Partial BGM loop cached id="+s.id+" usedFrames="+s.usedFrames+" loopBytes="+s.cachedBytes+" crc32="+s.cacheCrc32);
            //Batch even a one-frame loop, keeping exactly the original PCM order.
            ByteBuffer block=ByteBuffer.allocateDirect(512*s.spec.frameBytes());int cursor=0;
            while(true){awaitPlaying(s);block.clear();while(block.hasRemaining()){int n=Math.min(block.remaining(),cache.limit()-cursor);ByteBuffer part=cache.duplicate();part.position(cursor);part.limit(cursor+n);block.put(part);cursor=(cursor+n)%cache.limit();}block.flip();write(s,block);}
        }catch(CancellationException ignored){
        }catch(InterruptedException ignored){Thread.currentThread().interrupt();
        }catch(Exception|OutOfMemoryError e){if(!s.canceled&&!closed){s.failure=reason(e);android.util.Log.w("WoH.Audio","Partial BGM "+s.id+": "+s.failure,e);main.post(()->fail(s,s.failure));}}
        finally{
            synchronized(s.lock){if(s.track!=null){try{s.track.release();}catch(RuntimeException ignored){}s.track=null;}if(s.canceled&&!s.failed)s.phase="stopped";s.lock.notifyAll();}
            if(codec!=null)try{codec.release();}catch(RuntimeException ignored){}if(extractor!=null)try{extractor.release();}catch(RuntimeException ignored){}if(reserved>0)CacheBudget.release(reserved);sessions.remove(s);
        }
    }
    private void fail(Session s,String reason){synchronized(s.lock){if(closed||s.canceled||s.failed)return;s.failed=true;s.failure=reason;s.phase="error";}stopSession(s);if(current==s&&!closed)onError.accept(s.id,reason);}
    static String reason(Throwable e){String value=e.getMessage();if(value==null||value.isEmpty())value=e.getClass().getSimpleName();value=value.replace('\n',' ').replace('\r',' ');return value.substring(0,Math.min(160,value.length()));}
    String debugState(){
        Session s=current!=null?current:last;if(s==null)return "partialBgm=idle";String head="";synchronized(s.lock){try{if(s.track!=null){long h=Integer.toUnsignedLong(s.track.getPlaybackHeadPosition());head=" headFrames="+h+" expectedSourceFrame="+s.spec.sourceFrame(h)+" underruns="+s.track.getUnderrunCount();}}catch(RuntimeException ignored){head=" headFrames=unavailable";}}
        return "partialBgm="+s.phase+" mode=stream id="+s.id+" decodedFrames="+s.decodedFrames+" usedFrames="+s.usedFrames+" queuedFrames="+s.queuedFrames+" queuedLoopFrames="+s.queuedLoopFrames+" loopCachedBytes="+s.cachedBytes+" loopBytes="+s.spec.loopBytes+" globalCacheBytes="+CacheBudget.bytes()+" cacheCrc32="+s.cacheCrc32+" firstLoopCrc32="+s.firstLoopCrc32+" firstLoopCrcFrames="+s.firstLoopCrcFrames+" ringBytes="+s.ringBytes+" threadPriority="+s.threadPriority+" firstPcmMs="+s.firstPcmMs+" firstPlayMs="+s.firstPlayMs+" prepareMs="+s.decodeMs+" codec="+s.codec+" "+s.padding+head+(s.failure.isEmpty()?"":" error="+s.failure);
    }
}
