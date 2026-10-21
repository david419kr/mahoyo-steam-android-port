package org.mahoyo.port;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.RectF;
import android.opengl.GLES30;
import android.opengl.GLES11Ext;
import android.opengl.EGL14;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import org.json.JSONObject;
import org.json.JSONArray;
import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.nio.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;
import java.util.function.ToLongFunction;
import java.util.function.Predicate;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

final class SceneRenderer implements GLSurfaceView.Renderer {
    interface Assets { GameAsset file(String name); default Collection<String> imageFiles(){return Collections.emptyList();} }
    private final GLSurfaceView view;
    private final Assets assets;
    private final MoviePlayer movies;
    private final ExecutorService loader=Executors.newSingleThreadExecutor();
    private final ExecutorService hintResolver=Executors.newSingleThreadExecutor();
    private static final long IMAGE_BUDGET=192L*1024*1024;
    private volatile int textureLimit=4096;
    private final Residency<Texture> textures=new Residency<>(IMAGE_BUDGET,t->(long)t.width*t.height*4);
    private final WarmPolicy warm=new WarmPolicy(32);
    private final WindowMask windowMask=new WindowMask();
    private final PlainImageProgram plainImage=new PlainImageProgram();
    private boolean plainEnabled=true,identityChildStage;
    private final WaveRenderer waveRenderer=new WaveRenderer();
    private final GlobalScreenWaveRenderer screenWaveRenderer=new GlobalScreenWaveRenderer();
    private final Set<String> waveErrors=new HashSet<>();
    private boolean particleFrameFailed;
    private boolean waveFrameFailed;
    private final NoiseRenderer<JSONObject> noiseRenderer=new NoiseRenderer<>();
    private final Set<String> noiseErrors=new HashSet<>();
    private Map<String,NoiseClock.Frame<JSONObject>> noisePresented=new HashMap<>(),noiseOld=new HashMap<>(),noiseRendered=new HashMap<>();
    private Scene noiseOldScene;
    private String noiseTransitionId="";
    private boolean noiseFrameFailed;
    private boolean alphaRuleFrameFailed,alphaRuleFinalStage;
    private volatile boolean alphaRuleNeedsRebuild;
    private final LayerAlphaRuleRenderer alphaRuleRenderer=new LayerAlphaRuleRenderer();
    private boolean noiseFinalStage;
    private final Set<String> windowErrors=new HashSet<>();
    private boolean windowImageStage;
    private boolean windowChildStage;
    private PreparedImages preparedImages;
    private boolean warmResolving;
    private volatile boolean stopped;
    private volatile String warmInputKey="";
    private String warmResolvedKey="";
    private Scene warmDesired;
    private final Map<String,String> imageKeys=new HashMap<>();
    private final Map<String,GameAsset> imageSources=new ConcurrentHashMap<>();
    private final Set<String> pending=ConcurrentHashMap.newKeySet();
    private final Set<String> failed=ConcurrentHashMap.newKeySet();
    private final Set<Integer> unsupportedBlends=new HashSet<>();
    private final Set<Integer> unsupportedColors=new HashSet<>();
    private final Map<Integer,Integer> colorPaletteTextures=new HashMap<>();
    private final Set<Integer> failedColorPalettes=new HashSet<>();
    private final Set<String> destinationErrors=new HashSet<>();
    private final Consumer<Scene> committed;
    private final Consumer<String> errors;
    private volatile Scene scene,desired;
    private volatile Viewport viewport;
    private volatile boolean dynamic=true,ambient=true;
    private volatile boolean bgCover=true,eventCover=true;
    private volatile boolean hiddenText;
    private volatile TextOverlay.RenderState textRenderState;
    private volatile int filter;
    private int width,height,program,position,uv,sampler,size,kernel,opacity,blur,tint,boundsUniform,blendUniform,destinationUniform,viewportUniform;
    private int destinationTexture,destinationWidth,destinationHeight;
    private int destinationCopyFormat;
    private final DestinationState mainDestination=new DestinationState(),screenDestination=new DestinationState();
    private volatile int screenDestinationTextureCount;
    private boolean destinationCopyVerified,framebufferLogged;
    private int colorModeUniform,contrastUniform,brightnessUniform,rgbUniform,sepiaUniform,premultipliedUniform,blendFlagUniform,rawOutputUniform;
    private int noiseSamplerUniform,noiseOrientationUniform,noiseFinal28Uniform;
    private Texture transitionFrame,nativeTextTexture;
    private Texture languageFrame;
    private volatile String languageTarget="";
    private final TransitionFrameState transitionState=new TransitionFrameState();
    private String nativeTextKey="";
    private Texture movieFrame;
    private volatile String movieFrameId="";
    private int movieFramebuffer,movieProgram,moviePosition,movieUv,movieSampler,movieTransform;
    private int maskProgram,maskPosition,maskUv,maskOld,maskRule,maskArea,maskHeight,maskProgress,maskStrength;
    private String movieGpuFailure="";
    private final MovieRenderGate movieRenderGate=new MovieRenderGate();
    private int generation;
    private Texture solid;
    private final FloatBuffer vertices=ByteBuffer.allocateDirect(16*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
    private static final String VERTEX="#version 300 es\nin vec2 aPosition;in vec2 aUv;out vec2 vUv;void main(){vUv=aUv;gl_Position=vec4(aPosition,0.,1.);}";
    static final String MOVIE_VERTEX="#version 300 es\nin vec2 aPosition;in vec2 aUv;uniform mat4 uTransform;out vec2 vUv;void main(){vUv=(uTransform*vec4(aUv,0.,1.)).xy;gl_Position=vec4(aPosition,0.,1.);}";
    static final String MOVIE_FRAGMENT="#version 300 es\n#extension GL_OES_EGL_image_external_essl3 : require\nprecision highp float;uniform samplerExternalOES uMovie;in vec2 vUv;out vec4 frag;void main(){frag=vec4(texture(uMovie,vUv).rgb,1.);}";
    static final String MASK_FRAGMENT="#version 300 es\nprecision highp float;precision highp int;uniform sampler2D uOldFrame;uniform sampler2D uRule;uniform vec4 uArea;uniform float uHeight;uniform int uProgress;uniform int uStrength;in vec2 vUv;out vec4 frag;"+
        "float nativeMaskAlpha(float m,int p,int s){if(s==0)return p==0?1.:0.;float hi=min(2.,float(p)/128.);float lo=max(-1.,float(p)/128.-float(s)/256.);float x=clamp((m-lo)/(hi-lo),0.,1.);return x*x*(3.-2.*x);}"+
        "void main(){vec2 pixel=vec2(gl_FragCoord.x,uHeight-gl_FragCoord.y);vec2 ruleUv=(pixel-uArea.xy)/uArea.zw;float a=nativeMaskAlpha(texture(uRule,ruleUv).r,uProgress,uStrength);vec3 old=texture(uOldFrame,vUv).rgb;frag=vec4(old*a,a);}";
    static final String FRAGMENT="#version 300 es\nprecision highp float;precision highp int;uniform highp sampler2D uImage;uniform highp sampler2D uDestination;uniform highp sampler2D uSepia;uniform vec2 uViewport;uniform vec2 uSize;uniform int uKernel;uniform int uBlur;uniform int uBlend;uniform int uBlendFlag;uniform int uColorMode;uniform int uContrast;uniform int uBrightness;uniform int uRgb;uniform int uPremultiplied;uniform int uRawOutput;uniform float uOpacity;uniform vec4 uTint;uniform vec4 uBounds;in vec2 vUv;out vec4 frag;"+
        "uniform sampler2D uNoise;uniform int uNoiseOrientation;uniform int uNoiseFinal28;uniform sampler2D uAtlas;uniform int uCount;uniform float uAtlasSide;uniform vec4 uCenter;uniform vec4 uColor;uniform vec4 uDst[8];uniform vec4 uSrc[8];"+
        "bool containsPixel(vec2 p,vec4 r){return all(greaterThanEqual(p,r.xy))&&all(lessThan(p,r.zw));}"+
        "bool containsPatchPixel(vec2 p,vec4 r){return containsPixel(p,vec4(min(r.xy,r.zw),max(r.xy,r.zw)));}"+
        "float patchAlpha(vec2 p,float initial){float a=initial;for(int i=0;i<8;i++){if(i>=uCount)break;if(containsPatchPixel(p,uDst[i])){vec2 pixel=uSrc[i].xy+(p-uDst[i].xy)/(uDst[i].zw-uDst[i].xy)*(uSrc[i].zw-uSrc[i].xy);a=texture(uAtlas,pixel/uAtlasSide).a;}}return a;}"+
        "float patchAlpha(vec2 p){return patchAlpha(p,0.);}"+
        "float nativeWindowByte(float a){return floor(clamp(a,0.,1.)*255.+.5)/255.;}"+
        "float nativeWindowWeight(float baseAlpha,float maskAlpha,float sourceAlpha,float o){return min(min(nativeWindowByte(baseAlpha*o),nativeWindowByte(maskAlpha)),nativeWindowByte(sourceAlpha*o));}"+
        "float sinc(float x){x=abs(x);if(x<.00001)return 1.;float p=3.14159265359*x;return sin(p)/p;}"+
        "float cubic(float x){x=abs(x);if(x<1.)return (1.5*x-2.5)*x*x+1.;if(x<2.)return ((-.5*x+2.5)*x-4.)*x+2.;return 0.;}"+
        "float weight(float x){return uKernel==1?cubic(x):(abs(x)<3.?sinc(x)*sinc(x/3.):0.);}"+
        "bool nativeChannelPalette(int mode){return mode>=8&&mode<=82;}"+
        "int nativePaletteIndex(int value,int gray,int mode){return nativeChannelPalette(mode)?value:gray;}"+
        "int nativeGray(int r,int g,int b){return (322*r+1096*g+112*b+765)/1530;}"+
        "int nativePalette(int value,int gray,int mode,int solid,int sepia){if(mode==1)return solid;if(mode==2)return gray;if(mode==3||nativeChannelPalette(mode))return sepia;if(mode==4)return 255-value;return value;}"+
        "int nativeByte(int value,int brightness,int contrast){value=clamp(value+brightness,0,255);if(contrast>0){if(value<contrast)return 0;if(value<255-contrast)return (value-contrast)*255/(255-2*contrast);return 255;}if(contrast<0)return ((value*(255+2*contrast))/255-contrast)&255;return value;}"+
        "ivec3 nativeColor(ivec3 c){int gray=nativeGray(c.r,c.g,c.b);if(uColorMode==6)return ivec3(nativeByte(0,uBrightness,uContrast),nativeByte(gray,uBrightness,uContrast),nativeByte(0,uBrightness,uContrast));if(uColorMode==7)return ivec3(nativeByte(gray,uBrightness,uContrast),nativeByte(0,uBrightness,uContrast),nativeByte(0,uBrightness,uContrast));if(uColorMode==5)return ivec3(nativeByte(0,uBrightness,uContrast),nativeByte(0,uBrightness,uContrast),nativeByte(gray,uBrightness,uContrast));ivec3 sepia=ivec3(0);if(uColorMode==3)sepia=ivec3(floor(texelFetch(uSepia,ivec2(gray,0),0).rgb*255.+.5));else if(nativeChannelPalette(uColorMode)){sepia.r=int(floor(texelFetch(uSepia,ivec2(nativePaletteIndex(c.r,gray,uColorMode),0),0).r*255.+.5));sepia.g=int(floor(texelFetch(uSepia,ivec2(nativePaletteIndex(c.g,gray,uColorMode),0),0).g*255.+.5));sepia.b=int(floor(texelFetch(uSepia,ivec2(nativePaletteIndex(c.b,gray,uColorMode),0),0).b*255.+.5));}ivec3 solid=ivec3((uRgb>>16)&255,(uRgb>>8)&255,uRgb&255);return ivec3(nativeByte(nativePalette(c.r,gray,uColorMode,solid.r,sepia.r),uBrightness,uContrast),nativeByte(nativePalette(c.g,gray,uColorMode,solid.g,sepia.g),uBrightness,uContrast),nativeByte(nativePalette(c.b,gray,uColorMode,solid.b,sepia.b),uBrightness,uContrast));}"+
        "vec4 sourceTexel(ivec2 p){ivec2 lo=ivec2(floor(uBounds.xy*uSize));ivec2 hi=ivec2(floor(uBounds.zw*uSize));vec4 c=texelFetch(uImage,clamp(p,lo,hi),0);if(uPremultiplied!=0)c.rgb=c.a>.000001?c.rgb/c.a:vec3(0.);if(uColorMode!=0||uBrightness!=0||uContrast!=0)c.rgb=vec3(nativeColor(ivec3(floor(c.rgb*255.+.5))))/255.;return c;}"+
        "vec4 sampleImage(vec2 uv){vec2 p=uv*uSize-.5;ivec2 base=ivec2(floor(p));vec2 f=p-vec2(base);if(uKernel==0){vec4 top=mix(sourceTexel(base),sourceTexel(base+ivec2(1,0)),f.x);vec4 bottom=mix(sourceTexel(base+ivec2(0,1)),sourceTexel(base+ivec2(1,1)),f.x);return mix(top,bottom,f.y);}vec4 c=vec4(0.);float total=0.;for(int y=-2;y<=3;y++)for(int x=-2;x<=3;x++){float w=weight(float(x)-f.x)*weight(float(y)-f.y);c+=sourceTexel(base+ivec2(x,y))*w;total+=w;}return clamp(c/max(total,.0001),0.,1.);}"+
        "vec4 ambientTap(vec2 uv){float lod=clamp(log2(max(1.,min(uSize.x,uSize.y)/8.)),0.,10.);vec2 pad=vec2(exp2(lod)*.5)/uSize;vec2 lo=min(uBounds.xy+pad,(uBounds.xy+uBounds.zw)*.5),hi=max(uBounds.zw-pad,lo);return textureLod(uImage,clamp(uv,lo,hi),lod);}"+
        "vec3 nativeBlendRgb(int mode,vec3 d,vec3 s){"+
        "if(mode==11)return min(d+s,vec3(1.));"+
        "if(mode==12)return max(1.-(1.-d)*1.003921627998352/(s+.003921568859368563),vec3(0.));"+
        "if(mode==13)return min(d,s);"+
        "if(mode==14)return abs(d-s);"+
        "if(mode==16)return mix(min(d/max(vec3(1.)-s,vec3(.00000001)),vec3(1.)),vec3(1.),step(vec3(1.),s));"+
        "if(mode==18)return d+s-2.*d*s;if(mode==19)return mix(2.*d*s,1.-2.*(1.-d)*(1.-s),step(vec3(.5),s));"+
        "if(mode==20)return max(d,s);"+
        "if(mode==21)return d*s;"+
        "if(mode==22)return mix(2.*d*s,1.-2.*(1.-d)*(1.-s),step(vec3(.5),d));"+
        "if(mode==23)return 1.-(1.-d)*(1.-s);"+
        "if(mode==24)return pow(d,mix(2.*(1.-s),vec3(1.0127500295639038)/(2.*s+.00009999999747378752),step(vec3(.5),s)));"+
        "if(mode==25)return max(d+s-1.,vec3(0.));return s;}"+
        "vec3 nativeBlend1(vec3 d,vec3 s,float o){return min(d+s*o,vec3(1.));}"+
        "vec3 nativeBlend2(vec3 d,vec3 s,float o,float sa){return min(d*(1.-sa*o)+s*o,vec3(1.));}"+
        "vec3 nativeBlend4(vec3 d,vec3 s,float o){return mix(d,min(d,s),o);}"+
        "vec3 nativeBlend6(vec3 d,vec3 s,float o){return mix(d,max(d,s),o);}"+
        "vec3 nativeBlend7(vec3 d,vec3 s,float o,float sa,int flag){return mix(d,d*s,o*(flag==1?sa:1.));}"+
        "vec3 nativeBlend8(vec3 d,vec3 s,float o){return d+(s-d)*o;}"+
        "vec3 nativeBlend10(vec3 d,vec3 s,float o){return max(d+(s-1.)*o,vec3(0.));}"+
        "vec3 nativeBlend9(vec3 d,vec3 s,float o,float sa,int flag){return 1.-(1.-d)*(1.-s*(o*(flag==1?sa:1.)));}"+
        "vec3 nativeBlend15(vec3 d,vec3 s,float a){return abs(d-s*a);}"+
        "vec3 nativeBlend17(vec3 d,vec3 s,float a){return min(d/max(vec3(1.)-s*a,vec3(.00000001)),step(vec3(3./255.),d));}"+
        "vec3 nativeBlend5(vec3 d,vec3 s,float o,float sa,int flag){return nativeBlend17(d,s,o*((flag==1||flag==-1)?sa:1.));}"+
        "vec3 nativeWindowChildRgb(int mode,vec3 d,vec3 s,float sa,float o){if(mode==4)return nativeBlend4(d,s,o);if(mode==10)return nativeBlend10(d,s,o);if(mode==1)return nativeBlend1(d,s,o);if(mode==5)return nativeBlend5(d,s,o,sa,1);if(mode==6)return nativeBlend6(d,s,o);if(mode==7)return nativeBlend7(d,s,o,sa,0);if(mode==21)return nativeBlendRgb(21,d,s);if(mode==23)return nativeBlendRgb(23,d,s);if(mode==11)return nativeBlendRgb(11,d,s);if(mode==12)return nativeBlendRgb(12,d,s);if(mode==13)return nativeBlendRgb(13,d,s);if(mode==14)return nativeBlendRgb(14,d,s);if(mode==16)return nativeBlendRgb(16,d,s);if(mode==19)return nativeBlendRgb(19,d,s);if(mode==20)return nativeBlendRgb(20,d,s);if(mode==22)return nativeBlendRgb(22,d,s);if(mode==24)return nativeBlendRgb(24,d,s);if(mode==25)return nativeBlendRgb(25,d,s);if(mode==18)return nativeBlendRgb(18,d,s);if(mode==15)return nativeBlend15(d,s,sa*o);if(mode==17)return nativeBlend17(d,s,sa*o);return s;}"+
        "vec4 nativeWave109(vec4 d,vec4 s){float a=s.a+d.a*(1.-s.a);return a>0.?vec4((s.rgb*s.a+d.rgb*d.a*(1.-s.a))/a,a):vec4(0.);}"+
        "vec2 nativeNoiseUv(vec2 uv,int orientation){return vec2((orientation&2)==0?uv.x:1.-uv.x,(orientation&1)==0?uv.y:1.-uv.y);}"+
        "vec3 nativeNoisePoint(vec2 q){ivec2 n=textureSize(uNoise,0);return texelFetch(uNoise,clamp(ivec2(floor(q*vec2(n))),ivec2(0),n-ivec2(1)),0).rgb;}"+
        "vec3 nativeNoise78(vec2 uv,int q){return min(nativeNoisePoint(nativeNoiseUv(uv,q)),nativeNoisePoint(nativeNoiseUv(fract(uv*2.),q^3))*(192./255.));}"+
        NoiseSpecial.SHADER+"void main(){vec4 c=vec4(0.);if(uBlur==1){float total=0.;for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){float w=exp(-float(x*x+y*y)*.36);vec4 tap=ambientTap(vUv+vec2(float(x),float(y))*.07);tap.rgb*=tap.a;c+=tap*w;total+=w;}c/=total;c.rgb*=.46;c.a=1.;}else c=sampleImage(vUv);"+
        "if(uRawOutput==6){vec2 pixel=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);frag=vec4(nativeNoise82(pixel/vec2(1920.,1080.),uNoiseOrientation),c.a);return;}"+
        "if(uRawOutput==8){frag=vec4(1.,1.,1.,c.a);return;}"+
        "if(uRawOutput==9){vec2 pixel=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);frag=vec4(nativeNoisePoint(nativeNoiseUv(pixel/vec2(1920.,1080.),uNoiseOrientation)),1.);return;}"+
        "if(uRawOutput==10){vec2 pixel=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);frag=vec4(nativeNoisePoint(nativeNoiseUv(fract(pixel/vec2(960.,540.)),uNoiseOrientation^3))*(64./255.),192./255.);return;}"+
        "if(uRawOutput==11){frag=uTint;return;}"+
        "if(uRawOutput==5){vec2 pixel=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);frag=vec4(nativeNoise78(pixel/vec2(1920.,1080.),uNoiseOrientation),c.a);return;}"+
        "if(uRawOutput==4){vec2 pixel=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);frag=vec4(texture(uNoise,nativeNoiseUv(pixel/vec2(1920.,1080.),uNoiseOrientation)).rgb,c.a);return;}"+
        "vec3 source=clamp(c.rgb*uTint.rgb,0.,1.);float a=c.a*uTint.a*uOpacity;if(uRawOutput==2){vec4 d=texelFetch(uDestination,ivec2(gl_FragCoord.xy),0);vec2 p=vec2(gl_FragCoord.x,1080.-gl_FragCoord.y);float mask=containsPixel(p,uCenter)?1.:0.;if(uColor.a==0.)mask=patchAlpha(p,mask);float w=nativeWindowWeight(d.a,mask,c.a*uTint.a,uOpacity);frag=vec4((uBlend==5||uBlend==17)&&uBlendFlag==1?nativeWindowChildRgb(uBlend,d.rgb,source,c.a*uTint.a,uOpacity):mix(d.rgb,nativeWindowChildRgb(uBlend,d.rgb,source,c.a*uTint.a,uOpacity),w),d.a);}else if(uRawOutput==1){frag=vec4(source,a);}else if(uRawOutput==3){frag=nativeWave109(vec4(0.),vec4(source,c.a*uTint.a));}else if(uBlend==0||uBlend==3){frag=vec4(source*a,a);}else{vec4 dest=texture(uDestination,gl_FragCoord.xy/uViewport);vec3 d=dest.a>.00001?clamp(dest.rgb/dest.a,0.,1.):vec3(0.);if(uBlend==28){if(uNoiseFinal28!=85&&uNoiseFinal28!=120){frag=vec4(1.,0.,1.,1.);return;}frag=vec4(nativeFinal28(d,vec4(source,c.a*uTint.a),uOpacity,uNoiseFinal28),1.);}else if(uBlend==1)frag=vec4(nativeBlend1(d,source,uOpacity),1.);else if(uBlend==2)frag=vec4(nativeBlend2(d,source,uOpacity,c.a*uTint.a),1.);else if(uBlend==4)frag=vec4(nativeBlend4(d,source,uOpacity),1.);else if(uBlend==5)frag=vec4(nativeBlend5(d,source,uOpacity,c.a*uTint.a,uBlendFlag),1.);else if(uBlend==6)frag=vec4(nativeBlend6(d,source,uOpacity),1.);else if(uBlend==7)frag=vec4(nativeBlend7(d,source,uOpacity,c.a*uTint.a,uBlendFlag),1.);else if(uBlend==8)frag=vec4(nativeBlend8(d,source,uOpacity),1.);else if(uBlend==9)frag=vec4(nativeBlend9(d,source,uOpacity,c.a*uTint.a,uBlendFlag),1.);else if(uBlend==10)frag=vec4(nativeBlend10(d,source,uOpacity),1.);else if(uBlend==15)frag=vec4(nativeBlend15(d,source,a),1.);else if(uBlend==17)frag=vec4(nativeBlend17(d,source,a),1.);else frag=vec4(nativeBlendRgb(uBlend,d,source)*a,a);}}";

    SceneRenderer(GLSurfaceView view,Assets assets,Consumer<Scene> committed,Consumer<String> errors,MoviePlayer movies){this.view=view;this.assets=assets;this.committed=committed;this.errors=errors;this.movies=movies;}
    void update(Scene value,Viewport vp,DisplayPrefs prefs,boolean hidden){update(value,vp,prefs,hidden,null);}
    void update(Scene value,Viewport vp,DisplayPrefs prefs,boolean hidden,TextOverlay.RenderState textState){desired=value;viewport=vp;textRenderState=textState;dynamic=prefs.dynamic();ambient=prefs.ambient();bgCover=prefs.cover(false);eventCover=prefs.cover(true);filter=Math.max(0,Math.min(2,prefs.filter()));hiddenText=hidden;if(!value.preloadKey.equals(warmInputKey)){warmInputKey=value.preloadKey;view.queueEvent(()->updateWarmHints(value));}String id=movieId(value);if(movieRenderGate.request(id,frameKey(vp),value.transition!=null,id.equals(movieFrameId)))view.requestRender();}
    boolean ready(Scene value){return languageTarget.isEmpty()&&!alphaRuleNeedsRebuild&&!alphaRuleFrameFailed&&scene==value&&(value.transition==null||transitionState.ready(value.transition.id,transitionFrameKey(value,viewport)));}
    // GL-thread transaction: retain the old image until all newly selected assets are resident.
    void switchLanguage(String target,Runnable change,Runnable complete){
        if(languageFrame==null&&scene!=null&&viewport!=null&&width>0&&height>0){
            renderFrame(scene,viewport,false);
            if(!captureDestination())throw new IllegalStateException("Cannot preserve the current frame for language change");
            release(languageFrame);languageFrame=new Texture(destinationTexture,width,height);
            destinationTexture=0;destinationWidth=0;destinationHeight=0;destinationCopyVerified=false;
        }
        change.run();languageTarget=target;generation++;pending.clear();failed.clear();imageKeys.clear();
        warm.clear();preparedImages=null;warmResolving=false;warmInputKey="";warmResolvedKey="";warmDesired=null;
        windowMask.release();waveRenderer.release();screenWaveRenderer.release();noiseRenderer.release();alphaRuleRenderer.release();alphaRuleNeedsRebuild=true;
        noisePresented.clear();noiseOld.clear();noiseRendered.clear();noiseOldScene=null;
        releaseNativeText();complete.run();view.requestRender();
    }
    String waveDebugState(){return waveRenderer.debugState()+" "+screenWaveRenderer.debugState()+" screenWaveCopyTextures="+screenDestinationTextureCount;}
    String noiseDebugState(){return noiseRenderer.debugState();}
    void shutdown(){stopped=true;loader.shutdownNow();hintResolver.shutdownNow();view.queueEvent(()->{if(hasGlContext()){windowMask.release();plainImage.release();waveRenderer.release();screenWaveRenderer.release();releaseScreenDestination();noiseRenderer.release();alphaRuleRenderer.release();}});}
    @Override public void onSurfaceCreated(GL10 unused,EGLConfig config){
        int[] textureBounds={0};GLES30.glGetIntegerv(GLES30.GL_MAX_TEXTURE_SIZE,textureBounds,0);if(textureBounds[0]<=0)throw new IllegalStateException("GPU texture limit unavailable");textureLimit=textureBounds[0];
        generation++;textures.clear();pending.clear();failed.clear();unsupportedBlends.clear();unsupportedColors.clear();colorPaletteTextures.clear();failedColorPalettes.clear();destinationErrors.clear();destinationTexture=0;destinationWidth=0;destinationHeight=0;destinationCopyVerified=false;framebufferLogged=false;destinationCopyFormat=0;
        warm.clear();preparedImages=null;warmResolving=false;warmInputKey="";warmResolvedKey="";warmDesired=null;
        mainDestination.clear();screenDestination.clear();screenDestinationTextureCount=0;
        windowMask.contextLost();plainImage.contextLost();windowErrors.clear();waveRenderer.contextLost();screenWaveRenderer.contextLost();waveErrors.clear();windowImageStage=false;windowChildStage=false;
        noiseRenderer.contextLost();noiseErrors.clear();alphaRuleRenderer.contextLost();alphaRuleNeedsRebuild=true;
        transitionFrame=null;nativeTextTexture=null;nativeTextKey="";transitionState.clear();languageFrame=null;
        movieFrame=null;movieFrameId="";movieFramebuffer=0;movieProgram=0;movieGpuFailure="";
        program=GLES30.glCreateProgram();int v=shader(GLES30.GL_VERTEX_SHADER,VERTEX),f=shader(GLES30.GL_FRAGMENT_SHADER,FRAGMENT);GLES30.glAttachShader(program,v);GLES30.glAttachShader(program,f);GLES30.glLinkProgram(program);int[] ok=new int[1];GLES30.glGetProgramiv(program,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetProgramInfoLog(program));GLES30.glDeleteShader(v);GLES30.glDeleteShader(f);
        int[] white=new int[1];GLES30.glGenTextures(1,white,0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,white[0]);ByteBuffer pixel=ByteBuffer.allocateDirect(4);pixel.put(new byte[]{-1,-1,-1,-1}).position(0);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA,1,1,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,pixel);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);solid=new Texture(white[0],1,1);
        position=GLES30.glGetAttribLocation(program,"aPosition");uv=GLES30.glGetAttribLocation(program,"aUv");sampler=GLES30.glGetUniformLocation(program,"uImage");size=GLES30.glGetUniformLocation(program,"uSize");kernel=GLES30.glGetUniformLocation(program,"uKernel");opacity=GLES30.glGetUniformLocation(program,"uOpacity");blur=GLES30.glGetUniformLocation(program,"uBlur");tint=GLES30.glGetUniformLocation(program,"uTint");boundsUniform=GLES30.glGetUniformLocation(program,"uBounds");GLES30.glEnable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);
        blendUniform=GLES30.glGetUniformLocation(program,"uBlend");destinationUniform=GLES30.glGetUniformLocation(program,"uDestination");viewportUniform=GLES30.glGetUniformLocation(program,"uViewport");
        colorModeUniform=GLES30.glGetUniformLocation(program,"uColorMode");contrastUniform=GLES30.glGetUniformLocation(program,"uContrast");brightnessUniform=GLES30.glGetUniformLocation(program,"uBrightness");rgbUniform=GLES30.glGetUniformLocation(program,"uRgb");sepiaUniform=GLES30.glGetUniformLocation(program,"uSepia");loadColorPalette(3);
        premultipliedUniform=GLES30.glGetUniformLocation(program,"uPremultiplied");
        blendFlagUniform=GLES30.glGetUniformLocation(program,"uBlendFlag");
        rawOutputUniform=GLES30.glGetUniformLocation(program,"uRawOutput");
        noiseSamplerUniform=GLES30.glGetUniformLocation(program,"uNoise");noiseOrientationUniform=GLES30.glGetUniformLocation(program,"uNoiseOrientation");noiseFinal28Uniform=GLES30.glGetUniformLocation(program,"uNoiseFinal28");
        try{movieProgram=linkedProgram(MOVIE_VERTEX,MOVIE_FRAGMENT);moviePosition=GLES30.glGetAttribLocation(movieProgram,"aPosition");movieUv=GLES30.glGetAttribLocation(movieProgram,"aUv");movieSampler=GLES30.glGetUniformLocation(movieProgram,"uMovie");movieTransform=GLES30.glGetUniformLocation(movieProgram,"uTransform");}catch(RuntimeException error){movieGpuFailure="OES shader: "+error.getMessage();}
        maskProgram=linkedProgram(VERTEX,MASK_FRAGMENT);maskPosition=GLES30.glGetAttribLocation(maskProgram,"aPosition");maskUv=GLES30.glGetAttribLocation(maskProgram,"aUv");maskOld=GLES30.glGetUniformLocation(maskProgram,"uOldFrame");maskRule=GLES30.glGetUniformLocation(maskProgram,"uRule");maskArea=GLES30.glGetUniformLocation(maskProgram,"uArea");maskHeight=GLES30.glGetUniformLocation(maskProgram,"uHeight");maskProgress=GLES30.glGetUniformLocation(maskProgram,"uProgress");maskStrength=GLES30.glGetUniformLocation(maskProgram,"uStrength");
        movies.onSurfaceCreated();
    }
    @Override public void onSurfaceChanged(GL10 unused,int w,int h){width=w;height=h;destinationWidth=0;destinationHeight=0;destinationCopyVerified=false;framebufferLogged=false;GLES30.glViewport(0,0,w,h);}
    @Override public void onDrawFrame(GL10 unused){
        waveFrameFailed=false;
        noiseFrameFailed=false;alphaRuleFrameFailed=false;particleFrameFailed=false;
        if(desired!=null)updateWarmHints(desired);
        updateMovieFrame(movies.frameOnGl());
        Scene candidate=desired,s=scene;trimWaves(scene,desired);trimNoise(scene,desired);trimScreenWaves(scene,desired);
        if(candidate!=null&&candidate!=scene){boolean ready=true;for(String key:requiredImages(candidate)){if(!textures.contains(key)){load(key);ready=false;}}if(ready)s=candidate;}
        if(!languageTarget.isEmpty()){
            if(candidate==null||!languageTarget.equals(candidate.json.optString("language"))||s!=candidate){
                clearFrame();if(languageFrame!=null){resetColor();GLES30.glDisable(GLES30.GL_SCISSOR_TEST);drawFrameTexture(languageFrame,1);}return;
            }
        }
        trimTextures();
        Viewport vp=viewport;if(s==null||vp==null||width==0||height==0){clearFrame();return;}
        syncNoiseTransition(s);
        boolean recaptured=false;Scene.Transition transition=s.transition;String key=transitionFrameKey(s,vp);
        if(transition!=null){
            if(!TransitionFrameState.supported(transition.mode)||transition.old==null||transition.id.isEmpty()){errors.accept("잘못된 화면 전환 상태");return;}
            if(transition.mode==1){recaptured=renderCut(s,vp,key);}else{
            if(transitionState.needsCapture(transition.id,key)){
                Viewport previous=textRenderState==null?vp:textRenderState.viewportFor(transition.old,vp);renderFrame(transition.old,previous,true);if(waveFrameFailed)return;
                if(noiseFrameFailed)return;if(alphaRuleFrameFailed)return;if(particleFrameFailed)return;
                if(!captureDestination())return;
                release(transitionFrame);transitionFrame=new Texture(destinationTexture,width,height);
                // Detach the completed frame: later native blend/ambient copies must not overwrite it.
                destinationTexture=0;destinationWidth=0;destinationHeight=0;destinationCopyVerified=false;
                transitionState.captured(transition.id,key);recaptured=true;
            }
            renderFrame(s,vp,true);resetColor();GLES30.glDisable(GLES30.GL_SCISSOR_TEST);
            if(transition.maskId>0&&!transition.mask.isEmpty())drawMaskTransition(transitionFrame,transition,vp);else drawFrameTexture(transitionFrame,transition.oldOpacity);
            }
        }else{
            if(transitionFrame!=null){release(transitionFrame);transitionFrame=null;}transitionState.clear();releaseNativeText();
            renderFrame(s,vp,false);
        }
        if(waveFrameFailed)return;
        if(noiseFrameFailed)return;if(alphaRuleFrameFailed)return;if(particleFrameFailed)return;
        alphaRuleNeedsRebuild=false;
        if(!languageTarget.isEmpty()){languageTarget="";release(languageFrame);languageFrame=null;}
        noisePresented=new HashMap<>(noiseRendered);
        GLES30.glDisable(GLES30.GL_SCISSOR_TEST);
        if(s!=scene||recaptured){scene=s;committed.accept(s);}trimWaves(scene,desired);trimNoise(scene,desired);trimScreenWaves(scene,desired);trimAlphaRules(scene,desired);
        pumpWarm();
    }
    private boolean renderCut(Scene value,Viewport vp,String key){
        release(transitionFrame);transitionFrame=null;
        renderFrame(value,vp,true);
        if(waveFrameFailed)return false;
        if(noiseFrameFailed)return false;
        boolean prepared=transitionState.needsCapture(value.transition.id,key);
        transitionState.captured(value.transition.id,key);return prepared;
    }
    private void clearFrame(){GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glClearColor(0,0,0,1);GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);}
    private void renderFrame(Scene s,Viewport vp,boolean includeText){
        clearFrame();GLES30.glUseProgram(program);GLES30.glEnable(GLES30.GL_SCISSOR_TEST);
        if(MovieRenderGate.active(movieId(s),movieFrameId,movieFrame!=null)){if(s!=noiseOldScene)noiseRendered=new HashMap<>();resetColor();scissor(vp.frame);JSONArray source=new JSONArray();source.put(0);source.put(movieFrame.height);source.put(movieFrame.width);source.put(-movieFrame.height);draw(movieFrame,vp.frame,1,false,source,0xffffffff,0,0,null,.5f,.5f);if(ambient&&vp.hasLetterboxes())drawAmbient(vp);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);return;}
        boolean fillsArea=false;
        Map<Integer,Texture> noiseSources=prepareNoiseSources(s);if(noiseFrameFailed)return;if(alphaRuleFrameFailed)return;if(particleFrameFailed)return;
        Map<Integer,Texture> waveSources=prepareWaveSources(s);if(waveFrameFailed)return;
        if(s.screenWave==null){
            fillsArea=renderLayers(s,vp,noiseSources,waveSources,Integer.MIN_VALUE,Integer.MAX_VALUE);
            if(ambient&&!fillsArea&&vp.hasLetterboxes())drawAmbient(vp);
        }else{
            try{
                Texture raster=screenWaveSource(s,vp,noiseSources,waveSources);if(waveFrameFailed||noiseFrameFailed)return;
                fillsArea=renderLayers(s,vp,noiseSources,waveSources,Integer.MIN_VALUE,GlobalScreenWave.CAPTURE_MAX_PRIORITY);
                if(ambient&&!fillsArea&&vp.hasLetterboxes())drawAmbient(vp);
                resetColor();scissor(vp.content);draw(raster,vp.content,1,false,canvasSource(),0xffffffff,0,0,null,.5f,.5f);
                renderLayers(s,vp,noiseSources,waveSources,GlobalScreenWave.PRIORITY,Integer.MAX_VALUE);
            }catch(RuntimeException error){waveError(new IllegalStateException("global Screen Raster: "+error.getMessage(),error));return;}
        }
        if(includeText&&!hiddenText&&TextOverlay.captureInFrame(s,vp,desired,viewport))drawNativeText(s,vp);
        GLES30.glDisable(GLES30.GL_SCISSOR_TEST);
    }
    private boolean renderLayers(Scene s,Viewport vp,Map<Integer,Texture> noiseSources,Map<Integer,Texture> waveSources,int minimumPriority,int maximumPriority){
        boolean fillsArea=false;int particle=0;
        for(JSONObject layer:s.layers){if(layer.optInt("parent",-1)<0)particle=renderParticles(s,vp,particle,layer.optDouble("z",0),minimumPriority,maximumPriority);if(layer.optInt("parent",-1)<0&&(layer.optDouble("z",0)<minimumPriority||layer.optDouble("z",0)>maximumPriority))continue;if(layer.optInt("parent",-1)>=0){int parent=layer.optInt("parent",-1);boolean exists=false;for(JSONObject other:s.layers)if(other.optInt("id",-2)==parent){exists=true;break;}if(!exists)windowError(new IllegalArgumentException("Missing native window parent "+parent));continue;}if(!Residency.visible(layer.optDouble("alpha",1))&&!zeroAlphaWave40(layer))continue;int blend=layer.optInt("blend",0);if(!BlendSupport.supported(blend)){if(unsupportedBlends.add(blend))errors.accept("미지원 원본 합성 모드: "+blend);continue;}if(!BlendSupport.rootContext(blend,layer.optInt("parent",-1)>=0,layer.optJSONObject("window")!=null,layer.optJSONObject("wave")!=null||layer.optInt("decimalMode",0)%10!=0)){windowError(new IllegalArgumentException("Unsupported root blend18 window/wave/noise context"));continue;}int colorMode=layer.optInt("colorMode",0);if(!ColorSupport.supported(colorMode)){if(unsupportedColors.add(colorMode))errors.accept("미지원 원본 색상 모드: "+colorMode);continue;}if(colorMode==3||ColorSupport.channelPalette(colorMode)){if(!colorPaletteTextures.containsKey(colorMode))loadColorPalette(colorMode);Integer palette=colorPaletteTextures.get(colorMode);if(palette==null)continue;GLES30.glActiveTexture(GLES30.GL_TEXTURE2);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,palette);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);}String image=layer.optString("image","");if(image.isEmpty()&&!layer.has("color"))continue;String key=image.isEmpty()?"":imageKey(image);Texture texture=image.isEmpty()?solid:textures.get(key);if(texture==null){load(key);continue;}
            GLES30.glUniform1i(colorModeUniform,colorMode);GLES30.glUniform1i(contrastUniform,layer.optInt("colorValue",0));GLES30.glUniform1i(brightnessUniform,layer.optInt("brightness",0));GLES30.glUniform1i(rgbUniform,layer.optInt("rgb",0));GLES30.glUniform1i(sepiaUniform,2);
            String kind=layer.optString("kind","image");boolean event=kind.equals("event")||kind.equals("cg");boolean background=event||kind.equals("background")||kind.equals("bg");
            boolean cover=event?eventCover:bgCover;
            if(background&&dynamic&&cover)fillsArea=true;
            scissor(vp.clip(background,cover,dynamic));
            float x=(float)layer.optDouble("x",0),y=(float)layer.optDouble("y",0),w=(float)layer.optDouble("w",s.width),h=(float)layer.optDouble("h",s.height),a=(float)layer.optDouble("alpha",1);
            if(blend==18)a=WindowMask.childOpacity(layer.optDouble("alpha",1));
            if(layer.optJSONObject("wave")!=null&&((blend==16&&layer.optJSONObject("wave").optInt("preset",0)==19)||(blend==17&&layer.optJSONObject("wave").optInt("preset",0)==22)||(blend==22&&layer.optJSONObject("wave").optInt("preset",0)==23)||(layer.optInt("parent",-1)<0&&layer.optJSONObject("wave").optInt("preset",0)>=24&&layer.optJSONObject("wave").optInt("preset",0)<=48)))a=WaveMesh.nativeRootOpacity(layer.optDouble("alpha",1));
            int color=0xffffffff;Object c=layer.opt(image.isEmpty()?"color":"tint");try{if(c instanceof Number)color=((Number)c).intValue()|0xff000000;else if(c instanceof String)color=android.graphics.Color.parseColor((String)c);}catch(IllegalArgumentException ignored){}
            float pixelOffsetX=(float)layer.optDouble("pixelOffsetX",0),pixelOffsetY=(float)layer.optDouble("pixelOffsetY",0);
            if(layer.optJSONObject("alphaRule")!=null){try{drawAlphaRule(texture,layer,s,vp,background,cover);}catch(RuntimeException error){alphaRuleFrameFailed=true;errors.accept("원본 mask7: "+error.getMessage());}continue;}
            if(layer.optJSONObject("noise")!=null){try{drawNoise(noiseSources.get(layer.optInt("id",-1)),layer,s,vp,background,cover,a,blend);}catch(RuntimeException error){noiseError(error);}continue;}
            if(layer.optJSONObject("wave")!=null&&layer.optJSONObject("window")==null){try{drawWave(waveSources.get(layer.optInt("id",-1)),layer,s,vp,background,cover,a,blend);}catch(RuntimeException error){waveError(error);}continue;}
            JSONObject window=layer.optJSONObject("window");if(window!=null){
                try{WindowMask.Spec spec=WindowMask.Spec.from(window);Texture prepared=windowLayer(spec,texture,layer,s.layers,waveSources,x+pixelOffsetX,y+pixelOffsetY,w,h,color);if(prepared!=null){resetColor();scissor(vp.clip(background,cover,dynamic));JSONArray flipped=new JSONArray();flipped.put(0);flipped.put(WindowMask.HEIGHT);flipped.put(WindowMask.WIDTH);flipped.put(-WindowMask.HEIGHT);int family=layer.optInt("finalCallback28",0);if(family!=0&&family!=120)throw new IllegalArgumentException("Unreviewed window final28 family");GLES30.glUniform1i(noiseFinal28Uniform,family);try{RectF groupDestination=vp.image(0,0,WindowMask.WIDTH,WindowMask.HEIGHT,background,cover,dynamic,s.width,s.height);if(family!=0||!tryPlain(prepared,groupDestination,a,flipped,0xffffffff,0,blend,.5f,.5f,0,1,1))draw(prepared,groupDestination,family==120?WindowMask.childOpacity(layer.optDouble("alpha",1)):a,false,flipped,0xffffffff,0,family==120?28:blend,null,.5f,.5f,layer.optInt("blendFlag",0));}finally{GLES30.glUniform1i(noiseFinal28Uniform,0);}}}catch(RuntimeException error){if(layer.optJSONObject("wave")!=null||hasWaveChildren(layer,s.layers))waveError(error);else windowError(error);}continue;
            }
            RectF destination=vp.image(x+pixelOffsetX,y+pixelOffsetY,w,h,background,cover,dynamic,s.width,s.height);
            drawLayer(texture,destination,a,layer.optJSONArray("source"),color,layer,blend);
        }
        renderParticles(s,vp,particle,Double.POSITIVE_INFINITY,minimumPriority,maximumPriority);
        return fillsArea;
    }
    private int renderParticles(Scene s,Viewport vp,int cursor,double before,int minimum,int maximum){
        while(cursor<s.particles.sprites.size()){
            ParticleFrame.Sprite sprite=s.particles.sprites.get(cursor);if(sprite.priority>=before)break;cursor++;
            if(!sprite.inPriorityRange(minimum,maximum)||sprite.opacity8==0)continue;
            try{Texture texture=textures.get(imageKey(sprite.resource));if(texture==null)throw new IllegalStateException("Particle texture not ready: "+sprite.resource);
            if(sprite.source[0]+sprite.source[2]>texture.originalWidth||sprite.source[1]+sprite.source[3]>texture.originalHeight)throw new IllegalArgumentException("Particle atlas source outside texture");
            resetColor();scissor(vp.clip(true,eventCover,dynamic));
            float u0=sprite.source[0]/texture.originalWidth,v0=sprite.source[1]/texture.originalHeight,u1=(sprite.source[0]+sprite.source[2])/texture.originalWidth,v1=(sprite.source[1]+sprite.source[3])/texture.originalHeight;
            GLES30.glUniform4f(boundsUniform,u0+.5f/texture.width,v0+.5f/texture.height,u1-.5f/texture.width,v1-.5f/texture.height);
            GLES30.glUniform1i(blendUniform,0);GLES30.glUniform1i(blendFlagUniform,0);GLES30.glUniform1i(rawOutputUniform,0);GLES30.glUniform2f(viewportUniform,width,height);
            vertices.clear();for(int i=0;i<4;i++){RectF point=vp.image(sprite.quad[i*2],sprite.quad[i*2+1],0,0,true,eventCover,dynamic,s.width,s.height);vertices.put(point.left/width*2-1).put(1-point.top/height*2).put(i<2?u0:u1).put(i%2==0?v0:v1);}vertices.flip();
            bindVertices(position,uv);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,texture.id);GLES30.glUniform1i(sampler,0);GLES30.glUniform2f(size,texture.width,texture.height);GLES30.glUniform1i(kernel,filter);GLES30.glUniform1i(premultipliedUniform,texture.premultiplied?1:0);GLES30.glUniform1i(blur,0);GLES30.glUniform1f(opacity,sprite.alpha());GLES30.glUniform4f(tint,1,1,1,1);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);
            }catch(RuntimeException error){particleFrameFailed=true;errors.accept("원본 particle: "+error.getMessage());return cursor;}
        }return cursor;
    }
    private static void alphaRuleTokens(Scene s,Set<String> tokens){if(s==null)return;for(JSONObject l:s.layers){JSONObject rule=l.optJSONObject("alphaRule");if(rule!=null)tokens.add(rule.optString("token",""));}if(s.transition!=null&&s.transition.mode==0)alphaRuleTokens(s.transition.old,tokens);}
    private void trimAlphaRules(Scene a,Scene b){Set<String> tokens=new HashSet<>();alphaRuleTokens(a,tokens);alphaRuleTokens(b,tokens);alphaRuleRenderer.retain(tokens);}
    private static String alphaRuleImage(JSONObject spec){int rule=spec.optInt("rule",0);String expected=(rule<10?"rule_0":"rule_")+rule+".cbg";if(rule<1||rule>14||!expected.equals(spec.optString("image","")))throw new IllegalArgumentException("Invalid mask7 DTO");return expected;}
    private void drawAlphaRule(Texture source,JSONObject layer,Scene s,Viewport vp,boolean background,boolean cover){
        JSONObject spec=layer.optJSONObject("alphaRule");
        if(spec==null||spec.optInt("sourceFormat",0)!=2||spec.optInt("reference",-1)!=0||spec.optInt("liveAlphaFixed16",0)!=65536||spec.optString("token","").isEmpty())throw new IllegalArgumentException("Invalid mask7 DTO");
        String ruleName=alphaRuleImage(spec);int blend=layer.optInt("blend",0),colorMode=layer.optInt("colorMode",0),rgb=layer.optInt("rgb",0);
        if(layer.optString("image","").isEmpty()||layer.optInt("parent",-1)>=0||(blend!=0&&blend!=16)||layer.optInt("blendFlag",0)!=0||(colorMode!=0&&colorMode!=1)||layer.optInt("colorValue",0)!=0||layer.optInt("brightness",0)!=0||(colorMode==0?rgb!=0:rgb<0||rgb>0xffffff)||layer.optJSONArray("source")!=null||layer.optJSONObject("window")!=null||layer.optJSONObject("wave")!=null||layer.optJSONObject("noise")!=null||s.screenWave!=null)throw new IllegalArgumentException("Unreviewed mask7 material/context");
        for(JSONObject child:s.layers)if(child.optInt("parent",-1)==layer.optInt("id",-2))throw new IllegalArgumentException("Unreviewed mask7 group");
        Texture rule=textures.get(imageKey(ruleName));if(rule==null){load(imageKey(ruleName));alphaRuleFrameFailed=true;return;}
        if(source.originalWidth<=0||source.originalHeight<=0||source.originalWidth>8192||source.originalHeight>8192||source.premultiplied||rule.originalWidth!=1920||rule.originalHeight!=1080)throw new IllegalArgumentException("Wrong mask7 source/rule dimensions");
        final float x=(float)(layer.optDouble("x",0)+layer.optDouble("pixelOffsetX",0)),y=(float)(layer.optDouble("y",0)+layer.optDouble("pixelOffsetY",0));
        final RectF destination=vp.normalizedContent(1920,1080).image(x,y,(float)layer.optDouble("w",0),(float)layer.optDouble("h",0),background,cover,dynamic,s.width,s.height);
        final float rotation=(float)layer.optDouble("rotation",0),px=(float)layer.optDouble("pivotX",.5),py=(float)layer.optDouble("pivotY",.5);
        final int matrix=layer.optInt("matrixMode",0);final float iw=(float)layer.optDouble("intrinsicWidth",1),ih=(float)layer.optDouble("intrinsicHeight",1);
        int target=alphaRuleRenderer.render(spec.optString("token",""),source.id+":"+rule.id+":"+layer+":"+frameKey(vp),spec.optInt("P",-1),spec.optInt("S",-1),VERTEX,maskProgram->{
            int previousWidth=width,previousHeight=height;float[] previousProjection=QuadGeometry.captureProjection.get();
            try{width=1920;height=1080;QuadGeometry.captureProjection.set(new float[]{vp.content.left,vp.content.top,vp.content.width(),vp.content.height(),previousWidth,previousHeight});
                GLES30.glEnable(GLES30.GL_SCISSOR_TEST);scissor(vp.normalizedContent(1920,1080).clip(background,cover,dynamic));
                QuadGeometry.write(vertices,destination.left,destination.top,destination.right,destination.bottom,rotation,1920,1080,0,0,1,1,px,py,matrix,iw,ih);QuadGeometry.applyCapture(vertices,destination.left,destination.top,destination.right,destination.bottom,rotation,0,0,1,1,px,py,matrix,iw,ih);
                GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,source.id);linearTexture();GLES30.glActiveTexture(GLES30.GL_TEXTURE1);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,rule.id);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_NEAREST);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
                bindVertices(GLES30.glGetAttribLocation(maskProgram,"aPosition"),GLES30.glGetAttribLocation(maskProgram,"aUv"));GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);
            }finally{width=previousWidth;height=previousHeight;QuadGeometry.captureProjection.set(previousProjection);}
        });
        // Native135 already consumes the threshold and source alpha. Its live
        // object alpha is65536: do not multiply logical alpha or fade progress.
        // Color1 preserves alpha and replaces RGB, so apply it once after the
        // rawRGBA mask pass, then use the existing root16 composition unchanged.
        GLES30.glUseProgram(program);resetColor();GLES30.glUniform1i(colorModeUniform,colorMode);GLES30.glUniform1i(rgbUniform,rgb);scissor(vp.content);alphaRuleFinalStage=true;try{draw(new Texture(target,1920,1080),vp.content,1,false,canvasSource(),0xffffffff,0,blend,null,.5f,.5f,0);}finally{alphaRuleFinalStage=false;}
    }
    private static void screenWaveIds(Scene value,Set<String> ids){if(value==null)return;if(value.screenWave!=null)ids.add(value.screenWave.id);if(value.transition!=null&&value.transition.mode==0)screenWaveIds(value.transition.old,ids);}
    private void trimScreenWaves(Scene current,Scene next){Set<String> ids=new HashSet<>();screenWaveIds(current,ids);screenWaveIds(next,ids);screenWaveRenderer.retain(ids);if(ids.isEmpty())releaseScreenDestination();}
    private static final class DestinationState {
        int texture,width,height,format;boolean verified,logged;
        void clear(){texture=width=height=format=0;verified=logged=false;}
    }
    private void storeDestination(DestinationState state){state.texture=destinationTexture;state.width=destinationWidth;state.height=destinationHeight;state.format=destinationCopyFormat;state.verified=destinationCopyVerified;state.logged=framebufferLogged;if(state==screenDestination)screenDestinationTextureCount=state.texture==0?0:1;}
    private void loadDestination(DestinationState state){destinationTexture=state.texture;destinationWidth=state.width;destinationHeight=state.height;destinationCopyFormat=state.format;destinationCopyVerified=state.verified;framebufferLogged=state.logged;}
    private void releaseScreenDestination(){if(screenDestination.texture!=0)GLES30.glDeleteTextures(1,new int[]{screenDestination.texture},0);screenDestination.clear();screenDestinationTextureCount=0;}
    private Texture screenWaveSource(Scene s,Viewport vp,Map<Integer,Texture> noiseSources,Map<Integer,Texture> waveSources){
        try{int id=screenWaveRenderer.render(s.screenWave,()->{
            int previousWidth=width,previousHeight=height;float[] previousProjection=QuadGeometry.captureProjection.get();
            storeDestination(mainDestination);loadDestination(screenDestination);
            QuadGeometry.captureProjection.set(new float[]{vp.content.left,vp.content.top,vp.content.width(),vp.content.height(),previousWidth,previousHeight});
            try{width=GlobalScreenWave.WIDTH;height=GlobalScreenWave.HEIGHT;Viewport normalized=vp.normalizedContent(width,height);GLES30.glUseProgram(program);GLES30.glEnable(GLES30.GL_SCISSOR_TEST);
                boolean fills=renderLayers(s,normalized,noiseSources,waveSources,Integer.MIN_VALUE,GlobalScreenWave.CAPTURE_MAX_PRIORITY);
                if(ambient&&!fills&&normalized.hasLetterboxes())drawAmbient(normalized);
            }finally{width=previousWidth;height=previousHeight;QuadGeometry.captureProjection.set(previousProjection);storeDestination(screenDestination);loadDestination(mainDestination);}
        },(snapshot,mesh,count)->{
            GLES30.glUseProgram(program);resetColor();GLES30.glUniform1i(rawOutputUniform,1);GLES30.glUniform1i(blendUniform,0);GLES30.glUniform1i(blendFlagUniform,0);GLES30.glUniform1i(kernel,0);GLES30.glUniform1i(blur,0);GLES30.glUniform1i(premultipliedUniform,0);GLES30.glUniform1f(opacity,1);GLES30.glUniform4f(tint,1,1,1,1);
            GLES30.glUniform4f(boundsUniform,.5f/GlobalScreenWave.WIDTH,.5f/GlobalScreenWave.HEIGHT,1-.5f/GlobalScreenWave.WIDTH,1-.5f/GlobalScreenWave.HEIGHT);
            GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,snapshot);linearTexture();GLES30.glUniform1i(sampler,0);GLES30.glUniform2f(size,GlobalScreenWave.WIDTH,GlobalScreenWave.HEIGHT);
            mesh.position(0);GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,16,mesh);GLES30.glEnableVertexAttribArray(position);mesh.position(2);GLES30.glVertexAttribPointer(uv,2,GLES30.GL_FLOAT,false,16,mesh);GLES30.glEnableVertexAttribArray(uv);GLES30.glDrawArrays(GLES30.GL_TRIANGLES,0,count);mesh.position(0);
        });
        return new Texture(id,GlobalScreenWave.WIDTH,GlobalScreenWave.HEIGHT);
        }finally{GLES30.glUseProgram(program);GLES30.glUniform1i(rawOutputUniform,0);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);}
    }
    private String parentCacheKey(WindowMask.Spec spec,Texture texture,JSONObject layer,float x,float y,float w,float h,int color){
        if(!plainEnabled||!spec.identity()||layer.optJSONObject("wave")!=null||layer.optInt("colorMode",0)!=0||layer.optInt("brightness",0)!=0||layer.optInt("colorValue",0)!=0)return null;
        return layer.optString("image","")+":"+texture.id+":"+plainImage.samplingKey()+":"+filter+":"+color+":"+x+","+y+","+w+","+h+":"+layer.optJSONArray("source")+":"+layer.optDouble("rotation",0)+":"+layer.optDouble("pivotX",.5)+","+layer.optDouble("pivotY",.5)+":"+layer.optInt("matrixMode",0)+":"+layer.optDouble("intrinsicWidth",0)+","+layer.optDouble("intrinsicHeight",0);
    }
    private Texture windowLayer(WindowMask.Spec spec,Texture texture,JSONObject layer,List<JSONObject> layers,Map<Integer,Texture> waveSources,float x,float y,float w,float h,int color){
        int normal=0,inverse=0;for(String name:spec.assets()){Texture atlas=textures.get(imageKey(name));if(atlas==null)return null;if(atlas.width!=spec.atlas||atlas.height!=spec.atlas)throw new IllegalArgumentException("Wrong native window atlas dimensions: "+name);if(name.contains("-inv-"))inverse=atlas.id;else normal=atlas.id;}
        boolean scissor=GLES30.glIsEnabled(GLES30.GL_SCISSOR_TEST);
        int read=glInteger(GLES30.GL_READ_FRAMEBUFFER_BINDING),write=glInteger(GLES30.GL_DRAW_FRAMEBUFFER_BINDING);
        try{List<JSONObject> children=WindowMask.children(layers,layer);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);boolean paintBase=windowMask.begin(parentCacheKey(spec,texture,layer,x,y,w,h,color));GLES30.glUseProgram(program);windowImageStage=true;identityChildStage=spec.identity();
            if(layer.optJSONObject("wave")!=null){Texture waved=waveSources.get(layer.optInt("id",-1));if(waved==null)throw new IllegalStateException("Native own-window wave not ready");resetColor();draw(waved,new RectF(0,0,WaveMesh.WIDTH,WaveMesh.HEIGHT),1,false,canvasSource(),0xffffffff,0,0,null,.5f,.5f,0);}
            else if(paintBase)drawLayer(texture,new RectF(x,y,x+w,y+h),1,layer.optJSONArray("source"),color,layer,0);
            for(JSONObject child:children){float childAlpha=WindowMask.childOpacity(child);if(childAlpha<=0)continue;String image=child.optString("image","");boolean waved=child.optJSONObject("wave")!=null;Texture childTexture=waved?waveSources.get(child.optInt("id",-1)):image.isEmpty()?solid:textures.get(imageKey(image));if(childTexture==null)throw new IllegalStateException("Native child image not ready: "+image);windowMask.beginChild(spec,normal,program,!waved&&coversWindow(child));if(waved){resetColor();windowChildStage=true;draw(childTexture,new RectF(0,0,WaveMesh.WIDTH,WaveMesh.HEIGHT),childAlpha,false,canvasSource(),0xffffffff,0,child.optInt("blend",0),null,.5f,.5f,child.optInt("blendFlag",0));windowChildStage=false;continue;}int mode=child.optInt("colorMode",0);if(!ColorSupport.supported(mode))throw new IllegalArgumentException("Unsupported native child color "+mode);if(mode==3||ColorSupport.channelPalette(mode)){if(!colorPaletteTextures.containsKey(mode))loadColorPalette(mode);Integer palette=colorPaletteTextures.get(mode);if(palette==null)throw new IllegalStateException("Native child palette not ready");GLES30.glActiveTexture(GLES30.GL_TEXTURE2);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,palette);}GLES30.glUniform1i(colorModeUniform,mode);GLES30.glUniform1i(contrastUniform,child.optInt("colorValue",0));GLES30.glUniform1i(brightnessUniform,child.optInt("brightness",0));GLES30.glUniform1i(rgbUniform,child.optInt("rgb",0));GLES30.glUniform1i(sepiaUniform,2);int childColor=0xffffffff;Object tint=child.opt(image.isEmpty()?"color":"tint");if(tint instanceof Number)childColor=((Number)tint).intValue()|0xff000000;else if(tint instanceof String)childColor=android.graphics.Color.parseColor((String)tint);float cx=(float)(child.optDouble("x",0)+child.optDouble("pixelOffsetX",0)),cy=(float)(child.optDouble("y",0)+child.optDouble("pixelOffsetY",0));windowChildStage=true;drawLayer(childTexture,new RectF(cx,cy,cx+(float)child.optDouble("w",WindowMask.WIDTH),cy+(float)child.optDouble("h",WindowMask.HEIGHT)),childAlpha,child.optJSONArray("source"),childColor,child,child.optInt("blend",0));windowChildStage=false;}
            windowImageStage=false;int id=windowMask.apply(spec,normal,inverse);return new Texture(id,WindowMask.WIDTH,WindowMask.HEIGHT);}
        finally{windowImageStage=false;windowChildStage=false;identityChildStage=false;GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,write);GLES30.glViewport(0,0,width,height);if(scissor)GLES30.glEnable(GLES30.GL_SCISSOR_TEST);else GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glEnable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);GLES30.glUseProgram(program);}
    }
    private void windowError(RuntimeException error){String message="원본 window: "+error.getMessage();if(windowErrors.add(message))errors.accept(message);}
    private void waveError(RuntimeException error){waveFrameFailed=true;waveRenderer.noteError(error.getMessage());String message="원본 wave: "+error.getMessage();if(waveErrors.add(message))errors.accept(message);}
    private static void waveIds(Scene value,Set<Integer> ids){if(value==null)return;for(JSONObject layer:value.layers)if(layer.optJSONObject("wave")!=null)ids.add(layer.optInt("id",-1));if(value.transition!=null&&value.transition.mode==0)waveIds(value.transition.old,ids);}
    private void trimWaves(Scene current,Scene next){Set<Integer> ids=new HashSet<>();waveIds(current,ids);waveIds(next,ids);waveRenderer.retain(ids);}
    private Map<Integer,Texture> prepareWaveSources(Scene scene){
        Map<Integer,Texture> result=new HashMap<>();
        for(JSONObject layer:scene.layers)if(layer.optJSONObject("wave")!=null)try{
            validateWaveSource(layer,scene.layers);String image=layer.optString("image","");Texture texture=textures.get(imageKey(image));if(texture==null){load(imageKey(image));waveFrameFailed=true;continue;}
            configureSourceColor(layer);result.put(layer.optInt("id",-1),waveSource(texture,layer,sourceTint(layer)));
        }catch(RuntimeException error){waveError(error);}return result;
    }
    private static void validateWaveSource(JSONObject layer,List<JSONObject> layers){
        int ownPreset=layer.optJSONObject("wave").optInt("preset",0);
        boolean ownWindow=(ownPreset==43||ownPreset==44)&&layer.optInt("parent",-1)<0&&layer.optInt("blend",0)==0&&layer.optInt("blendFlag",0)==0;
        if((layer.optJSONObject("window")!=null&&!ownWindow)||layer.optJSONArray("source")!=null||layer.optString("kind","").equals("ui")||layer.optString("image","").isEmpty())throw new IllegalArgumentException("Unreviewed window/cropped/UI/solid wave");
        for(JSONObject child:layers)if(child.optInt("parent",-1)==layer.optInt("id",-2))throw new IllegalArgumentException("Unreviewed wave group/nesting");
        int preset=layer.optJSONObject("wave").optInt("preset",0),parentId=layer.optInt("parent",-1);
        if(preset>=24&&preset<=36){int[] blends={0,0,0,0,0,0,0,11,0,19,0,0,0},colours={39,39,39,39,0,39,39,70,52,0,27,0,0};if((preset==35?parentId<0:parentId>=0)||layer.optInt("blend",0)!=blends[preset-24]||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=colours[preset-24]||layer.optInt("colorValue",0)!=(preset==33?89:0)||layer.optInt("brightness",0)!=0||layer.optInt("rgb",0)!=0)throw new IllegalArgumentException("Unreviewed wave24..36 Native context");}
        if(preset>=37&&preset<=40){int[] blends={0,20,11,17},flags={0,0,0,1},colours={0,39,37,0};if(parentId>=0||layer.optInt("blend",0)!=blends[preset-37]||layer.optInt("blendFlag",0)!=flags[preset-37]||layer.optInt("colorMode",0)!=colours[preset-37]||layer.optInt("colorValue",0)!=0||layer.optInt("brightness",0)!=0||layer.optInt("rgb",0)!=0)throw new IllegalArgumentException("Unreviewed wave37..40 Native context");if(preset==40&&!(layer.opt("enabled") instanceof Boolean))throw new IllegalArgumentException("Missing wave40 enabled state");}
        if(preset>=41&&preset<=48&&(parentId>=0||layer.optInt("blend",0)!=0||layer.optInt("blendFlag",0)!=0))throw new IllegalArgumentException("Unreviewed wave41..48 root material");
        if(preset==23&&(parentId>=0||layer.optInt("blend",0)!=22||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=1||layer.optInt("rgb",0)!=0x4ca5ff))throw new IllegalArgumentException("Unreviewed wave23 root22/flag0/color1/4CA5FF context");
        if(preset==22&&(parentId>=0||layer.optInt("blend",0)!=17||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=1||layer.optInt("rgb",0)!=0x8e0707))throw new IllegalArgumentException("Unreviewed wave22 root17/flag0/color1/8E0707 context");
        if(preset==21&&(parentId>=0||layer.optInt("blend",0)!=0||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=1||layer.optInt("rgb",0)!=0x8e0707))throw new IllegalArgumentException("Unreviewed wave21 root0/flag0/color1/8E0707 context");
        if(preset==19&&(parentId>=0||layer.optInt("blend",0)!=16||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=0||layer.optInt("rgb",0)!=0))throw new IllegalArgumentException("Unreviewed wave19 root16/flag0/color0 context");
        if(preset==20&&(parentId<0||layer.optInt("blend",0)!=11||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=1||layer.optInt("rgb",0)!=0xff1313))throw new IllegalArgumentException("Unreviewed wave20 child11/flag0/color1/FF1313 context");
        if(parentId<0)return;JSONObject parent=null;for(JSONObject candidate:layers)if(candidate.optInt("id",-2)==parentId){parent=candidate;break;}
        if(parent==null||parent.optInt("parent",-1)>=0||parent.optJSONObject("window")==null||parent.optJSONObject("wave")!=null)throw new IllegalArgumentException("Unreviewed parented wave combination");
        if(preset==20||preset==35){JSONObject w=parent.optJSONObject("window");if(parent.optInt("blend",0)!=0||parent.optInt("blendFlag",0)!=0||parent.optInt("colorMode",0)!=0||parent.optInt("rgb",0)!=0||w.optInt("x",0)!=0||w.optInt("y",0)!=0||w.optInt("width",0)!=1920||w.optInt("height",0)!=1080||w.optInt("colorMode",0)!=0||w.optInt("softEdge",0)!=0)throw new IllegalArgumentException("Unreviewed wave"+preset+" full-window root0 parent context");return;}
        if(preset!=6||layer.optInt("blend",0)!=19||layer.optInt("blendFlag",0)!=0)throw new IllegalArgumentException("Unreviewed parented wave combination");
    }
    private static boolean zeroAlphaWave40(JSONObject layer){
        JSONObject wave=layer.optJSONObject("wave");
        return wave!=null&&wave.optInt("preset",0)==40&&layer.optInt("parent",-1)<0&&layer.optInt("blend",0)==17&&layer.optInt("blendFlag",0)==1&&Boolean.TRUE.equals(layer.opt("enabled"));
    }
    private static boolean hasWaveChildren(JSONObject parent,List<JSONObject> layers){int id=parent.optInt("id",-2);for(JSONObject child:layers)if(child.optInt("parent",-1)==id&&child.optJSONObject("wave")!=null)return true;return false;}
    private void configureSourceColor(JSONObject layer){
        int mode=layer.optInt("colorMode",0);if(!ColorSupport.supported(mode))throw new IllegalArgumentException("Unsupported wave source color "+mode);
        if(mode==3||ColorSupport.channelPalette(mode)){if(!colorPaletteTextures.containsKey(mode))loadColorPalette(mode);Integer palette=colorPaletteTextures.get(mode);if(palette==null)throw new IllegalStateException("Wave palette not ready");GLES30.glActiveTexture(GLES30.GL_TEXTURE2);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,palette);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);}
        GLES30.glUniform1i(colorModeUniform,mode);GLES30.glUniform1i(contrastUniform,layer.optInt("colorValue",0));GLES30.glUniform1i(brightnessUniform,layer.optInt("brightness",0));GLES30.glUniform1i(rgbUniform,layer.optInt("rgb",0));GLES30.glUniform1i(sepiaUniform,2);
    }
    private static int sourceTint(JSONObject layer){Object value=layer.opt(layer.optString("image","").isEmpty()?"color":"tint");if(value instanceof Number)return ((Number)value).intValue()|0xff000000;return value instanceof String?android.graphics.Color.parseColor((String)value):0xffffffff;}
    private Texture waveSource(Texture texture,JSONObject layer,int color){
        float x=(float)(layer.optDouble("x",0)+layer.optDouble("pixelOffsetX",0)),y=(float)(layer.optDouble("y",0)+layer.optDouble("pixelOffsetY",0)),w=(float)layer.optDouble("w",WaveMesh.WIDTH),h=(float)layer.optDouble("h",WaveMesh.HEIGHT);
        WaveMesh.Spec spec=WaveMesh.Spec.from(layer.optJSONObject("wave"));
        QuadGeometry.write(vertices,x,y,x+w,y+h,(float)layer.optDouble("rotation",0),WaveMesh.WIDTH,WaveMesh.HEIGHT,0,0,1,1,(float)layer.optDouble("pivotX",.5),(float)layer.optDouble("pivotY",.5),layer.optInt("matrixMode",0),(float)layer.optDouble("intrinsicWidth",0),(float)layer.optDouble("intrinsicHeight",0));
        float[] affine=new float[16];vertices.get(affine);vertices.position(0);
        JSONArray source=layer.optJSONArray("source");float u0=0,v0=0,u1=1,v1=1;if(source!=null&&source.length()>=4){u0=(float)source.optDouble(0)/texture.originalWidth;v0=(float)source.optDouble(1)/texture.originalHeight;u1=u0+(float)source.optDouble(2)/texture.originalWidth;v1=v0+(float)source.optDouble(3)/texture.originalHeight;}
        final float left=u0,top=v0,right=u1,bottom=v1;
        try{
            String key=texture.id+":"+texture.originalWidth+"x"+texture.originalHeight+":"+layer.toString();
            int id=waveRenderer.render(layer.optInt("id",-1),key,texture.originalWidth,texture.originalHeight,spec,affine,u0,v0,u1,v1,(mesh,count)->{
                GLES30.glUseProgram(program);GLES30.glUniform4f(boundsUniform,Math.min(left,right)+.5f/texture.width,Math.min(top,bottom)+.5f/texture.height,Math.max(left,right)-.5f/texture.width,Math.max(top,bottom)-.5f/texture.height);
                //RGBA source -> RGBA target is native109/q0, never shader108.
                GLES30.glUniform1i(rawOutputUniform,3);GLES30.glUniform1i(blendUniform,0);GLES30.glUniform1i(blendFlagUniform,0);GLES30.glUniform1i(kernel,0);GLES30.glUniform1i(blur,0);GLES30.glUniform1i(premultipliedUniform,texture.premultiplied?1:0);GLES30.glUniform1f(opacity,1);
                GLES30.glUniform4f(tint,android.graphics.Color.red(color)/255f,android.graphics.Color.green(color)/255f,android.graphics.Color.blue(color)/255f,android.graphics.Color.alpha(color)/255f);
                GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,texture.id);GLES30.glUniform1i(sampler,0);GLES30.glUniform2f(size,texture.width,texture.height);
                mesh.position(0);GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,16,mesh);GLES30.glEnableVertexAttribArray(position);mesh.position(2);GLES30.glVertexAttribPointer(uv,2,GLES30.GL_FLOAT,false,16,mesh);GLES30.glEnableVertexAttribArray(uv);GLES30.glDrawArrays(GLES30.GL_TRIANGLES,0,count);mesh.position(0);
            });
            return new Texture(id,WaveMesh.WIDTH,WaveMesh.HEIGHT);
        }finally{GLES30.glUseProgram(program);GLES30.glUniform1i(rawOutputUniform,0);}
    }
    private void drawWave(Texture prepared,JSONObject layer,Scene scene,Viewport vp,boolean background,boolean cover,float alpha,int blend){
        if(prepared==null)throw new IllegalStateException("Wave source not prepared");resetColor();scissor(vp.clip(background,cover,dynamic));
        draw(prepared,vp.image(0,0,WaveMesh.WIDTH,WaveMesh.HEIGHT,background,cover,dynamic,scene.width,scene.height),alpha,false,canvasSource(),0xffffffff,0,blend,null,.5f,.5f,layer.optInt("blendFlag",0));
    }
    private static JSONArray canvasSource(){JSONArray source=new JSONArray();source.put(0);source.put(WaveMesh.HEIGHT);source.put(WaveMesh.WIDTH);source.put(-WaveMesh.HEIGHT);return source;}
    private static NoiseClock.Spec noiseSpec(JSONObject layer){JSONObject value=layer.optJSONObject("noise");if(value==null)throw new IllegalArgumentException("Missing native noise state");Object raw=value.opt("profile");if(raw!=null&&(!(raw instanceof Number)||(((Number)raw).doubleValue()!=0&&((Number)raw).doubleValue()!=78&&((Number)raw).doubleValue()!=82&&((Number)raw).doubleValue()!=85)))throw new IllegalArgumentException("Invalid native noise profile");return new NoiseClock.Spec(value.optString("token",""),value.optInt("selector",0),value.optInt("periodMs",0),value.optLong("clockMs",-1),raw==null?0:((Number)raw).intValue());}
    private static JSONObject frozenNoiseLayer(JSONObject layer){try{return new JSONObject(layer.toString());}catch(org.json.JSONException error){throw new IllegalArgumentException("Invalid noise layer",error);}}
    private static void validateNoiseSource(JSONObject layer,List<JSONObject> layers){
        if(noiseSpec(layer).profile==78&&(layer.optString("image","").equals("img0408.mzp")==false||layer.optInt("blend",0)!=6||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=17||layer.optInt("colorValue",0)!=0||layer.optInt("brightness",0)!=0||layer.optInt("rgb",0)!=0))throw new IllegalArgumentException("Unreviewed noise78 Native material context");
        int profile=noiseSpec(layer).profile;
        if(profile==82&&(!layer.optString("image","").equals("img0408.mzp")||layer.optInt("blend",0)!=6||layer.optInt("blendFlag",0)!=0||layer.optInt("colorMode",0)!=0||layer.optInt("colorValue",0)!=0||layer.optInt("brightness",0)!=0||layer.optInt("rgb",0)!=0))throw new IllegalArgumentException("Unreviewed noise82 context");
        if(profile==85&&!layer.optBoolean("nativeFullBitmapGeometryReviewed",false))throw new IllegalArgumentException("Private noise85 requires the reviewed composed Native bitmap geometry");
        if(profile==85&&(!layer.optString("image","").equals("aok_n_06_02_07_40_00.mzp")||layer.optInt("finalCallback28",0)!=85||layer.optInt("blend",0)!=17||layer.optInt("blendFlag",0)!=0||(layer.optInt("colorMode",0)!=0&&layer.optInt("colorMode",0)!=70)||layer.optInt("colorValue",0)!=0||layer.optInt("brightness",0)!=0||layer.optInt("rgb",0)!=0))throw new IllegalArgumentException("Unreviewed noise85 context");
        if(layer.optInt("parent",-1)>=0||layer.optJSONObject("window")!=null||layer.optJSONObject("wave")!=null||layer.optJSONArray("source")!=null||layer.optString("kind","").equals("ui")||layer.optString("image","").isEmpty())throw new IllegalArgumentException("Unreviewed parent/window/wave/cropped/UI noise");
        for(JSONObject child:layers)if(child.optInt("parent",-1)==layer.optInt("id",-2))throw new IllegalArgumentException("Unreviewed native noise group");
    }
    private void syncNoiseTransition(Scene value){
        if(value.transition==null||value.transition.mode!=0){noiseTransitionId="";noiseOldScene=null;noiseOld.clear();return;}
        noiseOldScene=value.transition.old;
        if(noiseTransitionId.equals(value.transition.id))return;
        noiseTransitionId=value.transition.id;noiseOld=new HashMap<>();
        for(JSONObject layer:noiseOldScene.layers)if(layer.optJSONObject("noise")!=null){String token=layer.optJSONObject("noise").optString("token","");NoiseClock.Frame<JSONObject> frame=noisePresented.get(token);if(frame==null)frame=noiseRenderer.current(token);if(frame!=null)noiseOld.put(token,frame);}
    }
    private Map<Integer,Texture> prepareNoiseSources(Scene value){
        Map<Integer,Texture> result=new HashMap<>();Map<String,NoiseClock.Frame<JSONObject>> frames=new HashMap<>();boolean old=value==noiseOldScene;
        for(JSONObject layer:value.layers){if(layer.optJSONObject("noise")==null){if(layer.optInt("decimalMode",0)!=0)noiseError(new IllegalArgumentException("Unsupported decimal noise without validated state"));continue;}
            try{validateNoiseSource(layer,value.layers);NoiseClock.Spec spec=noiseSpec(layer);NoiseClock.Frame<JSONObject> frame=old?noiseOld.get(spec.token):null;
                // Missing decode/restore dependencies must not consume a flip.
                String currentImage=layer.optString("image","");NoiseClock.Frame<JSONObject> existing=frame!=null?frame:noiseRenderer.current(spec.token);String frozenImage=existing==null?currentImage:existing.source.optString("image","");boolean assetsReady=true;
                for(String name:new String[]{currentImage,frozenImage,spec.image()})if(textures.get(imageKey(name))==null){load(imageKey(name));assetsReady=false;}
                if(!assetsReady){noiseFrameFailed=true;continue;}
                if(frame==null)frame=noiseRenderer.sample(spec,()->frozenNoiseLayer(layer));
                if(old)noiseOld.put(spec.token,frame);frames.put(spec.token,frame);
                Texture source=textures.get(imageKey(frame.source.optString("image",""))),noise=textures.get(imageKey(frame.spec.image()));
                if(source==null||noise==null){if(source==null)load(imageKey(frame.source.optString("image","")));if(noise==null)load(imageKey(frame.spec.image()));noiseFrameFailed=true;continue;}
                if(noise.originalWidth!=NoiseRenderer.WIDTH||noise.originalHeight!=NoiseRenderer.HEIGHT)throw new IllegalArgumentException("Wrong native noise texture dimensions");
                result.put(layer.optInt("id",-1),noiseSource(frame,source,noise));
            }catch(RuntimeException error){noiseError(error);}
        }
        if(!old)noiseRendered=frames;return result;
    }
    private Texture noiseSource(NoiseClock.Frame<JSONObject> frame,Texture source,Texture noise){
        try{int id=noiseRenderer.render(frame,frozen->{
                JSONObject layer=frozen.source;resetColor();GLES30.glUseProgram(program);GLES30.glUniform1i(rawOutputUniform,frozen.spec.profile==78?5:frozen.spec.profile==82?6:frozen.spec.profile==85?8:4);GLES30.glUniform1i(blendUniform,0);GLES30.glUniform1i(blendFlagUniform,0);GLES30.glUniform1i(kernel,0);GLES30.glUniform1i(blur,0);GLES30.glUniform1i(premultipliedUniform,source.premultiplied?1:0);GLES30.glUniform1f(opacity,1);GLES30.glUniform4f(tint,1,1,1,1);
                GLES30.glUniform4f(boundsUniform,.5f/source.width,.5f/source.height,1-.5f/source.width,1-.5f/source.height);
                GLES30.glActiveTexture(GLES30.GL_TEXTURE3);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,noise.id);linearTexture();GLES30.glUniform1i(noiseSamplerUniform,3);GLES30.glUniform1i(noiseOrientationUniform,frozen.orientation);
                GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,source.id);GLES30.glUniform1i(sampler,0);GLES30.glUniform2f(size,source.width,source.height);
                float x=(float)(layer.optDouble("x",0)+layer.optDouble("pixelOffsetX",0)),y=(float)(layer.optDouble("y",0)+layer.optDouble("pixelOffsetY",0));
                QuadGeometry.write(vertices,x,y,x+(float)layer.optDouble("w",NoiseRenderer.WIDTH),y+(float)layer.optDouble("h",NoiseRenderer.HEIGHT),(float)layer.optDouble("rotation",0),NoiseRenderer.WIDTH,NoiseRenderer.HEIGHT,0,0,1,1,(float)layer.optDouble("pivotX",.5),(float)layer.optDouble("pivotY",.5),layer.optInt("matrixMode",0),(float)layer.optDouble("intrinsicWidth",0),(float)layer.optDouble("intrinsicHeight",0));
                bindVertices(position,uv);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);
                if(frozen.spec.profile==85)drawNoise85Passes(noise);
            });return new Texture(id,NoiseRenderer.WIDTH,NoiseRenderer.HEIGHT);
        }finally{GLES30.glUseProgram(program);GLES30.glUniform1i(rawOutputUniform,0);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);}
    }
    private void noiseQuad(Texture source,int raw,float x0,float y0,float x1,float y1,float grey){
        GLES30.glUniform1i(rawOutputUniform,raw);GLES30.glUniform1i(kernel,0);GLES30.glUniform1i(blur,0);GLES30.glUniform4f(tint,grey,grey,grey,grey);
        GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,source.id);GLES30.glUniform2f(size,source.width,source.height);
        QuadGeometry.write(vertices,x0,y0,x1,y1,0,1920,1080,0,0,1,1,.5f,.5f,0,1,1);bindVertices(position,uv);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);
    }
    private void drawNoise85Passes(Texture noise){
        GLES30.glEnable(GLES30.GL_BLEND);GLES30.glBlendEquationSeparate(GLES30.GL_FUNC_ADD,GLES30.GL_FUNC_ADD);
        try{
            GLES30.glBlendFuncSeparate(GLES30.GL_DST_COLOR,GLES30.GL_ZERO,GLES30.GL_DST_ALPHA,GLES30.GL_ZERO);noiseQuad(noise,9,0,0,1920,1080,1);
            GLES30.glBlendFuncSeparate(GLES30.GL_ONE,GLES30.GL_SRC_ALPHA,GLES30.GL_ZERO,GLES30.GL_ONE);noiseQuad(noise,10,0,0,1920,1080,1);
            GLES30.glBlendFuncSeparate(GLES30.GL_ONE,GLES30.GL_ONE,GLES30.GL_ONE,GLES30.GL_ONE);GLES30.glBlendEquationSeparate(GLES30.GL_MIN,GLES30.GL_MIN);
            for(int i=1;i<=11;i++){int[] r=NoiseSpecial.rectangle85(i);noiseQuad(solid,11,r[0],r[1],r[2],r[3],r[4]/255f);}
        }finally{GLES30.glBlendEquationSeparate(GLES30.GL_FUNC_ADD,GLES30.GL_FUNC_ADD);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);GLES30.glDisable(GLES30.GL_BLEND);}
    }
    private void drawNoise(Texture prepared,JSONObject layer,Scene value,Viewport vp,boolean background,boolean cover,float alpha,int blend){
        if(prepared==null)throw new IllegalStateException("Noise source not prepared");resetColor();scissor(vp.clip(background,cover,dynamic));noiseFinalStage=true;
        try{int family=layer.optInt("finalCallback28",0);GLES30.glUniform1i(noiseFinal28Uniform,family);if(family!=0&&family!=85)throw new IllegalArgumentException("Unreviewed noise final28 family");draw(prepared,vp.image(0,0,NoiseRenderer.WIDTH,NoiseRenderer.HEIGHT,background,cover,dynamic,value.width,value.height),family==85?WindowMask.childOpacity(layer.optDouble("alpha",1)):alpha,false,canvasSource(),0xffffffff,0,family==85?28:blend,null,.5f,.5f,layer.optInt("blendFlag",0));int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException("noise final draw GL0x"+Integer.toHexString(error));}finally{GLES30.glUniform1i(noiseFinal28Uniform,0);noiseFinalStage=false;}
    }
    private void noiseError(RuntimeException error){noiseFrameFailed=true;String message="원본 noise: "+error.getMessage();if(noiseErrors.add(message))errors.accept(message);}
    private static void noiseTokens(Scene value,Set<String> tokens){if(value==null)return;for(JSONObject layer:value.layers){JSONObject noise=layer.optJSONObject("noise");if(noise!=null)tokens.add(noise.optString("token",""));}if(value.transition!=null&&value.transition.mode==0)noiseTokens(value.transition.old,tokens);}
    private void trimNoise(Scene current,Scene next){Set<String> tokens=new HashSet<>();noiseTokens(current,tokens);noiseTokens(next,tokens);noiseRenderer.retain(tokens);noisePresented.keySet().retainAll(tokens);noiseOld.keySet().retainAll(tokens);noiseRendered.keySet().retainAll(tokens);}
    private void pinNoiseFrames(Set<String> keys,Scene value){if(value==null)return;for(JSONObject layer:value.layers){JSONObject noise=layer.optJSONObject("noise");if(noise==null)continue;String token=noise.optString("token","");int selector=noise.optInt("selector",0);if(selector==1||selector==2)keys.add(imageKey(selector==1?"noise0.mzp":"noise1.mzp"));String image=layer.optString("image","");if(!image.isEmpty())keys.add(imageKey(image));for(NoiseClock.Frame<JSONObject> frame:Arrays.asList(noiseOld.get(token),noisePresented.get(token),noiseRenderer.current(token)))if(frame!=null)keys.add(imageKey(frame.source.optString("image","")));}}
    private String frameKey(Viewport vp){return vp==null?"":width+"x"+height+":"+vp.area.left+","+vp.area.top+","+vp.area.right+","+vp.area.bottom+":"+vp.content.left+","+vp.content.top+","+vp.content.right+","+vp.content.bottom+":"+vp.x+","+vp.y+","+vp.scale+":"+dynamic+":"+ambient+":"+bgCover+":"+eventCover+":"+filter+":"+hiddenText;}
    private String transitionFrameKey(Scene value,Viewport vp){String key=frameKey(vp);return value.transition==null||textRenderState==null?key:key+":"+textRenderState.captureKey(value.transition.old);}
    private static String movieId(Scene value){return value==null||value.movie==null?"":value.movie.optString("id","");}
    private void updateMovieFrame(MoviePlayer.Frame frame){
        if(!frame.hasFrame||!frame.newFrame&&frame.id.equals(movieFrameId))return;
        if(!movieGpuFailure.isEmpty()){movies.frameFailed(frame.id,movieGpuFailure);return;}
        drainDestinationErrors("before movie copy",false);int read=glInteger(GLES30.GL_READ_FRAMEBUFFER_BINDING),draw=glInteger(GLES30.GL_DRAW_FRAMEBUFFER_BINDING);
        try{
            if(movieFrame==null){int[] ids=new int[1];GLES30.glGenTextures(1,ids,0);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,ids[0]);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGBA8,1920,1080,0,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,null);linearTexture();movieFrame=new Texture(ids[0],1920,1080);GLES30.glGenFramebuffers(1,ids,0);movieFramebuffer=ids[0];}
            GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER,movieFramebuffer);GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER,GLES30.GL_COLOR_ATTACHMENT0,GLES30.GL_TEXTURE_2D,movieFrame.id,0);
            if(GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)!=GLES30.GL_FRAMEBUFFER_COMPLETE)throw new IllegalStateException("incomplete movie framebuffer");
            GLES30.glViewport(0,0,1920,1080);GLES30.glDisable(GLES30.GL_SCISSOR_TEST);GLES30.glDisable(GLES30.GL_BLEND);GLES30.glUseProgram(movieProgram);
            movieCopyQuad(vertices);bindVertices(moviePosition,movieUv);
            GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,frame.textureId);GLES30.glUniform1i(movieSampler,0);GLES30.glUniformMatrix4fv(movieTransform,1,false,frame.transform,0);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);
            int error=GLES30.glGetError();if(error!=GLES30.GL_NO_ERROR)throw new IllegalStateException("OES copy GL error 0x"+Integer.toHexString(error));
            movieFrameId=frame.id;
        }catch(RuntimeException error){movieFrameId="";movieGpuFailure=error.getMessage();movies.frameFailed(frame.id,movieGpuFailure);}
        finally{GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER,read);GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER,draw);GLES30.glViewport(0,0,width,height);GLES30.glEnable(GLES30.GL_BLEND);GLES30.glUseProgram(program);}
    }
    private void drawMaskTransition(Texture old,Scene.Transition transition,Viewport vp){
        Texture rule=textures.get(imageKey(transition.mask));if(rule==null)return;
        GLES30.glUseProgram(maskProgram);scissor(vp.content);GLES30.glEnable(GLES30.GL_BLEND);GLES30.glBlendFunc(GLES30.GL_ONE,GLES30.GL_ONE_MINUS_SRC_ALPHA);
        GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,old.id);linearTexture();GLES30.glUniform1i(maskOld,0);
        GLES30.glActiveTexture(GLES30.GL_TEXTURE1);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,rule.id);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_NEAREST);GLES30.glUniform1i(maskRule,1);
        GLES30.glUniform4f(maskArea,vp.content.left,vp.content.top,vp.content.width(),vp.content.height());GLES30.glUniform1f(maskHeight,height);GLES30.glUniform1i(maskProgress,transition.progress256);GLES30.glUniform1i(maskStrength,transition.strength);
        QuadGeometry.write(vertices,0,0,width,height,0,width,height,0,1,1,0);bindVertices(maskPosition,maskUv);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);GLES30.glUseProgram(program);
    }
    private static void linearTexture(){GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);}
    static void movieCopyQuad(FloatBuffer out){QuadGeometry.write(out,0,0,1920,1080,0,1920,1080,0,1,1,0);}
    private void bindVertices(int position,int uv){vertices.position(0);GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,16,vertices);GLES30.glEnableVertexAttribArray(position);vertices.position(2);GLES30.glVertexAttribPointer(uv,2,GLES30.GL_FLOAT,false,16,vertices);GLES30.glEnableVertexAttribArray(uv);}
    private void resetColor(){GLES30.glUniform1i(colorModeUniform,0);GLES30.glUniform1i(contrastUniform,0);GLES30.glUniform1i(brightnessUniform,0);}
    private void drawFrameTexture(Texture texture,float alpha){JSONArray source=new JSONArray();source.put(0);source.put(height);source.put(width);source.put(-height);draw(texture,new RectF(0,0,width,height),alpha,false,source,0xffffffff,0,0,null,.5f,.5f);}
    private void drawNativeText(Scene s,Viewport vp){
        if(s.texts.isEmpty()&&vp.panel.height()<=0&&TextOverlay.fullscreenBackdropAlpha(s)<=0){releaseNativeText();return;}String key=frameKey(vp)+":"+s.texts.toString()+":"+(textRenderState==null?"":textRenderState.captureKey(s));
        if(!key.equals(nativeTextKey)){
            Bitmap bitmap=TextOverlay.capture(s,vp,width,height,textRenderState);
            try{releaseNativeText();int[] ids=new int[1];GLES30.glGenTextures(1,ids,0);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,ids[0]);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLUtils.texImage2D(GLES30.GL_TEXTURE_2D,0,bitmap,0);nativeTextTexture=new Texture(ids[0],width,height);nativeTextTexture.premultiplied=true;nativeTextKey=key;}finally{bitmap.recycle();}
        }
        resetColor();scissor(vp.content);draw(nativeTextTexture,new RectF(0,0,width,height),1,false,null,0xffffffff,0,0,null,.5f,.5f);
    }
    private void release(Texture texture){if(texture!=null)GLES30.glDeleteTextures(1,new int[]{texture.id},0);}
    private void releaseNativeText(){release(nativeTextTexture);nativeTextTexture=null;nativeTextKey="";}
    private void scissor(RectF r){int left=Math.round(r.left),right=Math.round(r.right),top=Math.round(r.top),bottom=Math.round(r.bottom);GLES30.glScissor(left,height-bottom,Math.max(0,right-left),Math.max(0,bottom-top));}
    private void drawAmbient(Viewport vp){
        // Blur the completed scene, including its transforms and crossfading layers.
        if(!captureDestination())return;
        GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,destinationTexture);
        GLES30.glGenerateMipmap(GLES30.GL_TEXTURE_2D);if(drainDestinationErrors("ambient mipmaps",true))return;
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR_MIPMAP_LINEAR);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);
        GLES30.glUniform1i(colorModeUniform,0);GLES30.glUniform1i(contrastUniform,0);GLES30.glUniform1i(brightnessUniform,0);
        Texture composed=new Texture(destinationTexture,width,height);
        JSONArray source=new JSONArray();source.put(Math.round(vp.area.left));source.put(Math.round(height-vp.area.top));source.put(Math.round(vp.area.width()));source.put(-Math.round(vp.area.height()));
        RectF sampleBounds=new RectF(vp.frame.left,height-vp.frame.bottom,vp.frame.right,height-vp.frame.top);
        for(RectF band:vp.letterboxes)if(band.width()>.5f&&band.height()>.5f){scissor(band);draw(composed,vp.area,1,true,source,0xffffffff,0,0,sampleBounds,.5f,.5f);}
    }
    private void draw(Texture t,RectF dst,float alpha,boolean ambientPass,JSONArray source,int color,float rotation,int blend,RectF sampleBounds,float pivotX,float pivotY){
        draw(t,dst,alpha,ambientPass,source,color,rotation,blend,sampleBounds,pivotX,pivotY,0);
    }
    // Every child fragment writes even transparent pixels; only a full, untransformed quad can omit the preservation blit.
    static boolean coversWindow(JSONObject layer){
        if(layer.optInt("matrixMode",0)!=0||(float)layer.optDouble("rotation",0)%360f!=0||layer.optJSONObject("wave")!=null)return false;
        float x=(float)(layer.optDouble("x",0)+layer.optDouble("pixelOffsetX",0)),y=(float)(layer.optDouble("y",0)+layer.optDouble("pixelOffsetY",0));
        float w=(float)layer.optDouble("w",WindowMask.WIDTH),h=(float)layer.optDouble("h",WindowMask.HEIGHT);
        return Float.isFinite(x)&&Float.isFinite(y)&&Float.isFinite(w)&&Float.isFinite(h)&&x<=0&&y<=0&&w>0&&h>0&&x+w>=WindowMask.WIDTH&&y+h>=WindowMask.HEIGHT;
    }
    private boolean tryPlain(Texture t,RectF dst,float alpha,JSONArray source,int color,float rotation,int blend,float pivotX,float pivotY,int matrixMode,float intrinsicWidth,float intrinsicHeight){
        if(!plainEnabled||(windowChildStage?blend!=0||!identityChildStage:blend!=0&&blend!=3))return false;
        try{plainImage.ensure(filter,windowChildStage?2:windowImageStage?1:0);}catch(RuntimeException error){plainEnabled=false;android.util.Log.w("WoH.GL","Plain image shader unavailable; retaining general material",error);return false;}
        float u0=0,v0=0,u1=1,v1=1;if(source!=null&&source.length()>=4){u0=(float)source.optDouble(0)/t.originalWidth;v0=(float)source.optDouble(1)/t.originalHeight;u1=u0+(float)source.optDouble(2)/t.originalWidth;v1=v0+(float)source.optDouble(3)/t.originalHeight;}
        QuadGeometry.write(vertices,dst.left,dst.top,dst.right,dst.bottom,rotation,windowImageStage?WindowMask.WIDTH:width,windowImageStage?WindowMask.HEIGHT:height,u0,v0,u1,v1,pivotX,pivotY,matrixMode,intrinsicWidth,intrinsicHeight);
        if(!windowImageStage&&!windowChildStage)QuadGeometry.applyCapture(vertices,dst.left,dst.top,dst.right,dst.bottom,rotation,u0,v0,u1,v1,pivotX,pivotY,matrixMode,intrinsicWidth,intrinsicHeight);
        try{plainImage.draw(vertices,t.id,t.width,t.height,t.premultiplied,u0,v0,u1,v1,color,alpha,windowChildStage?2:windowImageStage?1:0,filter);}finally{GLES30.glUseProgram(program);}return true;
    }
    private void drawLayer(Texture t,RectF dst,float alpha,JSONArray source,int color,JSONObject layer,int blend){
        int matrixMode=layer.optInt("matrixMode",0);
        if(layer.optInt("colorMode",0)==0&&layer.optInt("brightness",0)==0&&layer.optInt("colorValue",0)==0&&tryPlain(t,dst,alpha,source,color,(float)layer.optDouble("rotation",0),blend,(float)layer.optDouble("pivotX",.5),(float)layer.optDouble("pivotY",.5),matrixMode,(float)layer.optDouble("intrinsicWidth",0),(float)layer.optDouble("intrinsicHeight",0)))return;
        draw(t,dst,alpha,false,source,color,(float)layer.optDouble("rotation",0),blend,null,(float)layer.optDouble("pivotX",.5),(float)layer.optDouble("pivotY",.5),layer.optInt("blendFlag",0),matrixMode,(float)layer.optDouble("intrinsicWidth",0),(float)layer.optDouble("intrinsicHeight",0));
    }
    private void draw(Texture t,RectF dst,float alpha,boolean ambientPass,JSONArray source,int color,float rotation,int blend,RectF sampleBounds,float pivotX,float pivotY,int blendFlag){
        draw(t,dst,alpha,ambientPass,source,color,rotation,blend,sampleBounds,pivotX,pivotY,blendFlag,0,1,1);
    }
    private void draw(Texture t,RectF dst,float alpha,boolean ambientPass,JSONArray source,int color,float rotation,int blend,RectF sampleBounds,float pivotX,float pivotY,int blendFlag,int matrixMode,float intrinsicWidth,float intrinsicHeight){
        float u0=0,v0=0,u1=1,v1=1;if(source!=null&&source.length()>=4){u0=(float)source.optDouble(0)/t.originalWidth;v0=(float)source.optDouble(1)/t.originalHeight;u1=u0+(float)source.optDouble(2)/t.originalWidth;v1=v0+(float)source.optDouble(3)/t.originalHeight;}
        if(sampleBounds==null)GLES30.glUniform4f(boundsUniform,Math.min(u0,u1)+.5f/t.width,Math.min(v0,v1)+.5f/t.height,Math.max(u0,u1)-.5f/t.width,Math.max(v0,v1)-.5f/t.height);
        else GLES30.glUniform4f(boundsUniform,(sampleBounds.left+.5f)/t.width,(sampleBounds.top+.5f)/t.height,(sampleBounds.right-.5f)/t.width,(sampleBounds.bottom-.5f)/t.height);
        if(blend!=0&&blend!=3&&!windowChildStage&&!captureDestination()){if(noiseFinalStage)noiseFrameFailed=true;if(alphaRuleFinalStage)alphaRuleFrameFailed=true;return;}GLES30.glUniform1i(blendUniform,blend);GLES30.glUniform1i(blendFlagUniform,blendFlag);GLES30.glUniform1i(rawOutputUniform,windowChildStage?2:windowImageStage?1:0);GLES30.glUniform1i(destinationUniform,1);GLES30.glUniform2f(viewportUniform,width,height);
        QuadGeometry.write(vertices,dst.left,dst.top,dst.right,dst.bottom,rotation,windowImageStage?WindowMask.WIDTH:width,windowImageStage?WindowMask.HEIGHT:height,u0,v0,u1,v1,pivotX,pivotY,matrixMode,intrinsicWidth,intrinsicHeight);if(!windowImageStage&&!windowChildStage)QuadGeometry.applyCapture(vertices,dst.left,dst.top,dst.right,dst.bottom,rotation,u0,v0,u1,v1,pivotX,pivotY,matrixMode,intrinsicWidth,intrinsicHeight);GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,16,vertices);GLES30.glEnableVertexAttribArray(position);vertices.position(2);GLES30.glVertexAttribPointer(uv,2,GLES30.GL_FLOAT,false,16,vertices);GLES30.glEnableVertexAttribArray(uv);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,t.id);GLES30.glUniform1i(sampler,0);GLES30.glUniform2f(size,t.width,t.height);GLES30.glUniform1i(kernel,filter);GLES30.glUniform1i(premultipliedUniform,t.premultiplied?1:0);GLES30.glUniform1i(blur,ambientPass?1:0);GLES30.glUniform1f(opacity,Math.max(0,Math.min(1,alpha)));GLES30.glUniform4f(tint,android.graphics.Color.red(color)/255f,android.graphics.Color.green(color)/255f,android.graphics.Color.blue(color)/255f,android.graphics.Color.alpha(color)/255f);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);
    }
    private boolean captureDestination(){
        // Attribute older errors to their own stage, rather than to the following copy.
        drainDestinationErrors("before destination capture",false);
        if(!framebufferLogged){
            int readFbo=glInteger(GLES30.GL_READ_FRAMEBUFFER_BINDING),drawFbo=glInteger(GLES30.GL_DRAW_FRAMEBUFFER_BINDING),readBuffer=glInteger(GLES30.GL_READ_BUFFER);
            int samples=glInteger(GLES30.GL_SAMPLES),sampleBuffers=glInteger(GLES30.GL_SAMPLE_BUFFERS);
            if(readBuffer==GLES30.GL_NONE){errors.accept("GL read buffer is NONE");return false;}
            int red=framebufferParameter(readBuffer,GLES30.GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE),green=framebufferParameter(readBuffer,GLES30.GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE),blue=framebufferParameter(readBuffer,GLES30.GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE),alpha=framebufferParameter(readBuffer,GLES30.GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE);
            int encoding=framebufferParameter(readBuffer,GLES30.GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING),componentType=framebufferParameter(readBuffer,GLES30.GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE);
            if(drainDestinationErrors("read framebuffer query",true))return false;
            destinationCopyFormat=destinationInternalFormat(alpha,encoding==GLES30.GL_SRGB);
            android.util.Log.i("WoH.GL","Read framebuffer: readFbo="+readFbo+" drawFbo="+drawFbo+" readBuffer=0x"+Integer.toHexString(readBuffer)+" drawSamples="+samples+" drawSampleBuffers="+sampleBuffers+" rgbaBits="+red+","+green+","+blue+","+alpha+" encoding=0x"+Integer.toHexString(encoding)+" componentType=0x"+Integer.toHexString(componentType)+" destinationFormat=0x"+Integer.toHexString(destinationCopyFormat));
            framebufferLogged=true;
        }
        GLES30.glActiveTexture(GLES30.GL_TEXTURE1);
        if(destinationTexture==0){int[] ids=new int[1];GLES30.glGenTextures(1,ids,0);destinationTexture=ids[0];}
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,destinationTexture);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_NEAREST);
        if(destinationWidth!=width||destinationHeight!=height){
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);
            if(drainDestinationErrors("destination texture setup",true))return false;
            // ES3 table 3.16 forbids adding an alpha channel when copying an RGB buffer.
            // Unsized RGB/RGBA also adopts the read buffer's effective component sizes.
            GLES30.glCopyTexImage2D(GLES30.GL_TEXTURE_2D,0,destinationCopyFormat,0,0,width,height,0);
            if(drainDestinationErrors("CopyTexImage2D",true))return false;
            destinationWidth=width;destinationHeight=height;destinationCopyVerified=false;
        }else{
            if(drainDestinationErrors("destination texture bind",true))return false;
            GLES30.glCopyTexSubImage2D(GLES30.GL_TEXTURE_2D,0,0,0,0,0,width,height);
            if(drainDestinationErrors("CopyTexSubImage2D",true))return false;
        }
        if(!destinationCopyVerified){destinationCopyVerified=true;android.util.Log.i("WoH.GL","Destination copy OK "+width+"x"+height+" format=0x"+Integer.toHexString(destinationCopyFormat));}
        return true;
    }
    static int destinationInternalFormat(int alphaBits,boolean srgb){return srgb?(alphaBits>0?GLES30.GL_SRGB8_ALPHA8:GLES30.GL_SRGB8):(alphaBits>0?GLES30.GL_RGBA:GLES30.GL_RGB);}
    private static int glInteger(int name){int[] value=new int[1];GLES30.glGetIntegerv(name,value,0);return value[0];}
    private static int framebufferParameter(int attachment,int name){int[] value=new int[1];GLES30.glGetFramebufferAttachmentParameteriv(GLES30.GL_READ_FRAMEBUFFER,attachment,name,value,0);return value[0];}
    private boolean drainDestinationErrors(String stage,boolean fatal){
        boolean any=false;int error;
        while((error=GLES30.glGetError())!=GLES30.GL_NO_ERROR){any=true;String key=stage+":"+error;if(destinationErrors.add(key)){String message="GL "+stage+" error: 0x"+Integer.toHexString(error);android.util.Log.e("WoH.GL",message);if(fatal)errors.accept(message);}}
        return any;
    }
    private void loadColorPalette(int mode){
        if(failedColorPalettes.contains(mode))return;
        try(InputStream input=assets.file("meta/color"+mode+"-rgb.bin").open()){
            byte[] data=new byte[768];int offset=0;
            while(offset<data.length){int count=input.read(data,offset,data.length-offset);if(count<=0)throw new java.io.IOException("Truncated sepia LUT");offset+=count;}
            if(input.read()!=-1)throw new java.io.IOException("Unexpected sepia LUT size");
            ByteBuffer pixels=ByteBuffer.allocateDirect(data.length);pixels.put(data).flip();int[] ids=new int[1];GLES30.glGenTextures(1,ids,0);GLES30.glActiveTexture(GLES30.GL_TEXTURE2);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,ids[0]);GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D,0,GLES30.GL_RGB8,256,1,0,GLES30.GL_RGB,GLES30.GL_UNSIGNED_BYTE,pixels);
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_NEAREST);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
            colorPaletteTextures.put(mode,ids[0]);
        }catch(Exception e){failedColorPalettes.add(mode);errors.accept("원본 색상표 "+mode+"를 읽을 수 없습니다: "+e.getMessage());}
    }
    private void load(String name){load(name,false,Long.MAX_VALUE);}
    private void load(String name,boolean optional,long maximumBytes){
        if(stopped||failed.contains(name)||!pending.add(name))return;final int epoch=generation;
        try{loader.execute(()->{
            Bitmap bitmap=null;boolean limited=false;BitmapFactory.Options bounds=new BitmapFactory.Options();
            try{GameAsset file=imageSources.get(name);if(file==null)throw new IllegalStateException("Unresolved image resource");bounds.inJustDecodeBounds=true;file.bitmap(bounds);BitmapFactory.Options options=new BitmapFactory.Options();options.inPreferredConfig=Bitmap.Config.ARGB_8888;options.inPremultiplied=false;options.inScaled=false;options.inSampleSize=ImageSampling.sampleSize(bounds.outWidth,bounds.outHeight,textureLimit,IMAGE_BUDGET);limited=optional&&(long)Math.max(0,bounds.outWidth/options.inSampleSize)*Math.max(0,bounds.outHeight/options.inSampleSize)*4>maximumBytes;if(!limited)bitmap=file.bitmap(options);}catch(Exception e){android.util.Log.d(optional?"WoH.Warm":"WoH.Image",name,e);}
            final Bitmap ready=bitmap;final int originalWidth=bounds.outWidth,originalHeight=bounds.outHeight;
            if(stopped){if(ready!=null)ready.recycle();return;}final boolean skipped=limited;view.queueEvent(()->adoptImage(name,epoch,ready,originalWidth,originalHeight,skipped));
        });}catch(RejectedExecutionException ignored){pending.remove(name);}
    }
    private void adoptImage(String name,int epoch,Bitmap ready,int originalWidth,int originalHeight,boolean limited){
        if(stopped||epoch!=generation){if(ready!=null)ready.recycle();return;}pending.remove(name);boolean required=pinnedImages().contains(name);
        if(!required&&!currentWarmHint(name)){if(ready!=null)ready.recycle();if(warm.contains(name))warm.defer(name);pumpWarm();return;}
        if(!hasGlContext()){if(ready!=null)ready.recycle();warm.defer(name);if(required)view.requestRender();return;}
        if(limited){if(required)view.requestRender();pumpWarm();return;}
        if(ready==null){if(required){failed.add(name);errors.accept("이미지를 읽을 수 없습니다: "+name);}else android.util.Log.d("WoH.Warm","Prepared hint could not be decoded: "+name);pumpWarm();return;}
        int[] ids=new int[1];GLES30.glGenTextures(1,ids,0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,ids[0]);linearTexture();GLUtils.texImage2D(GLES30.GL_TEXTURE_2D,0,ready,0);
        Texture texture=new Texture(ids[0],ready.getWidth(),ready.getHeight());texture.originalWidth=originalWidth;texture.originalHeight=originalHeight;Texture previous=textures.put(name,texture);if(previous!=null)GLES30.glDeleteTextures(1,new int[]{previous.id},0);ready.recycle();trimTextures();if(required)view.requestRender();pumpWarm();
    }
    private static boolean hasGlContext(){return !EGL14.EGL_NO_CONTEXT.equals(EGL14.eglGetCurrentContext());}
    private boolean currentWarmHint(String name){Scene value=desired;return value!=null&&warmResolvedKey.equals(value.preloadKey)&&warm.contains(name);}
    private void updateWarmHints(Scene value){
        if(stopped)return;warmDesired=value;if(value.preloads.isEmpty()){warm.replace(Collections.emptySet());preparedImages=null;warmResolvedKey=value.preloadKey;return;}
        if(warmResolvedKey.equals(value.preloadKey)||warmResolving)return;
        warmResolving=true;final int epoch=generation;final PreparedImages known=preparedImages;
        try{hintResolver.execute(()->{
            PreparedImages index=known;Set<String> files=new LinkedHashSet<>();try{if(index==null)index=PreparedImages.paths(assets.imageFiles());for(int i=value.preloads.size()-1;i>=0;i--){JSONObject hint=value.preloads.get(i);String base=hint.optString("base",hint.optString("name",""));if(hint.optString("kind","").equals("range"))files.addAll(index.range(base,hint.optInt("xMin",0),hint.optInt("yMin",0),hint.optInt("xMax",0),hint.optInt("yMax",0)));else if(hint.optString("kind","").equals("image"))files.addAll(index.image(base,hint.optInt("variantX",0),hint.optInt("variantY",0)));}}catch(Exception error){android.util.Log.d("WoH.Warm","Prepared-only hint scan: "+error.getMessage());}
            if(stopped)return;final PreparedImages resolved=index;view.queueEvent(()->{if(stopped||epoch!=generation)return;warmResolving=false;preparedImages=resolved;if(warmDesired!=null&&warmDesired.preloadKey.equals(value.preloadKey)){Set<String> keys=new LinkedHashSet<>();for(String file:files)keys.add(imageKey(file));warm.replace(keys);warmResolvedKey=value.preloadKey;pumpWarm();}else if(warmDesired!=null)updateWarmHints(warmDesired);});
        });}catch(RejectedExecutionException ignored){warmResolving=false;}
    }
    private void pumpWarm(){
        if(stopped||!pending.isEmpty()||!hasGlContext()||desired==null||!warmResolvedKey.equals(desired.preloadKey))return;long pinned=0;for(String name:pinnedImages()){Texture texture=textures.get(name);if(texture==null)return;pinned+=(long)texture.width*texture.height*4;}long room=IMAGE_BUDGET-pinned;if(room<=0)return;
        String next=warm.next(name->textures.contains(name)||failed.contains(name));if(next!=null)load(next,true,room);
    }
    private String imageKey(String name){
        String key=imageKeys.get(name);if(key!=null)return key;
        try{GameAsset resource=assets.file(name);key=resource.key();imageSources.putIfAbsent(key,resource);}catch(Exception e){key="!invalid:"+name;failed.add(key);errors.accept("잘못된 이미지 경로: "+name);}
        imageKeys.put(name,key);return key;
    }
    private Set<String> requiredImages(Scene value){
        Set<String> keys=new HashSet<>();if(value==null)return keys;
        pinNoiseFrames(keys,value);
        for(String resource:value.particles.resources)keys.add(imageKey(resource));
        for(JSONObject layer:value.layers)if(layer.optJSONObject("alphaRule")!=null){keys.add(imageKey(layer.optString("image","")));keys.add(imageKey(alphaRuleImage(layer.optJSONObject("alphaRule"))));}
        if(!MovieRenderGate.active(movieId(value),movieFrameId,movieFrame!=null))for(JSONObject layer:value.layers){String name=layer.optString("image","");if(Residency.required(layer.optDouble("alpha",1),layer.optBoolean("preload",false))||layer.optJSONObject("wave")!=null){if(!name.isEmpty())keys.add(imageKey(name));try{WindowMask.Spec window=WindowMask.Spec.from(layer.optJSONObject("window"));if(window!=null)for(String asset:window.assets())keys.add(imageKey(asset));}catch(RuntimeException error){windowError(error);}}}
        for(JSONObject child:value.layers)if(child.optJSONObject("wave")!=null&&child.optInt("parent",-1)>=0)for(JSONObject parent:value.layers)if(parent.optInt("id",-2)==child.optInt("parent",-1)){
            String name=parent.optString("image","");if(!name.isEmpty())keys.add(imageKey(name));try{WindowMask.Spec window=WindowMask.Spec.from(parent.optJSONObject("window"));if(window!=null)for(String asset:window.assets())keys.add(imageKey(asset));}catch(RuntimeException error){waveError(error);}
        }
        if(value.transition!=null&&value.transition.mode==0&&!value.transition.mask.isEmpty())keys.add(imageKey(value.transition.mask));
        if(value.transition!=null&&TransitionFrameState.captureOld(value.transition.mode))keys.addAll(requiredImages(value.transition.old));
        return keys;
    }
    private Set<String> pinnedImages(){Set<String> keys=requiredImages(scene);keys.addAll(requiredImages(desired));return keys;}
    private void trimTextures(){for(Texture old:textures.trim(pinnedImages()))GLES30.glDeleteTextures(1,new int[]{old.id},0);}
    static final class WarmPolicy {
        // Native ranges can be huge; this advisory queue never owns visible-frame pins.
        private final int limit;
        private Set<String> current=new LinkedHashSet<>();
        private final Set<String> attempted=new HashSet<>();
        private final ArrayDeque<String> queue=new ArrayDeque<>();
        WarmPolicy(int limit){this.limit=limit;}
        boolean contains(String name){return current.contains(name);}
        void replace(Set<String> names){
            if(current.equals(names))return;Set<String> added=new LinkedHashSet<>(names);added.removeAll(current);ArrayDeque<String> old=new ArrayDeque<>(queue);current=new LinkedHashSet<>(names);attempted.retainAll(current);queue.clear();
            for(String name:added)offer(name);for(String name:old)if(current.contains(name))offer(name);
        }
        private void offer(String name){if(queue.size()<limit&&!attempted.contains(name)&&!queue.contains(name))queue.add(name);}
        String next(Predicate<String> resident){while(!queue.isEmpty()){String name=queue.remove();if(current.contains(name)&&attempted.add(name)&&!resident.test(name))return name;}return null;}
        void defer(String name){if(current.contains(name)){attempted.remove(name);offer(name);}}
        void clear(){current.clear();attempted.clear();queue.clear();}
    }
    static final class PreparedImages {
        private final SortedMap<String,String> files;
        PreparedImages(SortedMap<String,String> files){this.files=Collections.unmodifiableSortedMap(new TreeMap<>(files));}
        static PreparedImages paths(Collection<String> paths){TreeMap<String,String> files=new TreeMap<>();for(String path:paths){String name=new File(path).getName().toLowerCase(Locale.ROOT);if(name.endsWith(".png"))files.put(name.substring(0,name.length()-4),path);}return new PreparedImages(files);}
        static PreparedImages scan(File directory)throws java.io.IOException{
            TreeMap<String,String> files=new TreeMap<>();String root=directory.getCanonicalPath()+File.separator;File[] prepared=directory.listFiles();if(prepared!=null)for(File file:prepared){String name=file.getName().toLowerCase(Locale.ROOT);if(file.isFile()&&name.endsWith(".png")){String path=file.getCanonicalPath();if(path.startsWith(root))files.put(name.substring(0,name.length()-4),path);}}return new PreparedImages(files);
        }
        static boolean valid(String base){return !base.isEmpty()&&!base.contains("/")&&!base.contains("\\")&&!base.contains(":")&&!base.equals(".")&&!base.equals("..");}
        static String variant(String base,int x,int y){if(x==0&&y==0)return base;int dot=base.lastIndexOf('.');if(dot<0)dot=base.length();return base.substring(0,dot)+String.format(Locale.ROOT,"_%02d_%02d",x,y)+base.substring(dot);}
        Set<String> image(String base,int x,int y){if(!valid(base))return Collections.emptySet();String file=files.get(variant(base.toLowerCase(Locale.ROOT),x,y));return file==null?Collections.emptySet():Collections.singleton(file);}
        Set<String> range(String base,int xMin,int yMin,int xMax,int yMax){
            Set<String> found=new LinkedHashSet<>();if(!valid(base)||xMin>xMax||yMin>yMax)return found;base=base.toLowerCase(Locale.ROOT);int dot=base.lastIndexOf('.');if(dot<0)dot=base.length();String prefix=base.substring(0,dot)+"_",extension=base.substring(dot);
            for(Map.Entry<String,String> file:files.entrySet()){String name=file.getKey();int x,y;if(name.equals(base)){x=y=0;}else{if(!name.startsWith(prefix)||!name.endsWith(extension)||name.length()<prefix.length()+extension.length())continue;String suffix=name.substring(prefix.length(),name.length()-extension.length());int separator=suffix.indexOf('_');if(separator<0)continue;try{x=Integer.parseInt(suffix.substring(0,separator));y=Integer.parseInt(suffix.substring(separator+1));}catch(NumberFormatException invalid){continue;}if(!variant(base,x,y).equals(name))continue;}if(x>=xMin&&x<=xMax&&y>=yMin&&y<=yMax)found.add(file.getValue());}return found;
        }
    }
    static final class TransitionFrameState {
        private volatile String id="",geometry="";
        static boolean supported(int mode){return mode==0||mode==1;}
        static boolean captureOld(int mode){return mode==0;}
        boolean needsCapture(String next,String frame){return !ready(next,frame);}
        boolean ready(String next,String frame){return !next.isEmpty()&&next.equals(id)&&frame.equals(geometry);}
        void captured(String next,String frame){geometry=frame;id=next;}
        void clear(){id="";geometry="";}
    }
    static final class MovieRenderGate {
        private String id="",geometry="";
        static boolean active(String requested,String cached,boolean texture){return texture&&!requested.isEmpty()&&requested.equals(cached);}
        boolean request(String next,String frame,boolean transition,boolean cached){boolean changed=!next.equals(id)||!frame.equals(geometry);id=next;geometry=frame;return next.isEmpty()||transition||!cached||changed;}
    }
    static final class BlendSupport {static boolean rootContext(int mode,boolean parent,boolean window,boolean wave){return mode!=18||!parent&&!window&&!wave;}static boolean supported(int mode){return mode==0||mode==1||mode==2||mode==3||mode==4||mode==5||mode==6||mode==7||mode==8||mode==9||mode==10||mode==11||mode==12||mode==13||mode==14||mode==15||mode==16||mode==17||mode==18||mode==19||mode==20||mode==21||mode==22||mode==23||mode==24||mode==25;}}
    static final class ColorSupport {static boolean channelPalette(int mode){return mode>=8&&mode<=82;}static boolean supported(int mode){return mode>=0&&mode<=82;}}
    static final class QuadGeometry {
        // Per GL thread: rotate/flip in the current viewport BEFORE nonuniform capture normalization.
        static final ThreadLocal<float[]> captureProjection=new ThreadLocal<>();
        static void applyCapture(FloatBuffer out,float left,float top,float right,float bottom,float degrees,float u0,float v0,float u1,float v1,float pivotX,float pivotY,int mode,float intrinsicWidth,float intrinsicHeight){
            float[] p=captureProjection.get();if(p==null)return;
            write(out,p[0]+left/1920*p[2],p[1]+top/1080*p[3],p[0]+right/1920*p[2],p[1]+bottom/1080*p[3],degrees,p[4],p[5],u0,v0,u1,v1,pivotX,pivotY,mode,intrinsicWidth,intrinsicHeight);
            for(int vertex=0;vertex<4;vertex++){int at=vertex*4;float x=(out.get(at)+1)*p[4]/2,y=(1-out.get(at+1))*p[5]/2;out.put(at,(x-p[0])/p[2]*2-1);out.put(at+1,1-(y-p[1])/p[3]*2);}
        }
        static void write(FloatBuffer out,float left,float top,float right,float bottom,float degrees,float viewportWidth,float viewportHeight,float u0,float v0,float u1,float v1){
            write(out,left,top,right,bottom,degrees,viewportWidth,viewportHeight,u0,v0,u1,v1,.5f,.5f);
        }
        static void write(FloatBuffer out,float left,float top,float right,float bottom,float degrees,float viewportWidth,float viewportHeight,float u0,float v0,float u1,float v1,float pivotX,float pivotY){
            float cx=left+(right-left)*pivotX,cy=top+(bottom-top)*pivotY;
            double radians=Math.toRadians(degrees%360f);float cos=(float)Math.cos(radians),sin=(float)Math.sin(radians);
            out.clear();
            vertex(out,left,top,u0,v0,cx,cy,cos,sin,viewportWidth,viewportHeight);
            vertex(out,left,bottom,u0,v1,cx,cy,cos,sin,viewportWidth,viewportHeight);
            vertex(out,right,top,u1,v0,cx,cy,cos,sin,viewportWidth,viewportHeight);
            vertex(out,right,bottom,u1,v1,cx,cy,cos,sin,viewportWidth,viewportHeight);out.flip();
        }
        static void write(FloatBuffer out,float left,float top,float right,float bottom,float degrees,float viewportWidth,float viewportHeight,float u0,float v0,float u1,float v1,float pivotX,float pivotY,int mode,float intrinsicWidth,float intrinsicHeight){
            if(mode==0){write(out,left,top,right,bottom,degrees,viewportWidth,viewportHeight,u0,v0,u1,v1,pivotX,pivotY);return;}
            if(mode!=1||!Float.isFinite(intrinsicWidth)||!Float.isFinite(intrinsicHeight)||intrinsicWidth<=0||intrinsicHeight<=0)throw new IllegalArgumentException("Unsupported native matrix mode or intrinsic extent");
            float w=right-left,h=bottom-top,cx=left+w*pivotX,cy=top+h*pivotY;
            double radians=Math.toRadians(degrees%360f);float cos=(float)Math.cos(radians),sin=(float)Math.sin(radians);
            out.clear();
            matrixVertex(out,-pivotX,-pivotY,u0,v0,cx,cy,w,h,cos,sin,intrinsicWidth,intrinsicHeight,viewportWidth,viewportHeight);
            matrixVertex(out,-pivotX,1-pivotY,u0,v1,cx,cy,w,h,cos,sin,intrinsicWidth,intrinsicHeight,viewportWidth,viewportHeight);
            matrixVertex(out,1-pivotX,-pivotY,u1,v0,cx,cy,w,h,cos,sin,intrinsicWidth,intrinsicHeight,viewportWidth,viewportHeight);
            matrixVertex(out,1-pivotX,1-pivotY,u1,v1,cx,cy,w,h,cos,sin,intrinsicWidth,intrinsicHeight,viewportWidth,viewportHeight);out.flip();
        }
        private static void matrixVertex(FloatBuffer out,float dx,float dy,float u,float v,float cx,float cy,float w,float h,float cos,float sin,float intrinsicWidth,float intrinsicHeight,float viewportWidth,float viewportHeight){
            // Native matrix1 rotates intrinsic coordinates first, then axis-scales.
            float px=cx+cos*w*dx-sin*w*(intrinsicHeight/intrinsicWidth)*dy;
            float py=cy+sin*h*(intrinsicWidth/intrinsicHeight)*dx+cos*h*dy;
            out.put(px/viewportWidth*2-1).put(1-py/viewportHeight*2).put(u).put(v);
        }
        private static void vertex(FloatBuffer out,float x,float y,float u,float v,float cx,float cy,float cos,float sin,float width,float height){
            float dx=x-cx,dy=y-cy;
            // Screen Y grows downwards, so a positive angle rotates clockwise.
            float px=cx+dx*cos-dy*sin,py=cy+dx*sin+dy*cos;
            out.put(px/width*2-1).put(1-py/height*2).put(u).put(v);
        }
    }
    // Kept free of Android types so residency and readiness can be checked on a host JVM.
    static final class Residency<T> {
        private final LinkedHashMap<String,T> entries=new LinkedHashMap<>(16,.75f,true);
        private final long budget;
        private final ToLongFunction<T> size;
        private long bytes;
        Residency(long budget,ToLongFunction<T> size){this.budget=budget;this.size=size;}
        static boolean visible(double alpha){return Double.isFinite(alpha)&&alpha>.0001;}
        static boolean required(double alpha,boolean preload){return preload||visible(alpha);}
        boolean contains(String name){return entries.containsKey(name);}
        T get(String name){return entries.get(name);}
        T put(String name,T value){T old=entries.put(name,value);bytes+=size.applyAsLong(value)-(old==null?0:size.applyAsLong(old));return old;}
        void clear(){entries.clear();bytes=0;}
        long bytes(){return bytes;}
        List<T> trim(Set<String> pinned){
            List<T> removed=new ArrayList<>();Iterator<Map.Entry<String,T>> it=entries.entrySet().iterator();
            while(bytes>budget&&it.hasNext()){Map.Entry<String,T> entry=it.next();if(pinned.contains(entry.getKey()))continue;removed.add(entry.getValue());bytes-=size.applyAsLong(entry.getValue());it.remove();}
            return removed;
        }
    }
    private static int shader(int kind,String source){int id=GLES30.glCreateShader(kind);GLES30.glShaderSource(id,source);GLES30.glCompileShader(id);int[] ok=new int[1];GLES30.glGetShaderiv(id,GLES30.GL_COMPILE_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetShaderInfoLog(id));return id;}
    private static int linkedProgram(String vertex,String fragment){int id=GLES30.glCreateProgram(),v=shader(GLES30.GL_VERTEX_SHADER,vertex),f=shader(GLES30.GL_FRAGMENT_SHADER,fragment);GLES30.glAttachShader(id,v);GLES30.glAttachShader(id,f);GLES30.glLinkProgram(id);GLES30.glDeleteShader(v);GLES30.glDeleteShader(f);int[] ok=new int[1];GLES30.glGetProgramiv(id,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetProgramInfoLog(id));return id;}
    private static final class Texture{final int id,width,height;int originalWidth,originalHeight;boolean premultiplied;Texture(int id,int width,int height){this.id=id;this.width=width;this.height=height;originalWidth=width;originalHeight=height;}}
}
