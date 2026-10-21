package org.mahoyo.port;

import android.app.Dialog;
import android.content.*;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Bundle;
import android.view.*;
import android.widget.*;
import java.text.DateFormat;
import java.util.*;
import java.util.function.BiConsumer;

/** A fixed header and a dense, responsive grid. No storage or bitmap decode on UI. */
final class SlotGridDialog extends SwipeDialog {
    private final SlotThumbnailStore store;private boolean saving,canSave;private BiConsumer<Boolean,SlotThumbnailStore.Entry> selected;
    private Button saveTab,loadTab;private final SettingsActivity.TabSwipeGesture swipe=new SettingsActivity.TabSwipeGesture();
    private List<SlotThumbnailStore.Entry> entries=Collections.emptyList();private GridView grid;private TextView subtitle;private boolean dead;
    SlotGridDialog(Context context,SlotThumbnailStore store,boolean saving,boolean canSave,BiConsumer<Boolean,SlotThumbnailStore.Entry> selected){super(context);this.store=store;this.saving=saving;this.canSave=canSave;this.selected=selected;}
    static int columns(float widthDp){return Math.max(1,Math.min(8,(int)Math.floor(Math.max(1,widthDp-24)/160)));}
    void selection(boolean available,BiConsumer<Boolean,SlotThumbnailStore.Entry> listener){canSave=available;selected=listener;if(saveTab!=null)updateTabs();}
    @Override protected void onCreate(Bundle state){super.onCreate(state);requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout body=new LinearLayout(getContext());body.setOrientation(LinearLayout.VERTICAL);body.setBackgroundColor(0xff0b1423);
        LinearLayout header=new LinearLayout(getContext());header.setGravity(Gravity.CENTER_VERTICAL);header.setPadding(dp(20),dp(6),dp(8),dp(6));LinearLayout titles=new LinearLayout(getContext());titles.setOrientation(LinearLayout.VERTICAL);LinearLayout tabs=new LinearLayout(getContext());saveTab=tab("セーブ",true);loadTab=tab("ロード",false);tabs.addView(saveTab,new LinearLayout.LayoutParams(0,dp(44),1));tabs.addView(loadTab,new LinearLayout.LayoutParams(0,dp(44),1));titles.addView(tabs,new LinearLayout.LayoutParams(-1,dp(44)));subtitle=label("保存データを読み込み中…",12,0xffa5b8d1);titles.addView(subtitle);header.addView(titles,new LinearLayout.LayoutParams(0,-2,1));TextView close=label("×",30,0xffeaf1fc);close.setGravity(Gravity.CENTER);close.setContentDescription(UiText.text(getContext(),"閉じる"));close.setBackground(shape(0xb32c4668,14));close.setClickable(true);close.setOnClickListener(v->cancel());header.addView(close,new LinearLayout.LayoutParams(dp(48),dp(48)));body.addView(header,new LinearLayout.LayoutParams(-1,dp(76)));
        grid=new GridView(getContext());grid.setPadding(dp(12),dp(6),dp(12),dp(18));grid.setClipToPadding(false);grid.setHorizontalSpacing(dp(8));grid.setVerticalSpacing(dp(8));grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);grid.setNumColumns(columns(getContext().getResources().getDisplayMetrics().widthPixels/getContext().getResources().getDisplayMetrics().density));grid.setAdapter(adapter);grid.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->{if(r-l!=or-ol)grid.setNumColumns(columns((r-l)/getContext().getResources().getDisplayMetrics().density));});grid.setOnItemClickListener((parent,view,index,id)->{if(index<entries.size()){SlotThumbnailStore.Entry entry=entries.get(index);if(saving||entry.exists)selected.accept(saving,entry);}});body.addView(grid,new LinearLayout.LayoutParams(-1,0,1));setContentView(body);updateTabs();
        store.list(values->{if(dead||!isShowing()&&!pullPreview){SlotThumbnailStore.recycle(values);return;}entries=values;long used=values.stream().filter(v->v.exists).count();subtitle.setText(UiText.format(getContext(),"saves_count",used,SlotThumbnailStore.COUNT));adapter.notifyDataSetChanged();});
    }
    private Button tab(String caption,boolean save){Button button=new Button(getContext());button.setText(UiText.text(getContext(),caption));button.setTextSize(15);button.setAllCaps(false);button.setMinWidth(0);button.setMinimumWidth(0);button.setMinHeight(0);button.setMinimumHeight(0);button.setPadding(dp(8),0,dp(8),0);button.setStateListAnimator(null);button.setBackgroundTintList(null);button.setOnClickListener(v->selectMode(save));return button;}
    private void updateTabs(){for(Button button:new Button[]{saveTab,loadTab}){boolean active=button==saveTab?saving:!saving;button.setSelected(active);button.setTextColor(active?Color.WHITE:0xffb7c7da);button.setTypeface(null,active?Typeface.BOLD:Typeface.NORMAL);button.setBackground(new android.graphics.drawable.InsetDrawable(shape(active?0xff3d719d:0xff17283c,6),dp(2),dp(4),dp(2),dp(4)));}saveTab.setEnabled(canSave);saveTab.setAlpha(canSave?1:.4f);}
    private void selectMode(boolean save){if(save==saving||save&&!canSave)return;saving=save;updateTabs();adapter.notifyDataSetChanged();grid.animate().cancel();grid.setAlpha(.6f);grid.setTranslationX(dp(save?-24:24));grid.animate().alpha(1).translationX(0).setDuration(180).start();}
    @Override public boolean dispatchTouchEvent(MotionEvent event){int action=event.getActionMasked();if(action==MotionEvent.ACTION_DOWN)swipe.start(event.getX(),event.getY(),false);if(action==MotionEvent.ACTION_CANCEL||action==MotionEvent.ACTION_POINTER_DOWN)swipe.cancel();int target=action==MotionEvent.ACTION_UP?swipe.finish(event.getX(),event.getY(),getContext().getResources().getDisplayMetrics().density):-1;if(target>=0){MotionEvent cancel=MotionEvent.obtain(event);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();selectMode(target==0);return true;}return super.dispatchTouchEvent(event);}
    @Override public void show(){super.show();Window window=getWindow();if(window!=null){window.setBackgroundDrawable(new ColorDrawable(pullPreview?Color.TRANSPARENT:0xff0b1423));WindowManager.LayoutParams attrs=window.getAttributes();attrs.layoutInDisplayCutoutMode=Build.VERSION.SDK_INT>=30?WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;window.setAttributes(attrs);if(Build.VERSION.SDK_INT>=30)window.setDecorFitsSystemWindows(false);window.setLayout(-1,-1);window.setStatusBarColor(0xff0b1423);window.setNavigationBarColor(0xff0b1423);window.getDecorView().setSystemUiVisibility(5894|View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);}}
    @Override public void dismiss(){dead=true;super.dismiss();if(grid!=null)grid.setAdapter(null);entries=Collections.emptyList();} // HWUI may still reference attached images; release references rather than recycling its draw buffers.
    private final BaseAdapter adapter=new BaseAdapter(){
        @Override public int getCount(){return entries.size();}@Override public Object getItem(int index){return entries.get(index);}@Override public long getItemId(int index){return index;}
        @Override public boolean isEnabled(int index){return saving||entries.get(index).exists;}
        @Override public boolean areAllItemsEnabled(){return false;}
        @Override public View getView(int index,View previous,ViewGroup parent){
            SlotThumbnailStore.Entry entry=entries.get(index);LinearLayout card=new LinearLayout(getContext());card.setOrientation(LinearLayout.VERTICAL);card.setPadding(dp(10),dp(9),dp(10),dp(10));card.setBackground(shape(entry.exists?0xb32a405e:0x9920324c,12));card.setAlpha(saving||entry.exists?1:.5f);TextView number=label(String.format(Locale.ROOT,"%02d",entry.slot),14,0xffb4d4f4);number.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));int numberHeight=(int)Math.ceil(number.getTextSize()*1.5f);card.addView(number,new LinearLayout.LayoutParams(-1,numberHeight));
            // GridView rows must have one height, including cells without a thumbnail.
            ImageView image=new ImageView(getContext());image.setScaleType(ImageView.ScaleType.FIT_CENTER);image.setBackgroundColor(0xff080f1c);image.setImageBitmap(entry.thumbnail);image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);LinearLayout.LayoutParams imageLayout=new LinearLayout.LayoutParams(-1,dp(92));imageLayout.topMargin=dp(6);imageLayout.bottomMargin=dp(7);card.addView(image,imageLayout);
            TextView date=label(entry.exists?DateFormat.getDateTimeInstance(DateFormat.SHORT,DateFormat.SHORT,Locale.forLanguageTag(UiText.language())).format(new Date(entry.modified)):"未保存",11,0xffa5b8d1);date.setSingleLine(true);date.setEllipsize(android.text.TextUtils.TruncateAt.END);int dateHeight=(int)Math.ceil(date.getTextSize()*1.5f);card.addView(date,new LinearLayout.LayoutParams(-1,dateHeight));TextView excerpt=label(entry.exists?(entry.text.isEmpty()?"保存データ":entry.text):saving?"ここに保存":"",13,entry.exists?0xffeaf1fc:0xffa5b8d1);excerpt.setMaxLines(3);excerpt.setMinLines(3);excerpt.setEllipsize(android.text.TextUtils.TruncateAt.END);excerpt.setPadding(0,dp(5),0,0);int lineHeight=(int)Math.ceil(excerpt.getTextSize()*1.5f);excerpt.setLineHeight(lineHeight);int excerptHeight=lineHeight*3+excerpt.getPaddingTop();card.addView(excerpt,new LinearLayout.LayoutParams(-1,excerptHeight));card.setLayoutParams(new AbsListView.LayoutParams(-1,card.getPaddingTop()+card.getPaddingBottom()+imageLayout.height+imageLayout.topMargin+imageLayout.bottomMargin+numberHeight+dateHeight+excerptHeight));card.setContentDescription(UiText.format(getContext(),"slot_number",entry.slot)+" · "+date.getText()+"、"+excerpt.getText());return card;
        }
    };
    private TextView label(String value,int sp,int color){TextView text=new TextView(getContext());text.setText(UiText.text(getContext(),value));text.setTextSize(sp);text.setTextColor(color);text.setIncludeFontPadding(false);return text;}
    private GradientDrawable shape(int color,int radius){GradientDrawable shape=new GradientDrawable();shape.setColor(color);shape.setCornerRadius(dp(radius));return shape;}
    private int dp(float value){return Math.round(value*getContext().getResources().getDisplayMetrics().density);}
}
