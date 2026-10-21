package org.mahoyo.port;

import android.opengl.GLES30;
import java.nio.*;
import java.util.*;

/** Two persistent targets per native job, with a fresh opaque snapshot each callback. */
final class GlobalScreenWaveRenderer {
    interface Capture {void draw();}
    interface Warp {void draw(int snapshot,FloatBuffer vertices,int count);}
    private static final class Frame {final int[] textures=new int[2],framebuffers=new int[2];FloatBuffer vertices;}
    private final Map<String,Frame> frames=new HashMap<>();
    private volatile long captures,warps;
    private volatile int allocated;
    private volatile String lastError="";
    String debugState(){return "screenWaveFbos="+allocated+" screenWaveCaptures="+captures+" screenWaveWarps="+warps+" screenWaveLastError="+lastError;}
    int render(GlobalScreenWave.Spec spec,Capture capture,Warp warp){
        Frame frame=frames.get(spec.id);if(frame==null){frame=new Frame();frames.put(spec.id,frame);}
        int read=integer(GLES30.GL_READ_FRAMEBUFFER_BINDING),write=integer(GLES30.GL_DRAW_FRAMEBUFFER_BINDING);int[] viewport=new int[4];GLES30.glGetIntegerv(GLES30.GL_VIEWPORT,viewport,0);boolean scissor=GLES30.glIsEnabled(GLES30.GL_SCISSOR_TEST),blend=GLES30.glIsEnabled(GLES30.GL_BLEND);
        try{
            if(frame.textures[0]==0){allocate(frame);allocated+=2;}
            // Native job+d8 is not a cached-snapshot indicator, even at strength1.
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,frame.framebuffers[0]);GLES30.glViewport(0,0,GlobalScreenWave.WIDTH,GlobalScreenWave.HEIGHT);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glEnable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);GLES30.glClearColor(0,0,0,1);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);capture.draw();check("global Screen Raster capture");captures++;
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,frame.framebuffers[1]);GLES30.glViewport(0,0,GlobalScreenWave.WIDTH,GlobalScreenWave.HEIGHT);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glDisable(GLES30.GL_BLEND);GLES30.glClearColor(0,0,0,0);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
            if(frame.vertices==null)frame.vertices=ByteBuffer.allocateDirect(GlobalScreenWave.CELLS*24*4).order(ByteOrder.nativeOrder()).asFloatBuffer();int count=GlobalScreenWave.write(frame.vertices,spec);warp.draw(frame.textures[0],frame.vertices,count);check("global Screen Raster mesh");warps++;return frame.textures[1];
        }catch(RuntimeException error){lastError=String.valueOf(error.getMessage());throw error;}
        finally{GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,write);GLES30.glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(scissor)GLES30.glEnable(GLES30.GL_SCISSOR_TEST);else GLES30.glDisable(GLES30.GL_SCISSOR_TEST);if(blend)GLES30.glEnable(GLES30.GL_BLEND);else GLES30.glDisable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);}
    }
    void retain(Set<String> live){for(Iterator<Map.Entry<String,Frame>>it=frames.entrySet().iterator();it.hasNext();){Map.Entry<String,Frame> item=it.next();if(!live.contains(item.getKey())){releaseFrame(item.getValue());it.remove();}}}
    void release(){for(Frame frame:frames.values())releaseFrame(frame);frames.clear();}
    void contextLost(){frames.clear();allocated=0;lastError="";}
    private void releaseFrame(Frame frame){if(frame.textures[0]!=0)allocated-=2;release(frame);}
    private static void allocate(Frame frame){
        try{GLES30.glGenTextures(2,frame.textures,0);GLES30.glGenFramebuffers(2,frame.framebuffers,0);for(int i=0;i<2;i++){GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,frame.textures[i]);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,GlobalScreenWave.WIDTH,GlobalScreenWave.HEIGHT,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,frame.framebuffers[i]);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,frame.textures[i],0);if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("Incomplete global Screen Raster target");}check("global Screen Raster allocation");}catch(RuntimeException error){release(frame);throw error;}
    }
    private static void release(Frame frame){GLES30.glDeleteTextures(2,frame.textures,0);GLES30.glDeleteFramebuffers(2,frame.framebuffers,0);Arrays.fill(frame.textures,0);Arrays.fill(frame.framebuffers,0);}
    private static int integer(int name){int[]v=new int[1];GLES30.glGetIntegerv(name,v,0);return v[0];}
    private static void check(String stage){int e=GLES30.glGetError();if(e!=GLES30.GL_NO_ERROR)throw new IllegalStateException(stage+" GL0x"+Integer.toHexString(e));}
}
