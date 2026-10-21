package org.mahoyo.port;

import android.opengl.GLES30;
import org.json.JSONObject;
import java.nio.*;
import java.util.*;

/** Native standalone canvas window; all three intermediate attachments are straight RGBA8. */
final class WindowMask {
    static final int WIDTH=1920,HEIGHT=1080;
    static final class Patch {
        final float[] destination,source;
        Patch(float l,float t,float r,float b,float sl,float st,float sr,float sb){destination=new float[]{l,t,r,b};source=new float[]{sl,st,sr,sb};}
    }
    static final class Spec {
        final int x,y,width,height,soft,color,atlas;
        final boolean edgeX,edgeY;
        Spec(int x,int y,int width,int height,int soft,int colorMode){
            if(width<=0||width>=4096||height<=0||height>=2048||soft<0||soft>=4096)throw new IllegalArgumentException("Native window extent outside reviewed bounds");
            this.x=x;this.y=y;this.width=width;this.height=height;this.soft=soft;color=color(colorMode);atlas=atlas(soft);edgeX=width<WIDTH;edgeY=height<HEIGHT;
            // Original 8A4D0 uploads signed, possibly overlapping edge quads.
            // The center Clear keeps its orientation; an inverted center is empty.
        }
        static Spec from(JSONObject value){return value==null?null:new Spec(value.optInt("x",0),value.optInt("y",0),value.optInt("width",0),value.optInt("height",0),value.optInt("softEdge",0),value.optInt("colorMode",0));}
        boolean identity(){return x==0&&y==0&&width==WIDTH&&height==HEIGHT&&soft==0&&color==0;}
        float[] center(){return new float[]{x+(edgeX?soft:0),y+(edgeY?soft:0),x+width-(edgeX?soft:0),y+height-(edgeY?soft:0)};}
        List<Patch> patches(int mode){
            if(mode<0||mode>2)throw new IllegalArgumentException("Unreviewed native edge mode");
            if(soft==0||!edgeX&&!edgeY)return Collections.emptyList();
            float[] bounds=mode==0?new float[]{x,y,x+width,y+height}:center();float[] xs={bounds[0],bounds[0]+soft,bounds[2]-soft,bounds[2]},ys={bounds[1],bounds[1]+soft,bounds[3]-soft,bounds[3]};
            int n=atlas/2;int[][] cells={{0,0},{1,0},{2,0},{0,1},{2,1},{0,2},{1,2},{2,2}};
            int[][] src={{0,0,n,n},{n-1,0,n,n},{n,0,2*n,n},{0,n-1,n,n},{n,n-1,2*n,n},{0,n,n,2*n},{n-1,n,n,2*n},{n,n,2*n,2*n}};
            boolean[] enabled={true,edgeY,true,edgeX,edgeX,true,edgeY,true};ArrayList<Patch> result=new ArrayList<>();
            for(int i=0;i<cells.length;i++)if(enabled[i]){int c=cells[i][0],r=cells[i][1];result.add(new Patch(xs[c],ys[r],xs[c+1],ys[r+1],edgeX?src[i][0]:n-1,edgeY?src[i][1]:n-1,edgeX?src[i][2]:n,edgeY?src[i][3]:n));}return result;
        }
        Set<String> assets(){if(soft==0||!edgeX&&!edgeY)return Collections.emptySet();LinkedHashSet<String> result=new LinkedHashSet<>();result.add(path(false,atlas));if(color!=0)result.add(path(true,atlas));return result;}
    }
    static int atlas(int soft){for(int side:new int[]{20,64,80,128,256,512})if(soft<=side/2)return side;return 512;}
    static int color(int mode){switch(mode){case 1:return 0xff000000;case 2:return 0xffffffff;case 3:return 0xffff0000;case 4:return 0xff2d1300;case 5:return 0xfff6f4e8;default:return 0;}}
    static String path(boolean inverse,int atlas){return String.format(Locale.ROOT,"meta/window-atlas-%s-%03d.png",inverse?"inv":"normal",atlas);}
    static List<JSONObject> children(List<JSONObject> layers,JSONObject parent){ArrayList<JSONObject> result=new ArrayList<>();int id=parent.optInt("id",-1);for(JSONObject child:layers)if(child.optInt("parent",-1)==id){if(child==parent||child.optJSONObject("window")!=null||(child.optInt("blend",0)!=0&&child.optInt("blend",0)!=1&&child.optInt("blend",0)!=11&&child.optInt("blend",0)!=12&&child.optInt("blend",0)!=13&&child.optInt("blend",0)!=14&&child.optInt("blend",0)!=15&&child.optInt("blend",0)!=16&&child.optInt("blend",0)!=17&&child.optInt("blend",0)!=19&&child.optInt("blend",0)!=20&&child.optInt("blend",0)!=22&&child.optInt("blend",0)!=24&&child.optInt("blend",0)!=25&&child.optInt("blend",0)!=18&&child.optInt("blend",0)!=5&&child.optInt("blend",0)!=4&&child.optInt("blend",0)!=10&&child.optInt("blend",0)!=6&&child.optInt("blend",0)!=7&&child.optInt("blend",0)!=21&&child.optInt("blend",0)!=23)||child.optInt("blend",0)==15&&child.optInt("blendFlag",0)==1||child.optInt("blend",0)==17&&(child.optInt("blendFlag",0)<0||child.optInt("blendFlag",0)>1)||(child.optInt("blend",0)==4||child.optInt("blend",0)==10||child.optInt("blend",0)==6||child.optInt("blend",0)==7||child.optInt("blend",0)==12||child.optInt("blend",0)==13||child.optInt("blend",0)==14||child.optInt("blend",0)==20||child.optInt("blend",0)==21||child.optInt("blend",0)==23||child.optInt("blend",0)==24||child.optInt("blend",0)==25||child.optInt("blend",0)==18)&&child.optInt("blendFlag",0)!=0||child.optInt("blend",0)==5&&child.optInt("blendFlag",0)!=1)throw new IllegalArgumentException("Unsupported nested/window/blended native child");if(child.optInt("blend",0)==7&&(parent.optInt("parent",-1)>=0||parent.optJSONObject("window")==null||child.has("source")||parent.has("source")||child.optInt("decimalMode",0)%10!=0||parent.optInt("decimalMode",0)%10!=0))throw new IllegalArgumentException("Unsupported child7 cropped/noise/parent context");JSONObject wave=child.optJSONObject("wave");if(wave!=null&&(!((wave.optInt("preset",0)==6&&child.optInt("blend",0)==19&&child.optInt("blendFlag",0)==0)||(wave.optInt("preset",0)==20&&child.optInt("blend",0)==11&&child.optInt("blendFlag",0)==0&&child.optInt("colorMode",0)==1&&child.optInt("rgb",0)==0xff1313)||(wave.optInt("preset",0)==35&&child.optInt("blend",0)==0&&child.optInt("blendFlag",0)==0&&child.optInt("colorMode",0)==0&&child.optInt("rgb",0)==0))))throw new IllegalArgumentException("Unsupported native wave child combination");for(JSONObject descendant:layers)if(descendant.optInt("parent",-1)==child.optInt("id",-2))throw new IllegalArgumentException("Unsupported nested native window group");result.add(child);}return result;}
    static float childOpacity(double alpha){int fixed=(int)(float)(Math.max(0,Math.min(1,alpha))*65536.);return ((fixed*255)>>16)/255f;}
    static float childOpacity(JSONObject child){return child.has("groupOpacityByte")?Math.max(0,Math.min(255,child.optInt("groupOpacityByte",0)))/255f:childOpacity(child.optDouble("alpha",1));}
    static final String VERTEX="#version 300 es\nin vec2 aPosition;void main(){gl_Position=vec4(aPosition,0.,1.);}";
    static final String FRAGMENT="#version 300 es\nprecision highp float;precision highp int;uniform sampler2D uSource;uniform sampler2D uAtlas;uniform int uPass;uniform int uCount;uniform float uAtlasSide;uniform vec4 uCenter;uniform vec4 uColor;uniform vec4 uDst[8];uniform vec4 uSrc[8];out vec4 frag;"+
        "bool containsPixel(vec2 p,vec4 r){return all(greaterThanEqual(p,r.xy))&&all(lessThan(p,r.zw));}"+
        "bool containsPatchPixel(vec2 p,vec4 r){return containsPixel(p,vec4(min(r.xy,r.zw),max(r.xy,r.zw)));}"+
        "float patchAlpha(vec2 p,float initial){float a=initial;for(int i=0;i<8;i++){if(i>=uCount)break;if(containsPatchPixel(p,uDst[i])){vec2 pixel=uSrc[i].xy+(p-uDst[i].xy)/(uDst[i].zw-uDst[i].xy)*(uSrc[i].zw-uSrc[i].xy);a=texture(uAtlas,pixel/uAtlasSide).a;}}return a;}"+
        "float patchAlpha(vec2 p){return patchAlpha(p,0.);}"+
        "vec4 nativeWindowOver(vec4 d,float atlasAlpha,vec4 color){float t=atlasAlpha*color.a;float a=t+d.a*(1.-t);vec3 rgb=a>0.?(t*color.rgb+(1.-t)*d.a*d.rgb)/a:vec3(0.);return vec4(rgb,a);}"+
        "void main(){vec2 p=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);vec4 d=texelFetch(uSource,ivec2(gl_FragCoord.xy),0);if(uPass==0){float mask=containsPixel(p,uCenter)?1.:0.;if(uColor.a==0.)mask=patchAlpha(p,mask);frag=vec4(d.rgb,min(d.a,mask));}else frag=nativeWindowOver(d,patchAlpha(p),uColor);}";
    private final int[] textures=new int[3],framebuffers=new int[3];
    private final FloatBuffer quad=ByteBuffer.allocateDirect(8*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
    private int program,position,source,atlas,pass,count,atlasSide,center,color,dst,src,current;
    private boolean identityEnabled=true,copyElisionEnabled=true,parentCacheEnabled=true;
    private String cachedBaseKey;
    void contextLost(){Arrays.fill(textures,0);Arrays.fill(framebuffers,0);program=0;current=0;cachedBaseKey=null;}
    void release(){if(textures[0]!=0)GLES30.glDeleteTextures(3,textures,0);if(framebuffers[0]!=0)GLES30.glDeleteFramebuffers(3,framebuffers,0);if(program!=0)GLES30.glDeleteProgram(program);contextLost();}
    private void ensure(){
        if(program!=0)return;
        int v=0,f=0;
        try{program=GLES30.glCreateProgram();v=shader(GLES30.GL_VERTEX_SHADER,VERTEX);f=shader(GLES30.GL_FRAGMENT_SHADER,FRAGMENT);GLES30.glAttachShader(program,v);GLES30.glAttachShader(program,f);GLES30.glLinkProgram(program);int[] ok=new int[1];GLES30.glGetProgramiv(program,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetProgramInfoLog(program));
            position=GLES30.glGetAttribLocation(program,"aPosition");source=uniform("uSource");atlas=uniform("uAtlas");pass=uniform("uPass");count=uniform("uCount");atlasSide=uniform("uAtlasSide");center=uniform("uCenter");color=uniform("uColor");dst=uniform("uDst[0]");src=uniform("uSrc[0]");
            GLES30.glGenTextures(3,textures,0);GLES30.glGenFramebuffers(3,framebuffers,0);for(int i=0;i<3;i++){GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,textures[i]);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,WIDTH,HEIGHT,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,framebuffers[i]);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,textures[i],0);if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("Incomplete native window framebuffer");}
        }catch(RuntimeException error){release();throw error;}finally{if(v!=0)GLES30.glDeleteShader(v);if(f!=0)GLES30.glDeleteShader(f);}
    }
    void begin(){begin(null);}
    boolean begin(String baseKey){ensure();if(!parentCacheEnabled||!identityEnabled)baseKey=null;boolean reused=baseKey!=null&&baseKey.equals(cachedBaseKey);cachedBaseKey=baseKey;current=baseKey==null?0:1;GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,framebuffers[current]);GLES30.glViewport(0,0,WIDTH,HEIGHT);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glDisable(GLES30.GL_BLEND);if(!reused){GLES30.glClearColor(0,0,0,0);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);}return !reused;}
    void beginChild(Spec spec,int normal,int sceneProgram){beginChild(spec,normal,sceneProgram,false);}
    void beginChild(Spec spec,int normal,int sceneProgram,boolean fullCoverage){
        checkError("parent/previous child source");int previous=current;current=current==0?2:0;GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,framebuffers[previous]);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,framebuffers[current]);if(!copyElisionEnabled||!fullCoverage)GLES30.glBlitFramebuffer(0,0,WIDTH,HEIGHT,0,0,WIDTH,HEIGHT,GLES30.GL_COLOR_BUFFER_BIT,GLES30.GL_NEAREST);checkError("group framebuffer copy");GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,framebuffers[current]);GLES30.glUseProgram(sceneProgram);GLES30.glActiveTexture(GLES30.GL_TEXTURE1);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,textures[previous]);GLES30.glActiveTexture(GLES30.GL_TEXTURE3);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,normal);atlasParameters(normal);
        GLES30.glUniform1i(GLES30.glGetUniformLocation(sceneProgram,"uAtlas"),3);GLES30.glUniform1f(GLES30.glGetUniformLocation(sceneProgram,"uAtlasSide"),spec.atlas);GLES30.glUniform4fv(GLES30.glGetUniformLocation(sceneProgram,"uCenter"),1,spec.center(),0);GLES30.glUniform4f(GLES30.glGetUniformLocation(sceneProgram,"uColor"),0,0,0,(spec.color>>>24)/255f);uploadPatches(spec.patches(0),GLES30.glGetUniformLocation(sceneProgram,"uCount"),GLES30.glGetUniformLocation(sceneProgram,"uDst[0]"),GLES30.glGetUniformLocation(sceneProgram,"uSrc[0]"));
    }
    int apply(Spec spec,int normal,int inverse){
        checkError("image/group source");if(identityEnabled&&spec.identity())return textures[current];cachedBaseKey=null;render(current,1,spec,normal,0);if(spec.color==0||spec.soft==0||!spec.edgeX&&!spec.edgeY)return textures[1];render(1,2,spec,normal,1);render(2,1,spec,inverse,2);return textures[1];
    }
    private void render(int input,int output,Spec spec,int atlasTexture,int stage){
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,framebuffers[output]);GLES30.glDisable(GLES30.GL_BLEND);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glUseProgram(program);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,textures[input]);GLES30.glUniform1i(source,0);
        GLES30.glActiveTexture(GLES30.GL_TEXTURE1);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,atlasTexture);atlasParameters(atlasTexture);GLES30.glUniform1i(atlas,1);
        GLES30.glUniform1i(pass,stage==0?0:1);GLES30.glUniform1f(atlasSide,spec.atlas);float[] bounds=spec.center();GLES30.glUniform4fv(center,1,bounds,0);GLES30.glUniform4f(color,((spec.color>>>16)&255)/255f,((spec.color>>>8)&255)/255f,(spec.color&255)/255f,(spec.color>>>24)/255f);
        uploadPatches(spec.patches(stage==2?1:0),count,dst,src);
        quad.clear();quad.put(new float[]{-1,1,-1,-1,1,1,1,-1}).flip();GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,0,quad);GLES30.glEnableVertexAttribArray(position);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException("Native window GL error0x"+Integer.toHexString(error));
    }
    private int uniform(String name){return GLES30.glGetUniformLocation(program,name);}
    private static void checkError(String stage){int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException("Native window "+stage+" GL error0x"+Integer.toHexString(error));}
    private static void uploadPatches(List<Patch> patches,int count,int dst,int src){float[] destinations=new float[32],sources=new float[32];for(int i=0;i<patches.size();i++){System.arraycopy(patches.get(i).destination,0,destinations,i*4,4);System.arraycopy(patches.get(i).source,0,sources,i*4,4);}GLES30.glUniform1i(count,patches.size());if(!patches.isEmpty()){GLES30.glUniform4fv(dst,patches.size(),destinations,0);GLES30.glUniform4fv(src,patches.size(),sources,0);}}
    private static void atlasParameters(int texture){if(texture!=0){GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_REPEAT);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_REPEAT);}}
    private static int shader(int type,String code){int shader=GLES30.glCreateShader(type);GLES30.glShaderSource(shader,code);GLES30.glCompileShader(shader);int[] result=new int[1];GLES30.glGetShaderiv(shader,GLES30.GL_COMPILE_STATUS,result,0);if(result[0]==0){String error=GLES30.glGetShaderInfoLog(shader);GLES30.glDeleteShader(shader);throw new IllegalStateException(error);}return shader;}
}
