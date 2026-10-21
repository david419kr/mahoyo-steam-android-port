package org.mahoyo.port;

import android.opengl.GLES30;
import java.nio.*;
import java.util.*;

/** One isolated RGBA8 native canvas per live wave layer. */
final class WaveRenderer {
    interface Draw {void draw(FloatBuffer vertices,int count);}
    private static final class Frame {
        int texture,framebuffer;String key="";FloatBuffer vertices;
    }
    private final Map<Integer,Frame> frames=new HashMap<>();
    private volatile int allocatedFboCount;
    private volatile long drawCount;
    private volatile String lastError="";
    String debugState(){return "waveFbos="+allocatedFboCount+" waveDraws="+drawCount+" waveLastError="+lastError;}
    void noteError(String value){lastError=value==null?"unknown":value;}
    int render(int id,String key,int width,int height,WaveMesh.Spec spec,float[] affine,float u0,float v0,float u1,float v1,Draw draw){
        if(id<0||id>=64)throw new IllegalArgumentException("Invalid wave layer ID");int floats=WaveMesh.cells(width,height,spec)*24;Frame frame=frames.get(id);if(frame==null){frame=new Frame();frames.put(id,frame);}
        if(frame.texture!=0&&key.equals(frame.key))return frame.texture;
        int read=integer(GLES30.GL_READ_FRAMEBUFFER_BINDING),write=integer(GLES30.GL_DRAW_FRAMEBUFFER_BINDING);int[] viewport=new int[4];GLES30.glGetIntegerv(GLES30.GL_VIEWPORT,viewport,0);boolean scissor=GLES30.glIsEnabled(GLES30.GL_SCISSOR_TEST),blend=GLES30.glIsEnabled(GLES30.GL_BLEND);
        try{
            if(frame.texture==0){allocate(frame);allocatedFboCount++;}
            // The existing canvas is overwritten below. A failed update must
            // not retain its old cache key and expose damaged pixels later.
            frame.key="";
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,frame.framebuffer);GLES30.glViewport(0,0,WaveMesh.WIDTH,WaveMesh.HEIGHT);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glDisable(GLES30.GL_BLEND);GLES30.glClearColor(0,0,0,0);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
            if(floats/24>=1000){GLES30.glEnable(GLES30.GL_BLEND);GLES30.glBlendEquation(GLES30.GL_FUNC_ADD);GLES30.glBlendFunc(GLES30.GL_SRC_ALPHA,GLES30.GL_ONE_MINUS_SRC_ALPHA);}
            if(frame.vertices==null||frame.vertices.capacity()<floats)frame.vertices=ByteBuffer.allocateDirect(floats*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            int count=WaveMesh.write(frame.vertices,width,height,spec,affine,u0,v0,u1,v1);draw.draw(frame.vertices,count);check("wave mesh draw");drawCount++;frame.key=key;return frame.texture;
        }catch(RuntimeException error){noteError(error.getMessage());throw error;}
        finally{GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,write);GLES30.glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(scissor)GLES30.glEnable(GLES30.GL_SCISSOR_TEST);else GLES30.glDisable(GLES30.GL_SCISSOR_TEST);if(blend)GLES30.glEnable(GLES30.GL_BLEND);else GLES30.glDisable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);}
    }
    void retain(Set<Integer> live){for(Iterator<Map.Entry<Integer,Frame>> i=frames.entrySet().iterator();i.hasNext();){Map.Entry<Integer,Frame> item=i.next();if(!live.contains(item.getKey())){releaseFrame(item.getValue());i.remove();}}}
    void release(){for(Frame frame:frames.values())releaseFrame(frame);frames.clear();}
    void contextLost(){frames.clear();allocatedFboCount=0;lastError="";}
    private void releaseFrame(Frame frame){if(frame.texture!=0)allocatedFboCount--;release(frame);}
    private static void allocate(Frame frame){
        int[] names=new int[1];try{GLES30.glGenTextures(1,names,0);frame.texture=names[0];GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,frame.texture);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,WaveMesh.WIDTH,WaveMesh.HEIGHT,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLES30.glGenFramebuffers(1,names,0);frame.framebuffer=names[0];GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,frame.framebuffer);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,frame.texture,0);if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("Incomplete wave canvas");check("wave canvas allocation");}catch(RuntimeException error){release(frame);throw error;}
    }
    private static void release(Frame frame){if(frame.texture!=0)GLES30.glDeleteTextures(1,new int[]{frame.texture},0);if(frame.framebuffer!=0)GLES30.glDeleteFramebuffers(1,new int[]{frame.framebuffer},0);frame.texture=frame.framebuffer=0;frame.key="";}
    private static int integer(int name){int[] value=new int[1];GLES30.glGetIntegerv(name,value,0);return value[0];}
    private static void check(String where){int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException(where+" GL0x"+Integer.toHexString(error));}
}
