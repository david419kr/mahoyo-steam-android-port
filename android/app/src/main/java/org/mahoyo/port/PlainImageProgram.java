package org.mahoyo.port;

import android.opengl.GLES30;
import java.nio.FloatBuffer;

/** Same straight-alpha resampling math as the general material, without effect branches. */
final class PlainImageProgram {
    static final String VERTEX="#version 300 es\nin vec2 aPosition;in vec2 aUv;out vec2 vUv;void main(){vUv=aUv;gl_Position=vec4(aPosition,0.,1.);}";
    static final String FRAGMENT="#version 300 es\nprecision highp float;precision highp int;uniform sampler2D uImage;uniform sampler2D uDestination;uniform vec2 uSize;uniform vec4 uBounds;uniform vec4 uTint;uniform float uOpacity;uniform int uPremultiplied;uniform int uRawOutput;uniform int uHardware;uniform int uKernel;in vec2 vUv;out vec4 frag;"+
        "vec4 sourceTexel(ivec2 p){ivec2 lo=ivec2(floor(uBounds.xy*uSize));ivec2 hi=ivec2(floor(uBounds.zw*uSize));vec4 c=texelFetch(uImage,clamp(p,lo,hi),0);if(uPremultiplied!=0)c.rgb=c.a>.000001?c.rgb/c.a:vec3(0.);return c;}"+
        "float sinc(float x){x=abs(x);if(x<.00001)return 1.;float p=3.14159265359*x;return sin(p)/p;}"+
        "float cubic(float x){x=abs(x);if(x<1.)return (1.5*x-2.5)*x*x+1.;if(x<2.)return ((-.5*x+2.5)*x-4.)*x+2.;return 0.;}"+
        "float weight(float x){return uKernel==1?cubic(x):(abs(x)<3.?sinc(x)*sinc(x/3.):0.);}"+
        "vec4 sampleImage(vec2 uv){if(uKernel==0&&uHardware!=0&&uPremultiplied==0)return texture(uImage,clamp(uv,uBounds.xy,uBounds.zw));vec2 p=uv*uSize-.5;ivec2 base=ivec2(floor(p));vec2 f=p-vec2(base);if(uKernel==0){vec4 top=mix(sourceTexel(base),sourceTexel(base+ivec2(1,0)),f.x);vec4 bottom=mix(sourceTexel(base+ivec2(0,1)),sourceTexel(base+ivec2(1,1)),f.x);return mix(top,bottom,f.y);}float wx[6],wy[6];for(int i=0;i<6;i++){wx[i]=weight(float(i-2)-f.x);wy[i]=weight(float(i-2)-f.y);}vec4 c=vec4(0.);float total=0.;for(int y=-2;y<=3;y++)for(int x=-2;x<=3;x++){float w=wx[x+2]*wy[y+2];c+=sourceTexel(base+ivec2(x,y))*w;total+=w;}return clamp(c/max(total,.0001),0.,1.);}"+
        "float nativeWindowByte(float a){return floor(clamp(a,0.,1.)*255.+.5)/255.;}"+
        "void main(){vec4 c=sampleImage(vUv);vec3 source=clamp(c.rgb*uTint.rgb,0.,1.);float a=c.a*uTint.a*uOpacity;if(uRawOutput==2){vec4 d=texelFetch(uDestination,ivec2(gl_FragCoord.xy),0);float w=min(nativeWindowByte(d.a*uOpacity),nativeWindowByte(c.a*uTint.a*uOpacity));frag=vec4(mix(d.rgb,source,w),d.a);}else if(uRawOutput==1)frag=vec4(source,a);else frag=vec4(source*a,a);}";
    private final Material[] materials=new Material[9];
    private boolean hardwareEnabled=true;
    int samplingKey(){return hardwareEnabled?1:0;}
    void contextLost(){java.util.Arrays.fill(materials,null);}
    void release(){for(Material material:materials)if(material!=null)material.release();contextLost();}
    void ensure(int filter,int output){int key=filter*3+output;if(materials[key]==null)materials[key]=new Material(filter,output);}
    void draw(FloatBuffer vertices,int texture,int width,int height,boolean isPremultiplied,float u0,float v0,float u1,float v1,int color,float alpha,int output,int filter){materials[filter*3+output].draw(vertices,texture,width,height,isPremultiplied,u0,v0,u1,v1,color,alpha,hardwareEnabled);}
    private static int shader(int type,String source){int id=GLES30.glCreateShader(type);GLES30.glShaderSource(id,source);GLES30.glCompileShader(id);int[] ok=new int[1];GLES30.glGetShaderiv(id,GLES30.GL_COMPILE_STATUS,ok,0);if(ok[0]==0){String error=GLES30.glGetShaderInfoLog(id);GLES30.glDeleteShader(id);throw new IllegalStateException(error);}return id;}
    // Fixed filter/output lets mobile compilers remove unused loops and destination reads entirely.
    static String fragment(int filter,int output){return FRAGMENT.replace("uniform int uKernel;","const int uKernel="+filter+";").replace("uniform int uRawOutput;","const int uRawOutput="+output+";");}
    private static final class Material {
        private int program,position,uv,image,destination,size,bounds,tint,opacity,premultiplied,hardware,linearSampler;
        Material(int filter,int output){
            int v=0,f=0;
            try{program=GLES30.glCreateProgram();v=shader(GLES30.GL_VERTEX_SHADER,VERTEX);f=shader(GLES30.GL_FRAGMENT_SHADER,fragment(filter,output));GLES30.glAttachShader(program,v);GLES30.glAttachShader(program,f);GLES30.glLinkProgram(program);int[] ok=new int[1];GLES30.glGetProgramiv(program,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES30.glGetProgramInfoLog(program));
                position=GLES30.glGetAttribLocation(program,"aPosition");uv=GLES30.glGetAttribLocation(program,"aUv");image=uniform("uImage");destination=uniform("uDestination");size=uniform("uSize");bounds=uniform("uBounds");tint=uniform("uTint");opacity=uniform("uOpacity");premultiplied=uniform("uPremultiplied");hardware=uniform("uHardware");int[] samplers=new int[1];GLES30.glGenSamplers(1,samplers,0);linearSampler=samplers[0];GLES30.glSamplerParameteri(linearSampler,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glSamplerParameteri(linearSampler,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glSamplerParameteri(linearSampler,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glSamplerParameteri(linearSampler,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);
            }catch(RuntimeException e){release();throw e;}finally{if(v!=0)GLES30.glDeleteShader(v);if(f!=0)GLES30.glDeleteShader(f);}
        }
        private int uniform(String name){return GLES30.glGetUniformLocation(program,name);}
        void release(){if(program!=0)GLES30.glDeleteProgram(program);if(linearSampler!=0)GLES30.glDeleteSamplers(1,new int[]{linearSampler},0);program=0;linearSampler=0;}
        void draw(FloatBuffer vertices,int texture,int width,int height,boolean isPremultiplied,float u0,float v0,float u1,float v1,int color,float alpha,boolean hardwareEnabled){
            GLES30.glUseProgram(program);vertices.position(0);GLES30.glVertexAttribPointer(position,2,GLES30.GL_FLOAT,false,16,vertices);GLES30.glEnableVertexAttribArray(position);vertices.position(2);GLES30.glVertexAttribPointer(uv,2,GLES30.GL_FLOAT,false,16,vertices);GLES30.glEnableVertexAttribArray(uv);
            GLES30.glActiveTexture(GLES30.GL_TEXTURE0);GLES30.glBindTexture(GLES30.GL_TEXTURE_2D,texture);GLES30.glUniform1i(image,0);GLES30.glUniform1i(destination,1);GLES30.glUniform2f(size,width,height);GLES30.glUniform4f(bounds,Math.min(u0,u1)+.5f/width,Math.min(v0,v1)+.5f/height,Math.max(u0,u1)-.5f/width,Math.max(v0,v1)-.5f/height);GLES30.glUniform1i(premultiplied,isPremultiplied?1:0);GLES30.glUniform1i(hardware,hardwareEnabled?1:0);GLES30.glBindSampler(0,linearSampler);GLES30.glUniform1f(opacity,Math.max(0,Math.min(1,alpha)));GLES30.glUniform4f(tint,android.graphics.Color.red(color)/255f,android.graphics.Color.green(color)/255f,android.graphics.Color.blue(color)/255f,android.graphics.Color.alpha(color)/255f);GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP,0,4);GLES30.glBindSampler(0,0);
        }
    }
}
