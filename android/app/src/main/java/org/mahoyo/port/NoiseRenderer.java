package org.mahoyo.port;

import android.opengl.GLES30;
import java.util.*;
import java.util.function.Supplier;

/** CPU frozen frames survive context loss; GPU names do not. */
final class NoiseRenderer<T> {
    static final int WIDTH=1920,HEIGHT=1080;
    interface Draw<T>{void draw(NoiseClock.Frame<T> frame);}
    private static final class Canvas {int texture,framebuffer;String key="";}
    private final Map<String,NoiseClock<T>> clocks=new HashMap<>();
    private final Map<String,Canvas> canvases=new HashMap<>();
    private final Set<String> restorePending=new HashSet<>();
    private volatile int allocatedFboCount,cpuFrameCount;private volatile long drawCount;private volatile String lastError="",clockDebug="";
    NoiseClock.Frame<T> sample(NoiseClock.Spec spec,Supplier<T> source){NoiseClock<T> clock=clocks.get(spec.token);if(clock==null){clock=new NoiseClock<>();clocks.put(spec.token,clock);}NoiseClock.Frame<T> previous=clock.current();if(previous!=null&&(previous.spec.selector!=spec.selector||previous.spec.periodMs!=spec.periodMs))throw new IllegalArgumentException("Noise token reused for a different resource/period");NoiseClock.Frame<T> frame=restorePending.contains(spec.token)?previous:clock.sample(spec,source);cpuFrameCount=clocks.size();clockDebug=" noiseToken="+spec.token+" noiseSerial="+frame.serial+" noiseFlip="+frame.orientation+" noiseClock="+spec.clockMs;return frame;}
    NoiseClock.Frame<T> current(String token){NoiseClock<T> clock=clocks.get(token);return clock==null?null:clock.current();}
    int render(NoiseClock.Frame<T> frame,Draw<T> draw){
        Canvas canvas=canvases.get(frame.spec.token);if(canvas==null){canvas=new Canvas();canvases.put(frame.spec.token,canvas);}
        if(canvas.texture!=0&&canvas.key.equals(frame.key()))return canvas.texture;
        int read=integer(GLES30.GL_READ_FRAMEBUFFER_BINDING),write=integer(GLES30.GL_DRAW_FRAMEBUFFER_BINDING);int[] viewport=new int[4];GLES30.glGetIntegerv(GLES30.GL_VIEWPORT,viewport,0);boolean scissor=GLES30.glIsEnabled(GLES30.GL_SCISSOR_TEST),blend=GLES30.glIsEnabled(GLES30.GL_BLEND);
        try{
            if(canvas.texture==0){allocate(canvas);allocatedFboCount++;}
            canvas.key="";
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,canvas.framebuffer);GLES30.glViewport(0,0,WIDTH,HEIGHT);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glDisable(GLES30.GL_BLEND);GLES30.glClearColor(0,0,0,0);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
            draw.draw(frame);check("noise prepass");canvas.key=frame.key();drawCount++;NoiseClock<T> clock=clocks.get(frame.spec.token);if(clock!=null&&clock.current()==frame)restorePending.remove(frame.spec.token);return canvas.texture;
        }catch(RuntimeException error){lastError=String.valueOf(error.getMessage());throw error;}
        finally{GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,write);GLES30.glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(scissor)GLES30.glEnable(GLES30.GL_SCISSOR_TEST);else GLES30.glDisable(GLES30.GL_SCISSOR_TEST);if(blend)GLES30.glEnable(GLES30.GL_BLEND);else GLES30.glDisable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);}
    }
    void retain(Set<String> tokens){clocks.keySet().retainAll(tokens);restorePending.retainAll(tokens);cpuFrameCount=clocks.size();if(clocks.isEmpty())clockDebug="";for(Iterator<Map.Entry<String,Canvas>> i=canvases.entrySet().iterator();i.hasNext();){Map.Entry<String,Canvas> item=i.next();if(!tokens.contains(item.getKey())){if(item.getValue().texture!=0)allocatedFboCount--;release(item.getValue());i.remove();}}}
    void contextLost(){canvases.clear();restorePending.clear();for(Map.Entry<String,NoiseClock<T>> entry:clocks.entrySet())if(entry.getValue().current()!=null)restorePending.add(entry.getKey());allocatedFboCount=0;lastError="";}
    void release(){for(Canvas canvas:canvases.values())release(canvas);canvases.clear();clocks.clear();restorePending.clear();allocatedFboCount=cpuFrameCount=0;clockDebug="";}
    String debugState(){return "noiseFbos="+allocatedFboCount+" noiseDraws="+drawCount+" noiseCpuFrames="+cpuFrameCount+clockDebug+" noiseLastError="+lastError;}
    private static void allocate(Canvas canvas){int[] names=new int[1];try{GLES30.glGenTextures(1,names,0);canvas.texture=names[0];GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,canvas.texture);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,WIDTH,HEIGHT,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLES30.glGenFramebuffers(1,names,0);canvas.framebuffer=names[0];GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,canvas.framebuffer);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,canvas.texture,0);if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("Incomplete noise canvas");check("noise allocation");}catch(RuntimeException error){release(canvas);throw error;}}
    private static void release(Canvas canvas){if(canvas.texture!=0)GLES30.glDeleteTextures(1,new int[]{canvas.texture},0);if(canvas.framebuffer!=0)GLES30.glDeleteFramebuffers(1,new int[]{canvas.framebuffer},0);canvas.texture=canvas.framebuffer=0;canvas.key="";}
    private static int integer(int name){int[] v=new int[1];GLES30.glGetIntegerv(name,v,0);return v[0];}
    private static void check(String where){int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException(where+" GL0x"+Integer.toHexString(error));}
}
