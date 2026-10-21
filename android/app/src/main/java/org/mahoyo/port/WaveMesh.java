package org.mahoyo.port;

import org.json.JSONObject;
import java.nio.FloatBuffer;

/** Native258 presets in source bitmap pixels, before the existing image affine. */
final class WaveMesh {
    static final int WIDTH=1920,HEIGHT=1080,CELL_WIDTH=512;
    private static final int[] BATCH_HEIGHT={32,32,16,64,256,256,256,4,64,64,256,16,4,64,64,64,128,64,64,8,8,64,64,64,64};
    private static final float[][] BATCH_PARAMS={
        {1.0f,0.0f,3.0f,0.0f,0.0f,0.0f,18.75f,4.0f,0.07200000435113907f,0.0f},
        {1.0f,0.0f,3.0f,0.0f,0.0f,0.0f,18.75f,4.0f,0.07200000435113907f,0.0f},
        {1.0f,0.0f,4.0f,2.0f,0.0f,1.0f,37.5f,2.0f,0.14400000870227814f,0.0f},
        {1.0f,0.0f,2.0f,3.0f,0.0f,1.0f,37.5f,0.4000000059604645f,0.14400000870227814f,0.0f},
        {1.0f,0.0f,3.0f,2.0f,0.0f,3.0f,18.75f,1.0f,0.07200000435113907f,0.0f},
        {1.0f,0.0f,3.0f,0.0f,0.0f,0.0f,18.75f,1.600000023841858f,0.14400000870227814f,0.0f},
        {1.0f,0.0f,2.0f,0.0f,0.0f,0.0f,18.75f,1.600000023841858f,0.14400000870227814f,0.0f},
        {1.0f,0.0f,1.0f,100.0f,1.0f,1.0f,11.25f,1.2000000476837158f,0.7200000286102295f,0.0f},
        {1.0f,0.0f,1.0f,1.0f,10.0f,3.0f,3.75f,1.2000000476837158f,0.7200000286102295f,0.0f},
        {1.0f,0.0f,1.0f,1.0f,10.0f,3.0f,3.75f,1.2000000476837158f,0.7200000286102295f,0.0f},
        {1.0f,0.0f,1.0f,1.0f,10.0f,3.0f,3.75f,1.2000000476837158f,0.7200000286102295f,0.0f},
        {1.0f,10.0f,1.0f,0.0f,0.0f,0.0f,18.75f,1.399999976158142f,0.36000001430511475f,100.0f}
,
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,2.8125f,120.0f,7.200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,16.875f,6.0f,1.0800000429153442f,3000.0f},
        {1.0f,0.0f,1.0f,5.0f,0.0f,2.0f,0.375f,4.0f,2.1600000858306885f,0.0f},
        {1.0f,0.0f,1.0f,5.0f,0.0f,2.0f,0.375f,4.0f,2.1600000858306885f,0.0f},
        {1.0f,1.0f,2.0f,2.0f,1.0f,2.0f,5.625f,2.0f,0.7200000286102295f,0.0f}
,
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,1.875f,10.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,1.875f,10.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,93.75f,10.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,93.75f,10.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,9.375f,3.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,9.375f,3.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,9.375f,3.0f,0.7200000286102295f,0.0f},
        {1.0f,1.0f,1.0f,0.0f,0.0f,0.0f,9.375f,3.0f,0.7200000286102295f,0.0f}
    };
    static final class Spec {
        final int preset;final float strength;final long elapsed;
        Spec(int preset,float strength,long elapsed){
            if((preset<2||preset>48||preset==9)||!Float.isFinite(strength)||strength<0||strength>1||elapsed<0||elapsed>0xffffffffL)throw new IllegalArgumentException("Unreviewed native wave preset/strength/phase");
            this.preset=preset;this.strength=strength;this.elapsed=elapsed;
        }
        static Spec from(JSONObject value){if(value==null)return null;double time=value.optDouble("elapsedMs",Double.NaN);if(!Double.isFinite(time)||time!=Math.floor(time))throw new IllegalArgumentException("Invalid native wave clock");return new Spec(value.optInt("preset",0),(float)value.optDouble("strength",Double.NaN),(long)time);}
        int cellHeight(){if(preset>=24&&preset<=48)return BATCH_HEIGHT[preset-24];return preset==13||preset==14||preset==18||preset==20?64:preset==10?32:preset==4||preset==5||preset==7||preset==8?128:256;}
    }
    static int cells(int width,int height,Spec spec){
        if(width<=0||height<=0||width>16384||height>16384)throw new IllegalArgumentException("Invalid wave bitmap extent");
        int cells=((width+511)/512)*((height+spec.cellHeight()-1)/spec.cellHeight());
        if(cells>=1000&&!((spec.preset==31&&cells<=1590)||(spec.preset==36&&cells<=5084)||((spec.preset==43||spec.preset==44)&&cells<=3051)))throw new IllegalArgumentException("Unreviewed native wave mode0 mesh size");return cells;
    }
    static float nativeRootOpacity(double alpha){
        int fixed=(int)(float)(Math.max(0,Math.min(1,alpha))*65536.);
        return ((fixed*255)>>16)*(1f/255f);
    }
    static float[] delta(Spec spec,float y){
        if(spec.preset>=24&&spec.preset<=48){
            float[] p=BATCH_PARAMS[spec.preset-24];float phase=(p[7]*y+(float)(int)spec.elapsed*p[8])*.004363323f,k=spec.strength*p[6];
            return new float[]{((float)Math.cos(p[0]*phase+p[1])*p[2])*k,((float)Math.cos(p[3]*phase+p[4])*p[5])*k};
        }
        if(spec.preset==23){
            float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*5.625f;
            return new float[]{(float)Math.cos(phase)*k,((float)Math.cos(2f*phase+10f)*2f)*k};
        }
        if(spec.preset==22){
            float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*56.25f;
            return new float[]{((float)Math.cos(2f*phase+1f)*2f)*k,0f*k};
        }
        if(spec.preset==21){
            float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*5.625f;
            return new float[]{((float)Math.cos(phase+1f)*2f)*k,0f*k};
        }
        if(spec.preset==20){
            float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*7.5f;
            return new float[]{(float)Math.cos(phase)*k,((float)Math.cos(phase+1f)*2f)*k};
        }
        if(spec.preset==19){
            float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*3.75f;
            return new float[]{((float)Math.cos(phase+1f)*4f)*k,0f*k};
        }
        if(spec.preset==18){
            float phase=(2f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*11.25f;
            return new float[]{(float)Math.cos(phase)*k,((float)Math.cos(phase)*2f)*k};
        }
        if(spec.preset==17){
            float phase=(y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*1.875f;
            return new float[]{(float)Math.cos(phase)*k,((float)Math.cos(phase+1f)*8f)*k};
        }
        if(spec.preset==16){
            float phase=(2f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*37.5f;
            return new float[]{(float)Math.cos(2f*phase)*k,0f*k};
        }
        if(spec.preset==15){
            float phase=(y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*37.5f;
            return new float[]{((float)Math.cos(phase)*2f)*k,0f*k};
        }
        if(spec.preset==14){
            float phase=(y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*18.75f;
            return new float[]{(float)Math.cos(phase)*k,(float)Math.cos(2f*phase)*k};
        }
        if(spec.preset==13){
            float phase=(1.600000023841858f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*37.5f;
            return new float[]{(float)Math.cos(phase)*k,(float)Math.cos(phase+2f)*k};
        }
        if(spec.preset==12){
            float phase=(y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*3.75f;
            return new float[]{(float)Math.cos(phase)*k,((float)Math.cos(phase+1f)*2f)*k};
        }
        if(spec.preset==10){
            //141E80 normalizes raw p0/p2 zero to one; native 512x32 mode26.
            float phase=(10f*y+(float)(int)spec.elapsed*1.440000057220459f)*.004363323f,k=spec.strength*11.25f;
            return new float[]{(float)Math.cos(phase)*k,(float)Math.cos(phase+1f)*k};
        }
        if(spec.preset==11){
            //p9=900 is retained by the original setter but unread by the
            //reviewed complete780E0 consumer. Do not reinterpret as a pivot.
            float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*5.625f;
            return new float[]{(float)Math.cos(phase)*k,((float)Math.cos(3f*phase+1f)*2f)*k};
        }
        float p0=spec.preset==3||spec.preset==5?2:1,p1=spec.preset==4?10:1,p2=spec.preset==8?-10:spec.preset==7?10:spec.preset==6?20:2,p3=spec.preset==7||spec.preset==8?0:spec.preset==2||spec.preset==6?2:spec.preset==4?0:1,p4=spec.preset==4||spec.preset==7||spec.preset==8?0:1,p5=spec.preset==7||spec.preset==8?0:spec.preset==6?20:spec.preset==4?0:2;
        float phase=(1.2000000476837158f*y+(float)(int)spec.elapsed*.7200000286102295f)*.004363323f,k=spec.strength*(spec.preset==6?.375f:1.875f);
        return new float[]{((float)Math.cos(p0*phase+p1)*p2)*k,((float)Math.cos(p3*phase+p4)*p5)*k};
    }
    static float[] sourceQuad(int width,int height,int column,int row,Spec spec){
        float x0=column*512f,y0=row*(float)spec.cellHeight(),x1=x0+Math.min(512,width),y1=y0+Math.min(spec.cellHeight(),height);
        float[] top=delta(spec,y0),bottom=delta(spec,y1);
        return new float[]{x0+top[0],y0+top[1],x1+top[0],y0+top[1],x0+bottom[0],y1+bottom[1],x1+bottom[0],y1+bottom[1]};
    }
    /** basis is the exact QuadGeometry TL/BL/TR/BR clip-space quad. */
    static int write(FloatBuffer out,int width,int height,Spec spec,float[] basis,float u0,float v0,float u1,float v1){
        int count=cells(width,height,spec);if(basis.length<16||out.capacity()<count*24)throw new IllegalArgumentException("Invalid wave mesh buffers");out.clear();
        int columns=(width+511)/512,rows=(height+spec.cellHeight()-1)/spec.cellHeight();
        float[] top=delta(spec,0);
        for(int row=0;row<rows;row++){
            float y0=row*(float)spec.cellHeight(),y1=y0+Math.min(spec.cellHeight(),height);float[] bottom=delta(spec,y1);
            for(int column=0;column<columns;column++){
            float x0=column*512f,x1=x0+Math.min(512,width);
            //TL,BL,TR and TR,BL,BR retain native strip's TR--BL diagonal.
            vertex(out,x0+top[0],y0+top[1],x0,y0,width,height,basis,u0,v0,u1,v1);vertex(out,x0+bottom[0],y1+bottom[1],x0,y1,width,height,basis,u0,v0,u1,v1);vertex(out,x1+top[0],y0+top[1],x1,y0,width,height,basis,u0,v0,u1,v1);
            vertex(out,x1+top[0],y0+top[1],x1,y0,width,height,basis,u0,v0,u1,v1);vertex(out,x0+bottom[0],y1+bottom[1],x0,y1,width,height,basis,u0,v0,u1,v1);vertex(out,x1+bottom[0],y1+bottom[1],x1,y1,width,height,basis,u0,v0,u1,v1);
            }top=bottom;
        }out.flip();return count*6;
    }
    private static void vertex(FloatBuffer out,float x,float y,float qx,float qy,int width,int height,float[] b,float u0,float v0,float u1,float v1){
        float sx=x/width,sy=y/height;
        out.put(b[0]+(b[8]-b[0])*sx+(b[4]-b[0])*sy).put(b[1]+(b[9]-b[1])*sx+(b[5]-b[1])*sy);
        out.put(u0+(u1-u0)*(qx/width)).put(v0+(v1-v0)*(qy/height));
    }
}
