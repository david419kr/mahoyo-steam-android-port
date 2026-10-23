package org.mahoyo.port;

import android.graphics.*;
import android.opengl.GLSurfaceView;
import android.os.*;
import android.view.PixelCopy;
import java.util.function.*;

/** Surface-only image + the same committed native text overlay; never Window pixels. */
final class SlotThumbnailCapture {
    static final class Plan {
        final int left,top,width,height,outWidth,outHeight;final float scaleX,scaleY;
        Plan(int left,int top,int width,int height){if(width<1||height<1)throw new IllegalArgumentException("Empty thumbnail area");this.left=left;this.top=top;this.width=width;this.height=height;float scale=Math.min(1,480f/Math.max(width,height));outWidth=Math.max(1,Math.round(width*scale));outHeight=Math.max(1,Math.round(height*scale));scaleX=(float)outWidth/width;scaleY=(float)outHeight/height;}
        Rect sourceArea(int viewWidth,int viewHeight,int bufferWidth,int bufferHeight){
            if(viewWidth<1||viewHeight<1||bufferWidth<1||bufferHeight<1)throw new IllegalArgumentException("Empty Surface");
            return new Rect(Math.round((float)left*bufferWidth/viewWidth),Math.round((float)top*bufferHeight/viewHeight),Math.round((float)(left+width)*bufferWidth/viewWidth),Math.round((float)(top+height)*bufferHeight/viewHeight));
        }
        float x(float surfaceX){return (surfaceX-left)*scaleX;}float y(float surfaceY){return (surfaceY-top)*scaleY;}
    }
    static void request(GLSurfaceView surface,TextOverlay text,Rect area,BooleanSupplier valid,Consumer<Bitmap> completion){
        Handler ui=new Handler(Looper.getMainLooper());Plan plan;
        try{plan=new Plan(area.left,area.top,area.width(),area.height());}catch(IllegalArgumentException badArea){completion.accept(null);return;}
        int width=surface.getWidth(),height=surface.getHeight();int[] surfacePosition=new int[2],textPosition=new int[2];surface.getLocationInWindow(surfacePosition);text.getLocationInWindow(textPosition);Bitmap bitmap=Bitmap.createBitmap(plan.outWidth,plan.outHeight,Bitmap.Config.ARGB_8888);
        boolean[] finished={false};Runnable timeout=()->{if(!finished[0]){finished[0]=true;completion.accept(null);}};
        ui.postDelayed(timeout,1500);
        Rect buffer=surface.getHolder().getSurfaceFrame();int bufferWidth=buffer.width(),bufferHeight=buffer.height();
        try{PixelCopy.request(surface,plan.sourceArea(width,height,bufferWidth,bufferHeight),bitmap,result->{ui.removeCallbacks(timeout);if(finished[0]){bitmap.recycle();return;}finished[0]=true;
            int[] nowSurface=new int[2],nowText=new int[2];surface.getLocationInWindow(nowSurface);text.getLocationInWindow(nowText);
            if(result!=PixelCopy.SUCCESS||!valid.getAsBoolean()||surface.getWidth()!=width||surface.getHeight()!=height||surface.getHolder().getSurfaceFrame().width()!=bufferWidth||surface.getHolder().getSurfaceFrame().height()!=bufferHeight||nowSurface[0]!=surfacePosition[0]||nowSurface[1]!=surfacePosition[1]||nowText[0]!=textPosition[0]||nowText[1]!=textPosition[1]){bitmap.recycle();completion.accept(null);return;}
            try{Canvas canvas=new Canvas(bitmap);canvas.scale(plan.scaleX,plan.scaleY);canvas.translate(textPosition[0]-surfacePosition[0]-plan.left,textPosition[1]-surfacePosition[1]-plan.top);if(text.getVisibility()==android.view.View.VISIBLE)text.draw(canvas);}
            catch(RuntimeException error){bitmap.recycle();completion.accept(null);return;}completion.accept(bitmap);
        },ui);}catch(RuntimeException failure){ui.removeCallbacks(timeout);finished[0]=true;bitmap.recycle();completion.accept(null);}
    }
}
