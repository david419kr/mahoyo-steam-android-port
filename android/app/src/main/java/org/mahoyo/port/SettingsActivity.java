package org.mahoyo.port;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.OrientationEventListener;
import android.view.WindowManager;

public final class SettingsActivity extends Activity {
    private SettingsPanel panel;
    private OrientationEventListener orientation;
    private SettingsRotation rotation;
    @Override protected void attachBaseContext(android.content.Context context){super.attachBaseContext(GameLanguage.localized(context));}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);UiText.refresh(this);setTitle(UiText.key(this,"settings"));setRequestedOrientation(state==null?ActivityInfo.SCREEN_ORIENTATION_LOCKED:state.getInt("orientation",ActivityInfo.SCREEN_ORIENTATION_LOCKED));
        android.view.Display entryDisplay=getWindowManager().getDefaultDisplay();android.util.DisplayMetrics metrics=new android.util.DisplayMetrics();entryDisplay.getRealMetrics(metrics);rotation=new SettingsRotation((entryDisplay.getRotation()%2==0)==(metrics.widthPixels>metrics.heightPixels));
        orientation=new OrientationEventListener(this){@Override public void onOrientationChanged(int degrees){int requested=rotation.sample(degrees,SystemClock.uptimeMillis());if(requested>=0)setRequestedOrientation(requested);}};
        WindowManager.LayoutParams attrs=getWindow().getAttributes();android.view.Display display=getWindowManager().getDefaultDisplay();float rate=0;
        for(android.view.Display.Mode mode:display.getSupportedModes())if(mode.getPhysicalWidth()==display.getMode().getPhysicalWidth()&&mode.getPhysicalHeight()==display.getMode().getPhysicalHeight()&&mode.getRefreshRate()>rate){rate=mode.getRefreshRate();attrs.preferredDisplayModeId=mode.getModeId();}getWindow().setAttributes(attrs);
        getWindow().getDecorView().setSystemUiVisibility(5894);if(android.os.Build.VERSION.SDK_INT>=30)getWindow().setDecorFitsSystemWindows(false);
        panel=new SettingsPanel(this,state==null?0:state.getInt("tab"),this::finish,this::recreate);setContentView(panel);panel.swipeDismiss(()->{finish();overridePendingTransition(0,0);});
    }
    @Override protected void onResume(){super.onResume();if(orientation.canDetectOrientation())orientation.enable();}
    @Override protected void onPause(){orientation.disable();super.onPause();}
    @Override public void onSaveInstanceState(Bundle state){state.putInt("tab",panel.currentTab());state.putInt("orientation",getRequestedOrientation());super.onSaveInstanceState(state);}
    @Override public void finish(){super.finish();overridePendingTransition(R.anim.settings_stay,R.anim.settings_exit_bottom);}
    static final class TabSwipeGesture {
        private float startX,startY;
        private boolean active,slider;
        void start(float x,float y,boolean onSlider){startX=x;startY=y;slider=onSlider;active=true;}
        void cancel(){active=false;slider=false;}
        int finish(float x,float y,float density,int current,int count){int direction=finish(x,y,density);return direction<0?-1:Math.max(0,Math.min(count-1,current+(direction==1?1:-1)));}
        int finish(float x,float y,float density){
            boolean eligible=active&&!slider;cancel();float dx=x-startX,dy=y-startY;
            return eligible&&Math.abs(dx)>90*density&&Math.abs(dy)<65*density?(dx<0?1:0):-1;
        }
    }
}
