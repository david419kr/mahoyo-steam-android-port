package org.mahoyo.port;
import android.opengl.GLES30;
import java.util.*;

/** One transient RGBA mask7 canvas per reviewed preparation token. */
final class LayerAlphaRuleRenderer {
    interface Draw { void draw(int program); }
    private static final class Canvas { int texture,framebuffer;String key=""; }
    private final Map<String,Canvas> canvases=new HashMap<>();
    private int program;
    private long draws;
    static final String FRAGMENT="#version 300 es\nprecision highp float;uniform sampler2D uSource,uRule;uniform int uP,uS;in vec2 vUv;out vec4 frag;void main(){vec2 pixel=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);vec4 c=texture(uSource,vUv);float hi=float(uP)/128.,lo=hi-float(uS)/256.;float x=clamp((texture(uRule,pixel/vec2(1920.,1080.)).r-lo)/(hi-lo),0.,1.);frag=vec4(c.rgb,(1.-x*x*(3.-2.*x))*c.a);}";
    int render(String token,String key,int p,int s,String vertex,Draw draw){
        if(token.isEmpty()||p<0||p>256||s<=0||s>256)throw new IllegalArgumentException("Invalid mask7 spec");
        key=key+":"+p+":"+s;
        Canvas c=canvases.get(token);if(c==null){c=new Canvas();canvases.put(token,c);}if(c.texture!=0&&c.key.equals(key))return c.texture;
        int read=integer(GLES30.GL_READ_FRAMEBUFFER_BINDING),write=integer(GLES30.GL_DRAW_FRAMEBUFFER_BINDING),oldProgram=integer(GLES30.GL_CURRENT_PROGRAM),active=integer(GLES30.GL_ACTIVE_TEXTURE);
        int[] viewport=new int[4],box=new int[4];GLES30.glGetIntegerv(GLES30.GL_VIEWPORT,viewport,0);GLES30.glGetIntegerv(GLES30.GL_SCISSOR_BOX,box,0);boolean blend=GLES30.glIsEnabled(GLES30.GL_BLEND),scissor=GLES30.glIsEnabled(GLES30.GL_SCISSOR_TEST);
        try{
            c.key="";if(program==0)program=link(vertex,FRAGMENT);if(c.texture==0)allocate(c);
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,c.framebuffer);GLES30.glViewport(0,0,1920,1080);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glDisable(GLES30.GL_BLEND);GLES30.glClearColor(0,0,0,0);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);GLES30.glUseProgram(program);
            GLES30.glUniform1i(GLES30.glGetUniformLocation(program,"uSource"),0);GLES30.glUniform1i(GLES30.glGetUniformLocation(program,"uRule"),1);GLES30.glUniform1i(GLES30.glGetUniformLocation(program,"uP"),p);GLES30.glUniform1i(GLES30.glGetUniformLocation(program,"uS"),s);
            draw.draw(program);check("mask7 prepass");c.key=key;draws++;return c.texture;
        }finally{GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,write);GLES30.glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(blend)GLES30.glEnable(GLES30.GL_BLEND);else GLES30.glDisable(GLES30.GL_BLEND);if(scissor)GLES30.glEnable(GLES30.GL_SCISSOR_TEST);else GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glScissor(box[0],box[1],box[2],box[3]);GLES30.glUseProgram(oldProgram);GLES30.glActiveTexture(active);}
    }
    void retain(Set<String> tokens){for(Iterator<Map.Entry<String,Canvas>> i=canvases.entrySet().iterator();i.hasNext();){Map.Entry<String,Canvas> e=i.next();if(!tokens.contains(e.getKey())){release(e.getValue());i.remove();}}}
    void contextLost(){canvases.clear();program=0;}
    void release(){for(Canvas c:canvases.values())release(c);canvases.clear();if(program!=0)GLES30.glDeleteProgram(program);program=0;}
    int canvasCount(){int n=0;for(Canvas c:canvases.values())if(c.texture!=0)n++;return n;}long drawCount(){return draws;}
    private static void allocate(Canvas c){int[] n=new int[1];try{GLES30.glGenTextures(1,n,0);c.texture=n[0];GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,c.texture);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,1920,1080,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLES30.glGenFramebuffers(1,n,0);c.framebuffer=n[0];GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,c.framebuffer);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,c.texture,0);if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("Incomplete mask7 canvas");check("mask7 allocate");}catch(RuntimeException e){release(c);throw e;}}
    private static void release(Canvas c){if(c.texture!=0)GLES30.glDeleteTextures(1,new int[]{c.texture},0);if(c.framebuffer!=0)GLES30.glDeleteFramebuffers(1,new int[]{c.framebuffer},0);c.texture=c.framebuffer=0;c.key="";}
    private static int shader(int type,String source){int shader=GLES30.glCreateShader(type);GLES30.glShaderSource(shader,source);GLES30.glCompileShader(shader);int[] ok=new int[1];GLES30.glGetShaderiv(shader,GLES30.GL_COMPILE_STATUS,ok,0);if(ok[0]==0){String error=GLES30.glGetShaderInfoLog(shader);GLES30.glDeleteShader(shader);throw new IllegalStateException(error);}return shader;}
    private static int link(String vertex,String fragment){int v=shader(GLES30.GL_VERTEX_SHADER,vertex),f=0,p=0;try{f=shader(GLES30.GL_FRAGMENT_SHADER,fragment);p=GLES30.glCreateProgram();GLES30.glAttachShader(p,v);GLES30.glAttachShader(p,f);GLES30.glLinkProgram(p);int[] ok=new int[1];GLES30.glGetProgramiv(p,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetProgramInfoLog(p));return p;}catch(RuntimeException e){if(p!=0)GLES30.glDeleteProgram(p);throw e;}finally{GLES30.glDeleteShader(v);if(f!=0)GLES30.glDeleteShader(f);}}
    private static int integer(int key){int[] n=new int[1];GLES30.glGetIntegerv(key,n,0);return n[0];}
    private static void check(String where){int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException(where+" GL0x"+Integer.toHexString(error));}
}
