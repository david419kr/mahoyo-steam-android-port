package org.mahoyo.port;

import android.app.Dialog;
import android.content.Context;
import android.graphics.PixelFormat;
import android.view.View;
import android.view.MotionEvent;
import android.view.Window;
import android.view.WindowManager;

/** Keep the real dialog visible while its original pointer stream stays in MainActivity. */
abstract class SwipeDialog extends Dialog {
    protected boolean pullPreview;
    private MenuSwipeDismiss dismissSwipe;
    SwipeDialog(Context context){super(context);}
    void preparePull(float x,float y){
        pullPreview=true;create();Window window=getWindow();window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE|WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);window.setDimAmount(0);window.setFormat(PixelFormat.TRANSLUCENT);window.setWindowAnimations(0);
        View decor=window.getDecorView();decor.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);decor.setTranslationX(x);decor.setTranslationY(y);
    }
    View pullView(){return getWindow().getDecorView();}
    void commitPull(){pullPreview=false;Window window=getWindow();window.setWindowAnimations(0);window.clearFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE|WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);View decor=window.getDecorView();decor.setTranslationX(0);decor.setTranslationY(0);decor.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);}
    void swipeDismiss(int action,java.util.function.BooleanSupplier edge){dismissSwipe=new MenuSwipeDismiss(pullView(),action,edge,()->{getWindow().setWindowAnimations(0);cancel();});}
    @Override public boolean dispatchTouchEvent(MotionEvent e){boolean claimed=dismissSwipe!=null&&dismissSwipe.claimed();if(dismissSwipe!=null&&dismissSwipe.touch(e,!pullPreview)){if(!claimed){MotionEvent cancel=MotionEvent.obtain(e);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();}return true;}return super.dispatchTouchEvent(e);}
    @Override public void dismiss(){if(dismissSwipe!=null)dismissSwipe.dispose();super.dismiss();}
}
