package org.mahoyo.port;

import org.json.JSONObject;
import java.nio.FloatBuffer;

/** Native global Screen Raster1/9. Local image wave presets remain separate. */
final class GlobalScreenWave {
    static final int WIDTH=1920,HEIGHT=1080,CAPTURE_MAX_PRIORITY=22559,PRIORITY=22560,CELLS=540;
    static final class Spec {
        final String id;final int preset;final float strength;final long elapsed;
        Spec(String id,int preset,float strength,long elapsed,int captureMax,int priority){
            if(id==null||id.isEmpty()||(preset!=1&&preset!=9)||!Float.isFinite(strength)||strength<0||strength>1||elapsed<0||elapsed>0xffffffffL||captureMax!=CAPTURE_MAX_PRIORITY||priority!=PRIORITY)throw new IllegalArgumentException("Unreviewed global Screen Raster state");
            this.id=id;this.preset=preset;this.strength=strength;this.elapsed=elapsed;
        }
        static Spec from(JSONObject value){if(value==null)return null;double t=value.optDouble("elapsedMs",Double.NaN);if(!Double.isFinite(t)||t!=Math.floor(t))throw new IllegalArgumentException("Invalid global Screen Raster clock");return new Spec(value.optString("id",""),value.optInt("preset",0),(float)value.optDouble("strength",Double.NaN),(long)t,value.optInt("captureMaxPriority",0),value.optInt("priority",0));}
    }
    static float offset(Spec spec,float y){
        if(spec.preset==1){
            float phase=(2f*y+(float)(int)spec.elapsed*3f)*.004363323f;
            return ((float)Math.cos(100f*phase+1f))*(spec.strength*15f);
        }
        float phase=(4.199999809265137f*y+(float)(int)spec.elapsed*.30000001192092896f)*.004363323f;
        return ((float)Math.cos(200f*phase+1f))*(spec.strength*56.25f);
    }
    static float[] quad(Spec spec,int row){
        if(row<0||row>=CELLS)throw new IllegalArgumentException("Invalid global Screen Raster row");
        float y=row*2f,top=offset(spec,y),bottom=offset(spec,y+2);
        return new float[]{top,y,WIDTH+top,y,bottom,y+2,WIDTH+bottom,y+2};
    }
    static int write(FloatBuffer out,Spec spec){
        if(out.capacity()<CELLS*24)throw new IllegalArgumentException("Global Screen Raster mesh buffer too small");out.clear();
        float top=offset(spec,0);
        for(int row=0;row<CELLS;row++){
            float y=row*2f,bottom=offset(spec,y+2);
            vertex(out,top,y,0,y);vertex(out,bottom,y+2,0,y+2);vertex(out,WIDTH+top,y,WIDTH,y);
            vertex(out,WIDTH+top,y,WIDTH,y);vertex(out,bottom,y+2,0,y+2);vertex(out,WIDTH+bottom,y+2,WIDTH,y+2);top=bottom;
        }
        out.flip();return CELLS*6;
    }
    private static void vertex(FloatBuffer out,float x,float y,float qx,float qy){out.put(x/WIDTH*2-1).put(1-y/HEIGHT*2).put(qx/WIDTH).put(1-qy/HEIGHT);}
}
