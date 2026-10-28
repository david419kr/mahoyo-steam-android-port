package org.mahoyo.port;

import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.widget.*;

final class SettingsPanel extends LinearLayout {
    private DisplayPrefs prefs;
    private MenuSwipeDismiss dismissSwipe;
    private LinearLayout content;
    private FrameLayout pages;
    private ScrollView scroll;
    private final int[] scrollPositions=new int[3];
    private Button displayTab,gameTab,helpTab;
    private int tab;private final Runnable close,reload;
    private final SettingsActivity.TabSwipeGesture swipe=new SettingsActivity.TabSwipeGesture();
    private static final int INK=0xffe5eefb, ACCENT=0xff759fc9;
    SettingsPanel(android.content.Context context,int initialTab,Runnable close,Runnable reload){
        super(context);this.close=close;this.reload=reload;prefs=new DisplayPrefs(context);tab=Math.max(0,Math.min(2,initialTab));
        LinearLayout root=this;root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(0xff0b1423);root.setClickable(true);
        LinearLayout header=new LinearLayout(getContext());header.setGravity(16);header.setPadding(dp(12),dp(2),dp(16),dp(2));root.addView(header);
        Button back=button("‹");back.setContentDescription(tr("뒤로"));header.addView(back,new LinearLayout.LayoutParams(dp(48),dp(48)));back.setOnClickListener(v->close.run());
        TextView title=label("설정",20);title.setTypeface(null,Typeface.BOLD);header.addView(title,new LinearLayout.LayoutParams(0,dp(48),1));title.setGravity(16);
        LinearLayout tabs=new LinearLayout(getContext());tabs.setPadding(dp(3),0,dp(3),0);tabs.setBackground(surface(0xff142236,0xff293c52,8));LinearLayout.LayoutParams tp=new LinearLayout.LayoutParams(-1,dp(44));tp.setMargins(dp(16),dp(2),dp(16),dp(4));root.addView(tabs,tp);
        displayTab=button("화면");gameTab=button("게임");tabs.addView(displayTab,new LinearLayout.LayoutParams(0,-1,1));tabs.addView(gameTab,new LinearLayout.LayoutParams(0,-1,1));displayTab.setOnClickListener(v->selectTab(0));gameTab.setOnClickListener(v->selectTab(1));helpTab=button("도움말");tabs.addView(helpTab,new LinearLayout.LayoutParams(0,-1,1));helpTab.setOnClickListener(v->selectTab(2));
        pages=new FrameLayout(getContext());root.addView(pages,new LinearLayout.LayoutParams(-1,0,1));createPage();
    }
    void swipeDismiss(Runnable done){dismissSwipe=new MenuSwipeDismiss(this,ReaderGesture.SETTINGS,()->scroll==null||!scroll.canScrollVertically(-1),done);}
    int currentTab(){return tab;}
    private void createPage(){
        scroll=new ScrollView(getContext());scroll.setFillViewport(false);pages.addView(scroll,new FrameLayout.LayoutParams(-1,-1));
        content=new LinearLayout(getContext());content.setOrientation(1);content.setPadding(dp(16),dp(8),dp(16),dp(16));scroll.addView(content,new ScrollView.LayoutParams(-1,-2));showTab();
        final ScrollView page=scroll;final int position=scrollPositions[tab];page.post(()->page.scrollTo(0,position));
    }
    private void selectTab(int target){
        if(target<0||target>2||target==tab)return;
        int direction=target>tab?1:-1;scrollPositions[tab]=scroll.getScrollY();final ScrollView outgoing=scroll;
        // Cancel an interrupted transition before starting another; keep only its current page.
        for(int i=pages.getChildCount()-1;i>=0;--i){View page=pages.getChildAt(i);page.animate().cancel();if(page!=outgoing)pages.removeViewAt(i);}
        outgoing.setTranslationX(0);outgoing.setAlpha(1);tab=target;createPage();
        float distance=direction*Math.max(dp(48),pages.getWidth()*.16f);scroll.setTranslationX(distance);scroll.setAlpha(0);
        scroll.animate().translationX(0).alpha(1).setDuration(200).setInterpolator(new DecelerateInterpolator()).start();
        outgoing.animate().translationX(-distance).alpha(0).setDuration(160).withEndAction(()->pages.removeView(outgoing)).start();
    }
    @Override protected void onDetachedFromWindow(){if(dismissSwipe!=null)dismissSwipe.dispose();if(pages!=null)for(int i=0;i<pages.getChildCount();++i)pages.getChildAt(i).animate().cancel();super.onDetachedFromWindow();}
    @Override public boolean dispatchTouchEvent(MotionEvent e){
        boolean claimed=dismissSwipe!=null&&dismissSwipe.claimed();
        if(dismissSwipe!=null&&dismissSwipe.touch(e,!hitsSeekBar(this,e.getRawX(),e.getRawY()))){if(!claimed){swipe.cancel();MotionEvent cancel=MotionEvent.obtain(e);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();}return true;}
        int action=e.getActionMasked();
        if(action==MotionEvent.ACTION_DOWN)swipe.start(e.getX(),e.getY(),hitsSeekBar(getRootView(),e.getRawX(),e.getRawY()));
        if(action==MotionEvent.ACTION_CANCEL||action==MotionEvent.ACTION_POINTER_DOWN)swipe.cancel();
        int target=action==MotionEvent.ACTION_UP?swipe.finish(e.getX(),e.getY(),getResources().getDisplayMetrics().density,tab,3):-1;
        if(target>=0){MotionEvent cancel=MotionEvent.obtain(e);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();selectTab(target);return true;}
        return super.dispatchTouchEvent(e);
    }
    private boolean hitsSeekBar(View view,float x,float y){
        Rect bounds=new Rect();if(view.getVisibility()!=View.VISIBLE||!view.getGlobalVisibleRect(bounds)||!bounds.contains((int)x,(int)y))return false;
        if(view instanceof SeekBar)return true;
        if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=group.getChildCount()-1;i>=0;i--)if(hitsSeekBar(group.getChildAt(i),x,y))return true;}
        return false;
    }

    private void showTab(){
        styleChoice(displayTab,tab==0,true);styleChoice(gameTab,tab==1,true);styleChoice(helpTab,tab==2,true);content.removeAllViews();
        if(tab==0){
            options("표시 모드","dynamic",new String[]{"원본 16:9","동적"},prefs.dynamic()?1:0,true);
            options("배경","bgCover",new String[]{"전체 표시","꽉 채움"},prefs.cover(false)?1:0,true);
            options("이벤트 CG","eventCover",new String[]{"전체 표시","꽉 채움"},prefs.cover(true)?1:0,true);
            typography("렌더링 해상도","renderPercent",50,100,prefs.renderPercent());
            TextView resolutionHint=label("게임 이미지의 가로·세로 해상도에 각각 적용됩니다. 50%는 픽셀 수가 25%입니다. 대사와 메뉴는 기기 해상도를 유지합니다.",13);resolutionHint.setTextColor(0xffa7b9d0);resolutionHint.setPadding(0,0,0,dp(10));content.addView(resolutionHint);
            options("여백","ambient",new String[]{"검정","이미지색"},prefs.ambient()?1:0,true);
            options("대화창","dialogue",new String[]{"원본 전체 화면","하단 분리","자동"},prefs.dialogueMode(),false);
            typography("전체화면 배경 농도","fullscreenDimPercent",0,100,prefs.fullscreenDimPercent());
            TextView dimHint=label("0%는 검정 배경을 끕니다. 글자와 장식은 유지됩니다.",13);dimHint.setTextColor(0xffa7b9d0);dimHint.setPadding(0,0,0,dp(10));content.addView(dimHint);
            typography("글자 크기","fontPercent",75,150,prefs.fontPercent());typography("좌우 여백","marginPercent",0,160,prefs.marginPercent());
            TextView hint=label("화면비가 바뀌어도 진행은 유지됩니다. 두 손가락 탭으로 메뉴를 표시하거나 숨길 수 있습니다.",14);hint.setTextColor(0xffa7b9d0);hint.setPadding(0,dp(18),0,dp(18));content.addView(hint);
        }else if(tab==1){
            language();textSpeed();autoDelay();
            volume("음악","bgm");volume("음성","voice");volume("효과음","se");updates();
            Button installation=button(getContext().getString(R.string.setup_manage));installation.setOnClickListener(v->getContext().startActivity(new android.content.Intent(getContext(),InstallActivity.class).putExtra("manage",true)));content.addView(installation,new LinearLayout.LayoutParams(-1,dp(48)));
            TextView hint=label("하단 분리와 자동 배치는 화면 탭에서 선택할 수 있습니다. 대화창을 위아래로 밀어 이전 문장을 읽고, 탭하여 진행합니다.",14);hint.setTextColor(0xffa7b9d0);hint.setPadding(0,dp(18),0,dp(18));content.addView(hint);
        }else{
            content.addView(ReaderHelpDialog.guide(getContext()));Button open=button("다음 실행시 도움말 다시 보기");open.setOnClickListener(v->{prefs.values.edit().putBoolean("hideReaderHelp",false).apply();Toast.makeText(getContext(),UiText.key(getContext(),"help_reset_done"),Toast.LENGTH_SHORT).show();});content.addView(open,new LinearLayout.LayoutParams(-1,dp(48)));
        }
    }
    private String tr(String text){return UiText.text(getContext(),text);}
    private void updates(){
        LinearLayout card=sliderCard();Switch automatic=new Switch(getContext());automatic.setText(UiText.key(getContext(),"update_automatic"));automatic.setTextColor(INK);automatic.setTextSize(14);automatic.setChecked(AppUpdates.enabled(getContext()));card.addView(automatic,new LinearLayout.LayoutParams(-1,dp(48)));automatic.setOnCheckedChangeListener((button,value)->AppUpdates.prefs(getContext()).edit().putBoolean("enabled",value).apply());
        TextView hint=label(UiText.key(getContext(),"update_automatic_hint"),13);hint.setTextColor(0xffa7b9d0);card.addView(hint);
        Button check=button(UiText.key(getContext(),"update_check"));check.setOnClickListener(v->AppUpdates.manual((android.app.Activity)getContext()));card.addView(check,new LinearLayout.LayoutParams(-1,dp(46)));
        Button reset=button(UiText.key(getContext(),"update_reset"));reset.setOnClickListener(v->{AppUpdates.reset(getContext());automatic.setChecked(true);Toast.makeText(getContext(),UiText.key(getContext(),"update_reset_done"),Toast.LENGTH_SHORT).show();});card.addView(reset,new LinearLayout.LayoutParams(-1,dp(46)));
    }
    private void language(){
        String[] values=LanguagePolicy.choices(GameLanguage.korean(getContext()));String selected=GameLanguage.preference(getContext());
        LinearLayout row=sliderCard();row.addView(label("언어",14));LinearLayout choices=new LinearLayout(getContext());row.addView(choices);
        String[] labels=LanguagePolicy.labels(GameLanguage.korean(getContext()));
        for(int index=0;index<values.length;++index){final String value=values[index];Button choice=button(labels[index]);choice.setTextColor(value.equals(selected)?Color.WHITE:INK);styleChoice(choice,value.equals(selected),false);
            LinearLayout.LayoutParams layout=new LinearLayout.LayoutParams(0,dp(44),1);layout.setMargins(dp(2),dp(4),dp(2),0);choices.addView(choice,layout);
            choice.setOnClickListener(v->{if(value.equals(selected))return;if(!GameLanguage.save(getContext(),value)){Toast.makeText(getContext(),UiText.key(getContext(),"language_save_failed"),Toast.LENGTH_LONG).show();return;}reload.run();});
        }
    }
    private LinearLayout sliderCard(){
        LinearLayout card=new LinearLayout(getContext());card.setOrientation(LinearLayout.VERTICAL);card.setPadding(dp(12),dp(8),dp(12),dp(6));card.setBackground(surface(0xff142236,0xff243549,8));
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.bottomMargin=dp(6);content.addView(card,p);return card;
    }
    private TextView sliderHeader(LinearLayout card,String title){
        LinearLayout header=new LinearLayout(getContext());header.setGravity(16);card.addView(header,new LinearLayout.LayoutParams(-1,dp(32)));header.addView(label(title,14),new LinearLayout.LayoutParams(0,-2,1));TextView value=label("",14);value.setTextColor(ACCENT);header.addView(value);return value;
    }
    private void typography(String title,String key,int minimum,int maximum,int selected){
        LinearLayout card=sliderCard();TextView value=sliderHeader(card,title);value.setText(selected+"%");SeekBar seek=new SeekBar(getContext());seek.setMax(maximum-minimum);seek.setProgress(selected-minimum);seek.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));seek.setContentDescription(tr(title));card.addView(seek,new LinearLayout.LayoutParams(-1,dp(44)));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){prefs.values.edit().putInt(key,s.getProgress()+minimum).apply();}public void onProgressChanged(SeekBar s,int n,boolean user){value.setText((n+minimum)+"%");}});
    }
    private void textSpeed(){
        LinearLayout card=sliderCard();TextView value=sliderHeader(card,"텍스트 속도");int speed=prefs.textSpeed();int selected=speed==0?Math.max(10,Math.min(100,prefs.values.getInt("textSpeedLast",40))):speed;
        SeekBar seek=new SeekBar(getContext());seek.setMax(90);seek.setProgress(selected-10);seek.setEnabled(speed!=0);seek.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));seek.setContentDescription(tr("텍스트 속도, 초당 글자 수"));card.addView(seek,new LinearLayout.LayoutParams(-1,dp(44)));
        Switch instant=new Switch(getContext());instant.setText(tr("즉시 표시"));instant.setTextSize(14);instant.setTextColor(INK);instant.setMinHeight(dp(44));instant.setChecked(speed==0);card.addView(instant,new LinearLayout.LayoutParams(-1,dp(44)));value.setText(speed==0?tr("즉시 표시"):UiText.format(getContext(),"characters_per_second",speed));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}public void onProgressChanged(SeekBar s,int n,boolean user){if(!user)return;int next=n+10;value.setText(UiText.format(getContext(),"characters_per_second",next));prefs.values.edit().putInt("textSpeed",next).apply();}});
        instant.setOnCheckedChangeListener((button,checked)->{int next=checked?0:seek.getProgress()+10;seek.setEnabled(!checked);value.setText(checked?tr("즉시 표시"):UiText.format(getContext(),"characters_per_second",next));prefs.values.edit().putInt("textSpeed",next).putInt("textSpeedLast",seek.getProgress()+10).apply();});
    }
    private void autoDelay(){
        LinearLayout card=sliderCard();TextView value=sliderHeader(card,"자동 진행 대기");int delay=prefs.autoDelay();value.setText(UiText.format(getContext(),"seconds",delay/1000.0));
        SeekBar seek=new SeekBar(getContext());seek.setMax(95);seek.setProgress((delay-500)/100);seek.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));seek.setContentDescription(tr("자동 진행 대기 시간"));card.addView(seek,new LinearLayout.LayoutParams(-1,dp(44)));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}public void onProgressChanged(SeekBar s,int n,boolean user){if(!user)return;int next=500+n*100;value.setText(UiText.format(getContext(),"seconds",next/1000.0));prefs.values.edit().putInt("autoDelay",next).apply();}});
    }
    private void options(String title,String key,String[] labels,int selected,boolean bool){
        LinearLayout row=new LinearLayout(getContext());row.setOrientation(1);row.setPadding(dp(12),dp(8),dp(12),dp(6));row.setBackground(surface(0xff142236,0xff243549,8));
        LinearLayout.LayoutParams rp=new LinearLayout.LayoutParams(-1,-2);rp.bottomMargin=dp(6);content.addView(row,rp);row.addView(label(title,14));
        LinearLayout choices=new LinearLayout(getContext());row.addView(choices);
        for(int i=0;i<labels.length;i++){final int index=i;Button b=button(labels[i]);styleChoice(b,i==selected,false);LinearLayout.LayoutParams bp=new LinearLayout.LayoutParams(0,dp(44),1);bp.setMargins(dp(2),dp(4),dp(2),0);choices.addView(b,bp);b.setOnClickListener(v->{if(bool)prefs.values.edit().putBoolean(key,index==1).apply();else prefs.values.edit().putInt(key,index).apply();showTab();});}
    }
    private void volume(String title,String channel){TextView caption=label(title,14);caption.setPadding(dp(12),dp(12),dp(12),dp(2));content.addView(caption);SeekBar seek=new SeekBar(getContext());seek.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));seek.setMax(100);seek.setProgress(prefs.volume(channel));content.addView(seek,new LinearLayout.LayoutParams(-1,dp(48)));seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}public void onProgressChanged(SeekBar s,int n,boolean user){if(user)prefs.values.edit().putInt("volume_"+channel,n).apply();}});}
    private GradientDrawable surface(int fill,int border,int radius){GradientDrawable shape=new GradientDrawable();shape.setColor(fill);shape.setCornerRadius(dp(radius));if(border!=0)shape.setStroke(dp(1),border);return shape;}
    private void styleChoice(Button button,boolean selected,boolean isTab){
        button.setSelected(selected);button.setTextColor(selected?Color.WHITE:0xffb7c7da);button.setTypeface(null,selected?Typeface.BOLD:Typeface.NORMAL);
        int fill=selected?0xff3d719d:(isTab?Color.TRANSPARENT:0xff1c3048),border=isTab?0:(selected?0xff83abd0:0xff344b64);
        android.graphics.drawable.Drawable shape=new android.graphics.drawable.InsetDrawable(surface(fill,border,6),0,dp(4),0,dp(4));
        button.setBackground(new android.graphics.drawable.RippleDrawable(android.content.res.ColorStateList.valueOf(0x408dbbe9),shape,null));
    }
    private Button button(String title){Button b=new Button(getContext());b.setStateListAnimator(null);b.setElevation(0);b.setBackgroundTintList(null);b.setText(tr(title));b.setTextSize(14);b.setAllCaps(false);b.setTextColor(INK);b.setMinWidth(0);b.setMinimumWidth(0);b.setMinHeight(0);b.setMinimumHeight(0);b.setIncludeFontPadding(false);b.setPadding(dp(8),0,dp(8),0);styleChoice(b,false,false);return b;}
    private TextView label(String text,int size){TextView v=new TextView(getContext());v.setText(tr(text));v.setTextColor(INK);v.setTextSize(size);return v;}
    private int dp(float v){return Math.round(v*getResources().getDisplayMetrics().density);}
}
