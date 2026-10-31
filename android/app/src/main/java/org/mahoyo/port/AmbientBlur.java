package org.mahoyo.port;

import android.opengl.GLES30;
import java.nio.*;
import java.util.*;

/** Area-filtered reduction followed by a dense, separable Gaussian; no spaced full-size taps. */
final class AmbientBlur {
    static final String VERTEX="#version 300 es\nin vec2 aPosition;out vec2 vUv;void main(){vUv=aPosition*.5+.5;gl_Position=vec4(aPosition,0.,1.);}";
    static final String FRAGMENT="#version 300 es\nprecision highp float;uniform sampler2D uImage;uniform vec4 uBox;uniform vec4 uBounds;uniform vec2 uStep;uniform int uReduce;uniform float uWeights[5];in vec2 vUv;out vec4 frag;"+
        "vec4 tap(vec2 p){return texture(uImage,clamp(p,uBounds.xy,uBounds.zw));}"+
        "void main(){vec2 p=mix(uBox.xy,uBox.zw,vUv);if(uReduce!=0){frag=(tap(p+uStep)+tap(p-uStep)+tap(p+vec2(uStep.x,-uStep.y))+tap(p+vec2(-uStep.x,uStep.y)))*.25;}else{frag=tap(p)*uWeights[0];for(int i=1;i<=4;i++)frag+=(tap(p+float(i)*uStep)+tap(p-float(i)*uStep))*uWeights[i];}}";
    static final class Target {
        int texture,framebuffer,width,height;
    }
    private final List<Target> targets=new ArrayList<>();
    private final FloatBuffer quad=ByteBuffer.allocateDirect(8*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
    private int program,position,image,box,bounds,step,reduce,weights;
    Target render(int source,int width,int height,float[] area,float[] frame,int strength){
        int read=integer(GLES30.GL_READ_FRAMEBUFFER_BINDING),write=integer(GLES30.GL_DRAW_FRAMEBUFFER_BINDING),oldProgram=integer(GLES30.GL_CURRENT_PROGRAM),active=integer(GLES30.GL_ACTIVE_TEXTURE);
        int[] viewport=new int[4];GLES30.glGetIntegerv(GLES30.GL_VIEWPORT,viewport,0);
        boolean blend=GLES30.glIsEnabled(GLES30.GL_BLEND),scissor=GLES30.glIsEnabled(GLES30.GL_SCISSOR_TEST);
        try{
            ensure();GLES30.glDisable(GLES30.GL_BLEND);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
            int w=Math.max(1,Math.round((area[2]-area[0])*width)),h=Math.max(1,Math.round((area[3]-area[1])*height));
            int[] size=AmbientEdge.blurSize(w,h,strength);int used=0;Target output;
            float[] sourceBox=area,sourceBounds=frame;
            do{
                int nextW=AmbientEdge.nextSize(w,size[0]),nextH=AmbientEdge.nextSize(h,size[1]);output=target(used++,nextW,nextH);
                pass(source,output,sourceBox,sourceBounds,(sourceBox[2]-sourceBox[0])/(4*nextW),(sourceBox[3]-sourceBox[1])/(4*nextH),true);
                source=output.texture;w=nextW;h=nextH;sourceBox=new float[]{0,0,1,1};sourceBounds=new float[]{.5f/w,.5f/h,1-.5f/w,1-.5f/h};
            }while(w>size[0]||h>size[1]);
            output=target(used++,w,h);pass(source,output,sourceBox,sourceBounds,1f/w,0,false);
            source=output.texture;output=target(used++,w,h);pass(source,output,sourceBox,sourceBounds,0,1f/h,false);
            while(targets.size()>used)release(targets.remove(targets.size()-1));
            return output;
        }finally{
            GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,write);GLES30.glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
            if(blend)GLES30.glEnable(GLES30.GL_BLEND);else GLES30.glDisable(GLES30.GL_BLEND);
            if(scissor)GLES30.glEnable(GLES30.GL_SCISSOR_TEST);else GLES30.glDisable(GLES30.GL_SCISSOR_TEST);
            GLES30.glUseProgram(oldProgram);GLES30.glActiveTexture(active);
        }
    }
    private void pass(int source,Target output,float[] sourceBox,float[] sourceBounds,float dx,float dy,boolean downsample){
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,output.framebuffer);GLES30.glViewport(0,0,output.width,output.height);GLES30.glUseProgram(program);
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,source);GLES30.glUniform1i(image,0);GLES30.glUniform4fv(box,1,sourceBox,0);GLES30.glUniform4fv(bounds,1,sourceBounds,0);GLES30.glUniform2f(step,dx,dy);GLES30.glUniform1i(reduce,downsample?1:0);GLES30.glUniform1fv(weights,5,AmbientEdge.WEIGHTS,0);
        quad.clear();quad.put(new float[]{-1,-1,-1,1,1,-1,1,1}).flip();GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,0,quad);GLES30.glEnableVertexAttribArray(position);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);check("blur pass");
    }
    private Target target(int index,int width,int height){
        if(index==targets.size())targets.add(new Target());Target t=targets.get(index);if(t.texture!=0&&t.width==width&&t.height==height)return t;
        release(t);int[] id=new int[1];
        try{
            GLES30.glGenTextures(1,id,0);t.texture=id[0];GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,t.texture);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,width,height,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);
            GLES30.glGenFramebuffers(1,id,0);t.framebuffer=id[0];GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,t.framebuffer);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,t.texture,0);
            if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("Incomplete ambient framebuffer");check("blur allocation");t.width=width;t.height=height;return t;
        }catch(RuntimeException error){release(t);throw error;}
    }
    void contextLost(){targets.clear();program=0;}
    void release(){for(Target t:targets)release(t);targets.clear();if(program!=0)GLES30.glDeleteProgram(program);program=0;}
    private static void release(Target t){if(t.texture!=0)GLES30.glDeleteTextures(1,new int[]{t.texture},0);if(t.framebuffer!=0)GLES30.glDeleteFramebuffers(1,new int[]{t.framebuffer},0);t.texture=t.framebuffer=t.width=t.height=0;}
    private void ensure(){
        if(program!=0)return;int v=0,f=0;
        try{
            v=shader(GLES30.GL_VERTEX_SHADER,VERTEX);f=shader(GLES30.GL_FRAGMENT_SHADER,FRAGMENT);program=GLES30.glCreateProgram();GLES30.glAttachShader(program,v);GLES30.glAttachShader(program,f);GLES30.glLinkProgram(program);int[] ok=new int[1];GLES30.glGetProgramiv(program,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetProgramInfoLog(program));
            position=GLES30.glGetAttribLocation(program,"aPosition");image=uniform("uImage");box=uniform("uBox");bounds=uniform("uBounds");step=uniform("uStep");reduce=uniform("uReduce");weights=uniform("uWeights[0]");
        }catch(RuntimeException error){release();throw error;}finally{if(v!=0)GLES30.glDeleteShader(v);if(f!=0)GLES30.glDeleteShader(f);}
    }
    private int uniform(String name){return GLES30.glGetUniformLocation(program,name);}
    private static int shader(int type,String source){int id=GLES30.glCreateShader(type);GLES30.glShaderSource(id,source);GLES30.glCompileShader(id);int[] ok=new int[1];GLES30.glGetShaderiv(id,GLES30.GL_COMPILE_STATUS,ok,0);if(ok[0]==0){String error=GLES30.glGetShaderInfoLog(id);GLES30.glDeleteShader(id);throw new IllegalStateException(error);}return id;}
    private static int integer(int name){int[] value=new int[1];GLES30.glGetIntegerv(name,value,0);return value[0];}
    private static void check(String stage){int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException(stage+" GL0x"+Integer.toHexString(error));}
}
