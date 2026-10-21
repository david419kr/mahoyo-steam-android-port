package org.mahoyo.port;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.*;
import android.graphics.drawable.*;
import android.os.Handler;
import android.os.Looper;
import android.util.LruCache;
import android.view.*;
import android.widget.*;
import org.json.JSONObject;
import java.io.File;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.*;

/** Opaque catalog: the game framebuffer never flashes through on entry/exit. */
final class ArchiveView extends LinearLayout {
    private final BiConsumer<String,String> command;
    private final Function<String,GameAsset> asset;
    private final ExecutorService decoder=Executors.newSingleThreadExecutor();
    private final Handler ui=new Handler(Looper.getMainLooper());
    private final LruCache<String,Bitmap> cache=new LruCache<String,Bitmap>(8*1024*1024){@Override protected int sizeOf(String key,Bitmap value){return value.getAllocationByteCount();}};
    private final ImageButton back;private final ScrollView scroll;private final LinearLayout content;
    private ArchiveCatalog model;private volatile long displayed;private int columns;private boolean exiting,busy;private volatile boolean closed;
    ArchiveView(Context context,Function<String,GameAsset> asset,BiConsumer<String,String> command){super(context);this.command=command;this.asset=asset;setOrientation(VERTICAL);setBackgroundColor(0xff0b1423);setClickable(true);setFocusable(true);setVisibility(GONE);
        LinearLayout top=new LinearLayout(context);top.setGravity(Gravity.CENTER_VERTICAL);top.setPadding(dp(12),0,dp(16),0);top.setBackgroundColor(0xe61a2a41);
        back=new ImageButton(context);back.setImageDrawable(new IconDrawable("back"));back.setImageTintList(ColorStateList.valueOf(0xffeaf1fc));back.setBackground(new RippleDrawable(ColorStateList.valueOf(0x308dbbe9),shape(0,24),null));back.setContentDescription(UiText.text(getContext(),"タイトルに戻る"));back.setOnClickListener(v->back());top.addView(back,new LinearLayout.LayoutParams(dp(48),dp(48)));
        TextView title=new TextView(context);title.setText(UiText.text(getContext(),"書庫"));title.setTextColor(0xffeaf1fc);title.setTextSize(20);title.setTypeface(Typeface.create("sans-serif-medium",0));top.addView(title,new LinearLayout.LayoutParams(0,-2,1));addView(top,new LinearLayout.LayoutParams(-1,dp(56)));
        scroll=new ScrollView(context);scroll.setFillViewport(true);content=new LinearLayout(context);content.setOrientation(VERTICAL);content.setPadding(dp(16),dp(12),dp(16),dp(24));scroll.addView(content,new ScrollView.LayoutParams(-1,-2));addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
    }
    boolean active(){return model!=null;}
    void state(JSONObject value){ArchiveCatalog next=ArchiveCatalog.parse(value);if(next==null){model=null;if(getVisibility()==VISIBLE)exiting=true;return;}exiting=false;boolean changed=model==null||model.generation!=next.generation;model=next;busy=false;back.setContentDescription(UiText.text(getContext(),model.backLabel()));back.setTooltipText(UiText.text(getContext(),model.backLabel()));setVisibility(VISIBLE);if(changed){displayed=0;scroll.scrollTo(0,0);rebuild();}}
    void presented(boolean archive){if(!archive&&exiting){exiting=false;setVisibility(GONE);content.removeAllViews();displayed=0;cache.evictAll();}}
    void back(){if(model!=null&&!busy){busy=true;command.accept("archiveBack",model.back());}}
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh){super.onSizeChanged(w,h,oldw,oldh);if(w!=oldw)post(this::rebuild);}
    static int columns(int width,float density){return Math.max(1,Math.min(7,(int)((width-32*density)/(208*density))));}
    private void rebuild(){if(model==null||getWidth()==0)return;int cols=columns(getWidth(),getResources().getDisplayMetrics().density);if(displayed==model.generation&&columns==cols)return;displayed=model.generation;columns=cols;content.removeAllViews();final long generation=displayed;View selected=null;
        if(model.entries.isEmpty()){TextView empty=label("まだ閲覧できる物語はありません",16);empty.setGravity(Gravity.CENTER);empty.setPadding(0,dp(60),0,0);content.addView(empty);return;}
        int group=-1,index=0,position=0;LinearLayout row=null;
        for(ArchiveCatalog.Entry entry:model.entries){if(group!=entry.group){group=entry.group;index=0;ImageView header=new ImageView(getContext());header.setScaleType(ImageView.ScaleType.FIT_START);header.setAdjustViewBounds(true);header.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);LinearLayout.LayoutParams hp=new LinearLayout.LayoutParams(dp(320),dp(54));hp.setMargins(dp(4),dp(12),0,dp(7));content.addView(header,hp);image(entry.header,header,generation,640,108);}
            if(index%cols==0){row=new LinearLayout(getContext());row.setOrientation(HORIZONTAL);content.addView(row,new LinearLayout.LayoutParams(-1,-2));}
            LinearLayout card=new LinearLayout(getContext());card.setOrientation(VERTICAL);card.setPadding(dp(6),dp(6),dp(6),dp(10));card.setClickable(true);card.setFocusable(true);GradientDrawable fill=shape(0xb32a405e,12);fill.setStroke(dp(entry.id==model.selected?2:1),entry.id==model.selected?0xffb4d4f4:0xff4a6381);card.setBackground(new RippleDrawable(ColorStateList.valueOf(0x408dbbe9),fill,null));LinearLayout.LayoutParams cp=new LinearLayout.LayoutParams(0,-2,1);cp.setMargins(dp(4),dp(4),dp(4),dp(4));row.addView(card,cp);
            ImageView preview=new ImageView(getContext());preview.setScaleType(ImageView.ScaleType.FIT_CENTER);preview.setBackgroundColor(0xff080f1c);card.addView(preview,new LinearLayout.LayoutParams(-1,dp(100)));image(entry.preview,preview,generation,400,200);
            TextView title=label((entry.read?"":"● ")+entry.label,14);title.setMinLines(2);title.setMaxLines(3);title.setPadding(dp(6),dp(8),dp(6),0);card.addView(title,new LinearLayout.LayoutParams(-1,-2));card.setContentDescription(entry.label+UiText.text(getContext(),entry.read?"　既読":"　未読"));card.setOnClickListener(v->{if(model!=null&&model.generation==generation&&!busy){busy=true;command.accept("archivePlay",model.play(entry.id));}});if(entry.id==model.selected)selected=card;
            index++;position++;boolean last=position==model.entries.size()||model.entries.get(position).group!=entry.group;if(last&&index%cols!=0)for(int pad=index%cols;pad<cols;pad++){View spacer=new View(getContext());LinearLayout.LayoutParams sp=new LinearLayout.LayoutParams(0,1,1);sp.setMargins(dp(4),0,dp(4),0);row.addView(spacer,sp);}}
        if(selected!=null){View target=selected;post(()->{if(model!=null&&model.generation==generation)scroll.smoothScrollTo(0,Math.max(0,((View)target.getParent()).getTop()-dp(64)));});}
    }
    private void image(String name,ImageView view,long generation,int width,int height){String key=name+":"+width+":"+height;Bitmap cached=cache.get(key);if(cached!=null){view.setImageBitmap(cached);return;}view.setTag(key);decoder.execute(()->{if(closed||displayed!=generation)return;Bitmap decoded=null;try{GameAsset file=asset.apply(name);BitmapFactory.Options bounds=new BitmapFactory.Options();bounds.inJustDecodeBounds=true;file.bitmap(bounds);if(bounds.outWidth>0&&bounds.outHeight>0&&bounds.outWidth<=32768&&bounds.outHeight<=32768){BitmapFactory.Options options=new BitmapFactory.Options();options.inSampleSize=1;while(bounds.outWidth/options.inSampleSize>width*2||bounds.outHeight/options.inSampleSize>height*2)options.inSampleSize*=2;decoded=file.bitmap(options);}}catch(Exception ignored){}Bitmap result=decoded;if(result!=null&&!closed){cache.put(key,result);ui.post(()->{if(!closed&&model!=null&&model.generation==generation&&key.equals(view.getTag()))view.setImageBitmap(result);});}});}
    private TextView label(String value,int size){TextView t=new TextView(getContext());t.setText(UiText.text(getContext(),value));t.setTextColor(0xffe5eefb);t.setTextSize(size);t.setIncludeFontPadding(false);return t;}
    private GradientDrawable shape(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    void close(){closed=true;decoder.shutdownNow();ui.removeCallbacksAndMessages(null);cache.evictAll();}
    private int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
}
