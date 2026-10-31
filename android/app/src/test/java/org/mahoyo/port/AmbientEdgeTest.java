package org.mahoyo.port;

import org.junit.Test;
import static org.junit.Assert.*;

public final class AmbientEdgeTest {
    @Test public void usesActualFirstAndLastPixelsInsteadOfInsetMipTexels(){
        float[] b=AmbientEdge.bounds(2160,1584,0,134,2160,1349);
        assertArrayEquals(new float[]{.5f,235.5f,2159.5f,1449.5f},pixels(b,2160,1584),.001f);
    }
    @Test public void menuAndSplitPanelOffsetsDoNotRescaleTheSample(){
        float[] b=AmbientEdge.bounds(2400,1080,240,80,2160,1000);
        assertArrayEquals(new float[]{240.5f,80.5f,2159.5f,999.5f},pixels(b,2400,1080),.001f);
    }
    @Test public void fractionalFrameMatchesTheRenderersScissorRounding(){
        assertArrayEquals(AmbientEdge.bounds(1000,800,13,61,989,740),AmbientEdge.bounds(1000,800,12.6f,61.4f,988.8f,739.7f),0);
    }
    @Test public void halfPixelBottomDoesNotSampleTheUncoveredBlackRow(){
        assertArrayEquals(new int[]{0,134,2160,1348},AmbientEdge.rect(2160,1584,0,133.5f,2160,1348.5f));
        float[] bounds=AmbientEdge.bounds(2160,1584,0,133.5f,2160,1348.5f);
        assertEquals(1347.5f,1584-bounds[1]*1584,.0001f);
        assertArrayEquals(new int[]{12,1,191,100},AmbientEdge.rect(200,200,11.5f,.5f,191.5f,100.5f));
    }
    @Test public void halfResolutionPreservesTheSameVisibleBoundary(){
        float[] full=AmbientEdge.bounds(2160,1584,0,134,2160,1350),half=AmbientEdge.bounds(1080,792,0,67,1080,675);
        assertEquals(full[1]-half[1],-.5f/1584,.000001f);
        assertEquals(full[3]-half[3],.5f/1584,.000001f);
        assertArrayEquals(AmbientEdge.blurSize(2160,1216,50),AmbientEdge.blurSize(1080,608,50));
    }
    @Test public void reductionNeverSkipsSourcePixelsAtAnyStrength(){
        for(int strength=1;strength<=100;strength++)for(int[] extent:new int[][]{{2160,1482},{1960,840},{1920,1080},{143,91},{1,1}}){
            int w=extent[0],h=extent[1];int[] target=AmbientEdge.blurSize(w,h,strength);int passes=0;
            do{
                int nextW=AmbientEdge.nextSize(w,target[0]),nextH=AmbientEdge.nextSize(h,target[1]);
                assertTrue(w<=2*nextW&&h<=2*nextH);assertTrue(nextW<=w&&nextH<=h);
                w=nextW;h=nextH;assertTrue(++passes<20);
            }while(w>target[0]||h>target[1]);
            assertTrue(w<=512&&h<=512);
        }
    }
    @Test public void boundsStayInsideFramebufferOnTinyOrOverhangingFrames(){
        assertArrayEquals(new float[]{.5f,.5f,.5f,.5f},AmbientEdge.bounds(1,1,-10,-10,20,20),0);
    }
    @Test public void strengthChangesPrefilterResolutionInsteadOfGaussianTapSpacing(){
        assertArrayEquals(new int[]{2160,1482},AmbientEdge.blurSize(2160,1482,0));
        int[] weak=AmbientEdge.blurSize(2160,1482,1),medium=AmbientEdge.blurSize(2160,1482,50),strong=AmbientEdge.blurSize(2160,1482,100);
        assertTrue(weak[0]>medium[0]&&medium[0]>strong[0]);
        assertArrayEquals(strong,AmbientEdge.blurSize(2160,1482,999));
    }
    @Test public void gaussianPreservesFlatColorsAndHasNoSecondaryPeaksOrBrightnessShift(){
        float[] w=AmbientEdge.WEIGHTS;float sum=w[0];for(int i=1;i<w.length;i++){sum+=2*w[i];assertTrue(w[i]<w[i-1]&&w[i]>0);}
        assertEquals(1,sum,.000001f);
        // A hard boundary must remain monotonic; the old spaced taps produced repeated edges.
        float last=-1;for(int x=-12;x<=12;x++){float value=0;for(int i=-4;i<=4;i++)if(x+i>=0)value+=w[Math.abs(i)];assertTrue(value>=last);last=value;}
    }
    @Test public void alternatingFineLinesAreAveragedAwayBeforeTheGaussianAtEveryStrength(){
        for(int strength=1;strength<=100;strength++){
            float[] pixels=new float[2048];for(int i=0;i<pixels.length;i++)pixels[i]=i%2;
            int target=AmbientEdge.blurSize(pixels.length,1024,strength)[0];
            do{
                int size=AmbientEdge.nextSize(pixels.length,target);float[] reduced=new float[size];
                for(int x=0;x<size;x++){
                    float center=(x+.5f)*pixels.length/size-.5f,offset=pixels.length/(4f*size);
                    reduced[x]=(linear(pixels,center-offset)+linear(pixels,center+offset))*.5f;
                }
                pixels=reduced;
            }while(pixels.length>target);
            for(float value:pixels)assertEquals("strength "+strength,.5f,value,.00001f);
        }
    }
    private float linear(float[] values,float p){int low=(int)Math.floor(p);float f=p-low;return values[Math.max(0,Math.min(values.length-1,low))]*(1-f)+values[Math.max(0,Math.min(values.length-1,low+1))]*f;}
    private float[] pixels(float[] b,int w,int h){return new float[]{b[0]*w,b[1]*h,b[2]*w,b[3]*h};}
}
