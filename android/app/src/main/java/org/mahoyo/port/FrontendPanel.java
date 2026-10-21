package org.mahoyo.port;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;
import java.io.File;
import java.util.function.*;

abstract class FrontendPanel extends LinearLayout {
    final LinearLayout toolbar;final FrameLayout body;final ImageButton backButton;final TextView heading;
    final FrontendImages images;private boolean leaving;
    FrontendPanel(Context context,String title,Function<String,GameAsset> files,Consumer<String> errors){super(context);images=new FrontendImages(files,errors);setOrientation(VERTICAL);setClickable(true);setBackgroundColor(0xff0b1423);setVisibility(GONE);toolbar=new LinearLayout(context);toolbar.setGravity(Gravity.CENTER_VERTICAL);toolbar.setPadding(dp(8),0,dp(8),0);toolbar.setBackgroundColor(0xe61a2a41);backButton=button("back","タイトルに戻る",this::back);toolbar.addView(backButton,new LinearLayout.LayoutParams(dp(48),dp(48)));heading=new TextView(context);heading.setText(UiText.text(getContext(),title));heading.setTextColor(0xffeaf1fc);heading.setTextSize(19);heading.setTypeface(Typeface.create("sans-serif-medium",0));toolbar.addView(heading,new LinearLayout.LayoutParams(0,-2,1));addView(toolbar,new LinearLayout.LayoutParams(-1,dp(56)));body=new FrameLayout(context);addView(body,new LinearLayout.LayoutParams(-1,0,1));}
    final ImageButton button(String icon,String label,Runnable clicked){ImageButton b=new ImageButton(getContext());b.setImageDrawable(new IconDrawable(icon));b.setImageTintList(ColorStateList.valueOf(0xffdceaff));b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x358dbbe9),shape(0,16),null));b.setPadding(dp(12),dp(12),dp(12),dp(12));b.setContentDescription(UiText.text(getContext(),label));b.setTooltipText(UiText.text(getContext(),label));b.setOnClickListener(v->clicked.run());return b;}
    final void shown(boolean active){if(active){leaving=false;setVisibility(VISIBLE);}else if(getVisibility()==VISIBLE)leaving=true;}
    final void presented(boolean current){if(!current&&leaving){leaving=false;setVisibility(GONE);images.invalidate();release();}}
    abstract boolean active();abstract void back();abstract void release();abstract void resized();
    @Override protected void onSizeChanged(int w,int h,int ow,int oh){super.onSizeChanged(w,h,ow,oh);if(w!=ow||h!=oh)post(this::resized);}
    final GradientDrawable shape(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    final int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
    void close(){images.close();}
}
