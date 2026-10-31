package org.mahoyo.port;

/** Pixel centers of the rendered frame, in the framebuffer's bottom-up texture coordinates. */
final class AmbientEdge {
    static float[] bounds(int width,int height,float left,float top,float right,float bottom){
        int[] rect=rect(width,height,left,top,right,bottom);int l=rect[0],t=rect[1],r=rect[2],b=rect[3];
        return new float[]{(l+.5f)/width,(height-b+.5f)/height,(r-.5f)/width,(height-t-.5f)/height};
    }
    static int[] rect(int width,int height,float left,float top,float right,float bottom){
        // A pixel center exactly on a quad edge is not guaranteed to be covered by rasterization.
        int l=Math.max(0,Math.min(width-1,(int)Math.floor(left+.501f))),r=Math.max(l+1,Math.min(width,(int)Math.ceil(right-.501f)));
        int t=Math.max(0,Math.min(height-1,(int)Math.floor(top+.501f))),b=Math.max(t+1,Math.min(height,(int)Math.ceil(bottom-.501f)));
        return new int[]{l,t,r,b};
    }
    static int[] blurSize(int width,int height,int strength){
        if(strength<=0)return new int[]{width,height};
        float scale=Math.max(1,Math.min(width,height)*.0008f*Math.min(100,strength)/1.7f);
        scale=Math.max(scale,Math.max(width,height)/512f);
        return new int[]{Math.max(1,Math.round(width/scale)),Math.max(1,Math.round(height/scale))};
    }
    static int nextSize(int current,int target){return Math.max(target,(current+1)/2);}
    static final float[] WEIGHTS={.22702703f,.19459459f,.12162162f,.05405405f,.01621622f};
}
