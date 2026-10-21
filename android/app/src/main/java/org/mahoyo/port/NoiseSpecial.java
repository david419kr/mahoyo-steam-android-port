package org.mahoyo.port;

/** CPU/reference helpers for the separately proven special noise callbacks. */
final class NoiseSpecial {
    static final String SHADER=
        "vec3 nativeNoise82(vec2 uv,int q){vec3 m=nativeNoisePoint(nativeNoiseUv(fract(uv*2.),q^3))*(192./255.);return min(nativeNoisePoint(nativeNoiseUv(uv,q)),min(m*2.,vec3(1.)));}"+
        "vec3 nativeFinal28(vec3 d,vec4 s,float opacity,int family){float a=floor(clamp(opacity,0.,1.)*255.+.5);float red=family==120?floor((a+1.)/2.):100.;float r=red*(1./255.);float g=128.*(1./255.);float o=a*(1./255.);vec3 den=max(vec3(.001),vec3(1.)-(s.rgb*r)*1.9921875);vec3 f=min(d/den,step(vec3(3./255.),d));float cover=min((s.a*g)*1.9921875,1.)*o;return cover*(f-d)+d;}";
    static int[] rectangle85(int index){
        if(index<1||index>11)throw new IllegalArgumentException("Invalid Native85 rectangle");
        int left=1389+2*index,grey=255-255*index/12;
        return new int[]{left,0,left+48-4*index,1080,grey};
    }
    static float[] prepass(int profile,float maskRGB,float maskAlpha,float n0,float n2,int pixelX){
        float rgb=n0*maskRGB,a=maskAlpha;
        if(profile==78)rgb=Math.min(rgb,n2*(192f/255));
        else if(profile==82)rgb=Math.min(rgb,Math.min(2*(n2*(192f/255)),1));
        else if(profile==85){rgb=Math.min(1,n2*(64f/255)+rgb*(192f/255));for(int i=1;i<=11;i++){int[] r=rectangle85(i);if(pixelX>=r[0]&&pixelX<r[2]){float v=r[4]/255f;rgb=Math.min(rgb,v);a=Math.min(a,v);}}}
        else throw new IllegalArgumentException("Unreviewed special profile");
        return new float[]{rgb,a};
    }
    static float[] final28(float[] d,float[] s,int e0,int family,int q){
        if(e0<0||e0>255||(family!=85&&family!=120)||(q!=0&&q!=1))throw new IllegalArgumentException("Invalid final28 fixture");
        float inv=1f/255,r=(family==120?(e0+1)/2:100)*inv,g=128*inv,o=e0*inv,k=1.9921875f;
        float cover=Math.min((s[3]*g)*k,1)*o;float[] out=new float[4];
        for(int i=0;i<3;i++){float v=s[i]*r,den=Math.max(.001f,(float)((double)v*(-k)+1));float dodge=(1f/den)*d[i];float threshold=d[i]+(-.0117647061f)>=0?1:0;float diff=Math.min(dodge,threshold)-d[i];out[i]=(float)((double)cover*diff+d[i]);}
        out[3]=(float)((double)q*(d[3]-1f)+1);return out;
    }
}
