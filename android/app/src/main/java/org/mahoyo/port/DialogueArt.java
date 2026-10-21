package org.mahoyo.port;

import android.content.Context;
import android.graphics.*;
import java.io.*;

/** Immutable original pixels, loaded once and shared by live Canvas and frozen frames. */
final class DialogueArt {
    private static volatile Bitmap window,cursor;
    static synchronized void load(Context context){
        if(window!=null)return;
        try(InputStream w=context.getAssets().open("dialogue/window.png");InputStream c=context.getAssets().open("dialogue/cursor.png")){
            BitmapFactory.Options options=new BitmapFactory.Options();options.inScaled=false;
            Bitmap panel=BitmapFactory.decodeStream(w,null,options),drop=BitmapFactory.decodeStream(c,null,options);
            if(panel==null||drop==null||panel.getWidth()!=1920||panel.getHeight()!=1080||drop.getWidth()!=900||drop.getHeight()!=124)throw new IOException("Invalid original dialogue artwork");
            cursor=drop;window=panel;
        }catch(IOException error){throw new IllegalStateException("Original dialogue artwork unavailable",error);}
    }
    static void frame(Canvas canvas,RectF box,boolean split,float alpha){
        Bitmap image=window;if(image==null||alpha<=0)return;Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);paint.setAlpha(Math.round(255*Math.min(1,alpha)));
        float strip=Math.min(box.height()*(split?.13f:.10f),box.width()/1920f*96);
        canvas.drawBitmap(image,new Rect(0,0,1920,96),new RectF(box.left,box.top,box.right,box.top+strip),paint);
        canvas.drawBitmap(image,new Rect(0,984,1920,1080),new RectF(box.left,box.bottom-strip,box.right,box.bottom),paint);
    }
    static void cursor(Canvas canvas,float x,float baseline,float font,float right,float alpha,boolean animated){
        Bitmap image=cursor;if(image==null)return;int frame=animated?(int)((android.os.SystemClock.uptimeMillis()/50)%40):0,col=frame%20,row=frame/20;
        float width=font*.70f,height=width*56/45;if(x+width>right){x=0;baseline+=font*1.65f;}
        Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);paint.setAlpha(Math.round(255*Math.max(0,Math.min(1,alpha))));
        canvas.drawBitmap(image,new Rect(col*45,row*56,col*45+45,row*56+56),new RectF(x-font*.12f,baseline-font*.48f,x-font*.12f+width,baseline-font*.48f+height),paint);
    }
}
