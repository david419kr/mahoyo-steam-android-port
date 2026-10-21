package org.mahoyo.port;

import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.view.*;
import android.widget.ImageView;

/** Standalone CG inspection: fit first, then bounded pinch/pan; no game transform. */
final class ZoomImageView extends ImageView {
    private float zoom=1,dx,dy,lastX,lastY;private final Matrix image=new Matrix();
    private final ScaleGestureDetector scale;private final GestureDetector taps;
    ZoomImageView(Context context){super(context);setScaleType(ScaleType.MATRIX);setBackgroundColor(0xff080f1c);setClickable(true);scale=new ScaleGestureDetector(context,new ScaleGestureDetector.SimpleOnScaleGestureListener(){@Override public boolean onScale(ScaleGestureDetector d){float next=boundedZoom(zoom*d.getScaleFactor()),factor=next/zoom;dx=d.getFocusX()-getWidth()/2f-(d.getFocusX()-getWidth()/2f-dx)*factor;dy=d.getFocusY()-getHeight()/2f-(d.getFocusY()-getHeight()/2f-dy)*factor;zoom=next;position();return true;}});taps=new GestureDetector(context,new GestureDetector.SimpleOnGestureListener(){@Override public boolean onDown(MotionEvent e){return true;}@Override public boolean onDoubleTap(MotionEvent e){fit();return true;}});}
    static float boundedZoom(float value){return Float.isFinite(value)?Math.max(1,Math.min(4,value)):1;}
    void fit(){zoom=1;dx=dy=0;position();}
    @Override public void setImageBitmap(Bitmap value){super.setImageBitmap(value);fit();}
    @Override protected void onSizeChanged(int w,int h,int ow,int oh){super.onSizeChanged(w,h,ow,oh);position();}
    private void position(){Drawable d=getDrawable();if(d==null||getWidth()==0||getHeight()==0)return;float w=d.getIntrinsicWidth(),h=d.getIntrinsicHeight();if(w<=0||h<=0)return;float factor=Math.min(getWidth()/w,getHeight()/h)*zoom;float maxX=Math.max(0,(w*factor-getWidth())/2f),maxY=Math.max(0,(h*factor-getHeight())/2f);dx=Math.max(-maxX,Math.min(maxX,dx));dy=Math.max(-maxY,Math.min(maxY,dy));image.reset();image.postScale(factor,factor);image.postTranslate((getWidth()-w*factor)/2f+dx,(getHeight()-h*factor)/2f+dy);setImageMatrix(image);}
    @Override public boolean onTouchEvent(MotionEvent e){getParent().requestDisallowInterceptTouchEvent(true);taps.onTouchEvent(e);scale.onTouchEvent(e);if(e.getActionMasked()==MotionEvent.ACTION_DOWN){lastX=e.getX();lastY=e.getY();}else if(e.getActionMasked()==MotionEvent.ACTION_POINTER_UP&&e.getPointerCount()>1){int index=e.getActionIndex()==0?1:0;lastX=e.getX(index);lastY=e.getY(index);}else if(e.getActionMasked()==MotionEvent.ACTION_MOVE){if(!scale.isInProgress()&&e.getPointerCount()==1&&zoom>1){dx+=e.getX()-lastX;dy+=e.getY()-lastY;position();}lastX=e.getX();lastY=e.getY();}return true;}
}
