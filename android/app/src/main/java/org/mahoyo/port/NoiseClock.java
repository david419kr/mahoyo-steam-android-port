package org.mahoyo.port;

import java.util.function.Supplier;

/** Native077580 due gate: one flip per actual draw, never missed-frame backfill. */
final class NoiseClock<T> {
    static final class Spec {
        final String token;final int selector,periodMs,profile;final long clockMs;
        Spec(String token,int selector,int periodMs,long clockMs){this(token,selector,periodMs,clockMs,0);}
        Spec(String token,int selector,int periodMs,long clockMs,int profile){
            if(profile!=0&&profile!=78&&profile!=82&&profile!=85)throw new IllegalArgumentException("Unreviewed native noise profile");
            this.profile=profile;
            if(token==null||token.isEmpty()||selector<1||selector>2||(periodMs!=50&&periodMs!=100&&periodMs!=150)||clockMs<0)throw new IllegalArgumentException("Unsupported native noise config");
            this.token=token;this.selector=selector;this.periodMs=periodMs;this.clockMs=clockMs;
        }
        String image(){return selector==1?"noise0.mzp":"noise1.mzp";}
    }
    static final class Frame<T> {
        final Spec spec;final long serial;final int orientation;final T source;
        Frame(Spec spec,long serial,int orientation,T source){this.spec=spec;this.serial=serial;this.orientation=orientation;this.source=source;}
        String key(){return spec.token+":"+serial;}
    }
    private Spec spec;private long last,serial;private int phase,nextOrientation;private double nextDue;private Frame<T> frame;
    Frame<T> sample(Spec value,Supplier<T> source){
        if(spec!=null&&(!spec.token.equals(value.token)||spec.selector!=value.selector||spec.periodMs!=value.periodMs))throw new IllegalArgumentException("Noise token reused for a different resource/period");
        if(spec==null)spec=value;
        // Native lastTick0 is an anchor sentinel, including after a real tick0.
        if(last==0)phase=0;else phase+=(int)(value.clockMs-last);last=value.clockMs;
        if(frame==null||due(phase,nextDue)){
            T frozen=source.get();nextDue=nextDue(phase,value.periodMs);frame=new Frame<>(value,++serial,nextOrientation,frozen);nextOrientation=(nextOrientation+1)&3;
        }
        return frame;
    }
    Frame<T> current(){return frame;}
    static boolean due(int phase,double nextDue){return Long.compareUnsigned((long)Math.floor(nextDue),(long)phase)<=0;}
    static double nextDue(int phase,double period){double value=phase-phase%period+period,whole=Math.floor(value),fraction=value-whole,epsilon=1./16384.;return fraction>=1-epsilon?whole+1:fraction<=epsilon?whole:value;}
    static float[] uv(int orientation,float u,float v){if(orientation<0||orientation>3)throw new IllegalArgumentException("Invalid noise orientation");return new float[]{(orientation&2)==0?u:1-u,(orientation&1)==0?v:1-v};}
}
