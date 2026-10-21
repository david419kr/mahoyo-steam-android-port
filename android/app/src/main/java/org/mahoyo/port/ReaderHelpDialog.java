package org.mahoyo.port;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.*;
import android.widget.*;

/** A modal guide leaves the game renderer and playback running behind it. */
final class ReaderHelpDialog extends Dialog {
    ReaderHelpDialog(Context context){super(context,R.style.ReaderHelpTheme);}
    @Override public void onCreate(Bundle saved){super.onCreate(saved);requestWindowFeature(Window.FEATURE_NO_TITLE);UiText.refresh(getContext());setTitle(UiText.key(getContext(),"help"));
        Window window=getWindow();WindowManager.LayoutParams attrs=window.getAttributes();attrs.layoutInDisplayCutoutMode=android.os.Build.VERSION.SDK_INT>=30?WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;window.setAttributes(attrs);if(android.os.Build.VERSION.SDK_INT>=30)window.setDecorFitsSystemWindows(false);window.getDecorView().setSystemUiVisibility(5894|View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        FrameLayout root=new FrameLayout(getContext());root.setBackgroundColor(Color.TRANSPARENT);setContentView(root);
        LinearLayout panel=new LinearLayout(getContext());panel.setOrientation(LinearLayout.VERTICAL);panel.setPadding(dp(getContext(),16),dp(getContext(),12),dp(getContext(),16),dp(getContext(),8));GradientDrawable background=new GradientDrawable();background.setColor(0xcc112033);background.setCornerRadius(dp(getContext(),10));background.setStroke(dp(getContext(),1),0x6686a9c9);panel.setBackground(background);int[] initial=popupSize(getContext().getResources().getDisplayMetrics().widthPixels,getContext().getResources().getDisplayMetrics().heightPixels,getContext().getResources().getDisplayMetrics().density);root.addView(panel,new FrameLayout.LayoutParams(initial[0],initial[1],Gravity.CENTER));root.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->{int[] size=popupSize(r-l,b-t,getContext().getResources().getDisplayMetrics().density);FrameLayout.LayoutParams layout=(FrameLayout.LayoutParams)panel.getLayoutParams();if(layout.width!=size[0]||layout.height!=size[1]){layout.width=size[0];layout.height=size[1];panel.setLayoutParams(layout);}});
        TextView title=text(getContext(),"help_title",20);title.setTypeface(null,Typeface.BOLD);panel.addView(title,new LinearLayout.LayoutParams(-1,dp(getContext(),40)));
        ScrollView scroll=new ScrollView(getContext());scroll.addView(guide(getContext()));panel.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout buttons=new LinearLayout(getContext());Button close=button("close"),hide=button("help_hide");close.setOnClickListener(v->dismiss());hide.setOnClickListener(v->{new DisplayPrefs(getContext()).values.edit().putBoolean("hideReaderHelp",true).apply();dismiss();});buttons.addView(close,new LinearLayout.LayoutParams(0,dp(getContext(),48),1));buttons.addView(hide,new LinearLayout.LayoutParams(0,dp(getContext(),48),1));panel.addView(buttons);
    }
    @Override public void show(){super.show();Window window=getWindow();window.setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);window.setLayout(-1,-1);}
    static int[] popupSize(int width,int height,float density){return new int[]{Math.max(1,Math.min(Math.round(width*.84f),Math.round(760*density))),Math.max(1,Math.min(Math.round(height*.82f),Math.round(640*density)))};}
    static LinearLayout guide(Context context){LinearLayout body=new LinearLayout(context);body.setOrientation(LinearLayout.VERTICAL);
        String[] keys={"help_two_fingers","help_down","help_up","help_right","help_left","help_hold","help_scroll"};
        for(String key:keys){TextView row=text(context,key,key.equals("help_scroll")?13:15);row.setPadding(dp(context,4),dp(context,10),dp(context,4),dp(context,10));body.addView(row,new LinearLayout.LayoutParams(-1,-2));}return body;
    }
    private Button button(String key){Button button=new Button(getContext());button.setText(UiText.key(getContext(),key));button.setTextColor(Color.WHITE);button.setTextSize(14);button.setAllCaps(false);button.setMinWidth(0);button.setMinimumWidth(0);button.setStateListAnimator(null);button.setBackgroundTintList(null);GradientDrawable fill=new GradientDrawable();fill.setColor(key.equals("close")?0xff213a55:0xff3d719d);fill.setCornerRadius(dp(getContext(),6));button.setBackground(new android.graphics.drawable.InsetDrawable(fill,dp(getContext(),3),dp(getContext(),4),dp(getContext(),3),dp(getContext(),4)));return button;}
    private static TextView text(Context context,String key,int sp){TextView view=new TextView(context);view.setText(UiText.key(context,key));view.setTextColor(0xffe5eefb);view.setTextSize(sp);return view;}
    private static int dp(Context context,int value){return Math.round(value*context.getResources().getDisplayMetrics().density);}
}
