package org.mahoyo.port;

import android.view.MotionEvent;
import android.view.View;
import android.view.animation.DecelerateInterpolator;
import java.util.function.BooleanSupplier;

/** Shared interactive dismissal for the four reader screens. */
final class MenuSwipeDismiss {
    private final View view;private final int action;private final BooleanSupplier edge;private final Runnable close;
    private final MenuCloseGesture gesture=new MenuCloseGesture();private boolean animating;
    MenuSwipeDismiss(View view,int action,BooleanSupplier edge,Runnable close){this.view=view;this.action=action;this.edge=edge;this.close=close;}
    boolean claimed(){return gesture.claimed();}
    boolean touch(MotionEvent e,boolean enabled){
        if(animating)return true;
        int type=e.getActionMasked();
        if(type==MotionEvent.ACTION_DOWN)gesture.start(e.getRawX(),e.getRawY(),view.getResources().getDisplayMetrics().density,action,enabled);
        if(type==MotionEvent.ACTION_POINTER_DOWN||type==MotionEvent.ACTION_CANCEL){boolean claimed=gesture.claimed();gesture.cancel();if(claimed)finish(false);return claimed;}
        if(type==MotionEvent.ACTION_MOVE||type==MotionEvent.ACTION_UP){
            // ACTION_UP coordinates also carry the final movement, including a reversal.
            boolean claimed=gesture.claimed();
            if(type==MotionEvent.ACTION_MOVE||claimed)gesture.move(e.getRawX(),e.getRawY(),edge.getAsBoolean());
            if(gesture.claimed()){position(gesture.distance());if(type==MotionEvent.ACTION_UP){boolean commit=gesture.committed();gesture.cancel();finish(commit);}return true;}
            if(type==MotionEvent.ACTION_UP)gesture.cancel();
        }
        return false;
    }
    private boolean horizontal(){return action==ReaderGesture.SAVE||action==ReaderGesture.LOAD;}
    private float sign(){return action==ReaderGesture.SAVE||action==ReaderGesture.HISTORY?-1:1;}
    private void position(float distance){float extent=horizontal()?view.getWidth():view.getHeight();float offset=Math.min(extent,distance)*sign();if(horizontal())view.setTranslationX(offset);else view.setTranslationY(offset);}
    private void finish(boolean commit){animating=true;float target=commit?(horizontal()?view.getWidth():view.getHeight())*sign():0;view.animate().translationX(horizontal()?target:0).translationY(horizontal()?0:target).setDuration(commit?180:150).setInterpolator(new DecelerateInterpolator()).withEndAction(()->{animating=false;if(commit)close.run();}).start();}
    void dispose(){view.animate().cancel();gesture.cancel();animating=false;}
}
