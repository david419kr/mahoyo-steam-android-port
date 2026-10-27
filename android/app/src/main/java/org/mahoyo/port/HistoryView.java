package org.mahoyo.port;

import android.app.Dialog;
import android.content.*;
import android.content.res.ColorStateList;
import android.graphics.*;
import android.graphics.drawable.*;
import android.os.Bundle;
import android.text.TextPaint;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** Virtualized Native groups, with independently cached native-resolution glyph layouts. */
final class HistoryView extends SwipeDialog {
    private LinearLayout contentRoot;private int sourceWidth,sourceHeight;private float safeLeft,safeTop,safeRight=1,safeBottom=1;
    HistoryModel model;private final Consumer<HistoryModel.Group> selected,rewind;private final Runnable stop;
    private final LayoutCache layouts=new LayoutCache();private ListView list;private String playing="";private boolean pending,rewinding;private FrameLayout restoreOverlay;
    HistoryView(Context context,HistoryModel model,Consumer<HistoryModel.Group> selected,Runnable stop,Consumer<HistoryModel.Group> rewind){super(context);this.model=model;this.selected=selected;this.stop=stop;this.rewind=rewind;}
    @Override protected void onCreate(Bundle saved){super.onCreate(saved);getWindow().setWindowAnimations(pullPreview?0:R.style.HistoryMotion);requestWindowFeature(Window.FEATURE_NO_TITLE);contentRoot=new LinearLayout(getContext());LinearLayout root=contentRoot;root.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->applySafeArea());root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(0xb30b1423);
        LinearLayout header=new LinearLayout(getContext());header.setGravity(Gravity.CENTER_VERTICAL);header.setPadding(dp(12),dp(6),dp(12),dp(6));ImageButton back=button("back","履歴を閉じる");back.setOnClickListener(v->cancel());header.addView(back,new LinearLayout.LayoutParams(dp(44),dp(44)));TextView title=new TextView(getContext());title.setText(UiText.text(getContext(),"履歴"));title.setTextColor(0xffeaf1fc);title.setTextSize(20);title.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));title.setPadding(dp(14),0,0,0);header.addView(title,new LinearLayout.LayoutParams(0,-2,1));ImageButton close=button("close","閉じる");close.setOnClickListener(v->cancel());header.addView(close,new LinearLayout.LayoutParams(dp(44),dp(44)));root.addView(header,new LinearLayout.LayoutParams(-1,dp(60)));
        FrameLayout body=new FrameLayout(getContext());list=new ListView(getContext());list.setBackgroundColor(Color.TRANSPARENT);list.setDivider(null);list.setItemsCanFocus(true);list.setSelector(new ColorDrawable(Color.TRANSPARENT));list.setStackFromBottom(true);list.setPadding(dp(12),dp(4),dp(12),dp(20));list.setClipToPadding(false);list.setCacheColorHint(Color.TRANSPARENT);list.setAdapter(adapter);body.addView(list,new FrameLayout.LayoutParams(-1,-1));TextView empty=new TextView(getContext());empty.setText(UiText.text(getContext(),"履歴はまだありません"));empty.setTextSize(18);empty.setTextColor(0xffafc2dc);empty.setGravity(Gravity.CENTER);body.addView(empty,new FrameLayout.LayoutParams(-1,-1));list.setEmptyView(empty);root.addView(body,new LinearLayout.LayoutParams(-1,0,1));
        FrameLayout windowRoot=new FrameLayout(getContext());windowRoot.addView(root,new FrameLayout.LayoutParams(-1,-1));restoreOverlay=new FrameLayout(getContext());restoreOverlay.setBackgroundColor(0xcc000000);restoreOverlay.setClickable(true);restoreOverlay.setVisibility(View.GONE);windowRoot.addView(restoreOverlay,new FrameLayout.LayoutParams(-1,-1));
        LinearLayout loading=new LinearLayout(getContext());loading.setOrientation(LinearLayout.VERTICAL);loading.setGravity(Gravity.CENTER);loading.setPadding(dp(24),dp(24),dp(24),dp(24));restoreOverlay.addView(loading,new FrameLayout.LayoutParams(-1,-2,Gravity.CENTER));
        ProgressBar spinner=new ProgressBar(getContext());spinner.setIndeterminate(true);spinner.setIndeterminateTintList(ColorStateList.valueOf(0xffa8c8ec));loading.addView(spinner,new LinearLayout.LayoutParams(dp(56),dp(56)));
        TextView restoreStatus=new TextView(getContext());restoreStatus.setText(UiText.key(getContext(),"history_rewind_busy"));restoreStatus.setTextColor(0xffeaf1fc);restoreStatus.setTextSize(22);restoreStatus.setGravity(Gravity.CENTER);restoreStatus.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));restoreStatus.setPadding(0,dp(18),0,0);restoreStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);loading.addView(restoreStatus,new LinearLayout.LayoutParams(-1,-2));
        setContentView(windowRoot);applySafeArea();swipeDismiss(ReaderGesture.HISTORY,()->list==null||!list.canScrollVertically(1));
        if(!model.groups.isEmpty())list.post(()->list.setSelection(model.groups.size()-1));
    }
    void contentArea(RectF area,int width,int height){if(area==null||width<=0||height<=0)return;sourceWidth=width;sourceHeight=height;safeLeft=area.left/width;safeTop=area.top/height;safeRight=area.right/width;safeBottom=area.bottom/height;applySafeArea();}
    private void applySafeArea(){if(contentRoot==null)return;int w=contentRoot.getWidth()>0?contentRoot.getWidth():sourceWidth,h=contentRoot.getHeight()>0?contentRoot.getHeight():sourceHeight;contentRoot.setPadding(Math.round(w*safeLeft),Math.round(h*safeTop),Math.round(w*(1-safeRight)),Math.round(h*(1-safeBottom)));}
    @Override public void show(){super.show();Window window=getWindow();if(window!=null){window.setWindowAnimations(pullPreview?0:R.style.HistoryMotion);window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);window.setDimAmount(0);android.view.WindowManager.LayoutParams attrs=window.getAttributes();attrs.layoutInDisplayCutoutMode=android.os.Build.VERSION.SDK_INT>=30?android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;window.setAttributes(attrs);if(android.os.Build.VERSION.SDK_INT>=30)window.setDecorFitsSystemWindows(false);window.setStatusBarColor(0xff0b1423);window.setNavigationBarColor(0xff0b1423);window.setLayout(-1,-1);window.getDecorView().setSystemUiVisibility(5894|View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);}}
    void rewinding(boolean value){rewinding=value;setCancelable(!value);pending(value);if(restoreOverlay!=null)restoreOverlay.setVisibility(value?View.VISIBLE:View.GONE);if(contentRoot!=null)contentRoot.setImportantForAccessibility(value?View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS:View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);}
    @Override public boolean dispatchTouchEvent(MotionEvent event){return rewinding||super.dispatchTouchEvent(event);}
    void pending(boolean value){if(pending==value)return;pending=value;adapter.notifyDataSetChanged();}
    void replaceModel(HistoryModel value){model=value;playing="";pending=false;layouts.clear();adapter.notifyDataSetChanged();if(list!=null&&!model.groups.isEmpty())list.setSelection(model.groups.size()-1);}
    void replay(JSONObject value){HistoryModel.Group active=model.playingGroup(value);String next=active==null?"":active.key;if(next.equals(playing))return;playing=next;adapter.notifyDataSetChanged();}
    @Override public void dismiss(){super.dismiss();if(list!=null)list.setAdapter(null);layouts.clear();}
    private final BaseAdapter adapter=new BaseAdapter(){
        @Override public int getCount(){return model.groups.size();}@Override public Object getItem(int i){return model.groups.get(i);}@Override public long getItemId(int i){return model.groups.get(i).start;}
        @Override public View getView(int index,View old,ViewGroup parent){GroupView cell=old instanceof GroupView?(GroupView)old:new GroupView();cell.bind(model.groups.get(index));return cell;}
    };
    private final class GroupView extends LinearLayout {
        final MessageView message;final ImageButton playback,rewindButton;final TextView note;HistoryModel.Group group;
        GroupView(){super(HistoryView.this.getContext());setOrientation(HORIZONTAL);setGravity(Gravity.TOP);setPadding(dp(14),dp(14),dp(10),dp(14));LinearLayout content=new LinearLayout(getContext());content.setOrientation(VERTICAL);message=new MessageView(getContext(),layouts);content.addView(message,new LinearLayout.LayoutParams(-1,-2));note=new TextView(getContext());note.setTextSize(11);note.setTextColor(0xffa8bdd7);note.setPadding(0,dp(7),0,0);content.addView(note);addView(content,new LinearLayout.LayoutParams(0,-2,1));playback=button("play","音声を再生");LinearLayout actions=new LinearLayout(getContext());actions.setOrientation(VERTICAL);actions.addView(playback,new LinearLayout.LayoutParams(dp(44),dp(44)));rewindButton=button("rewind",UiText.key(getContext(),"history_rewind"));LinearLayout.LayoutParams rewindLayout=new LinearLayout.LayoutParams(dp(44),dp(44));rewindLayout.topMargin=dp(8);actions.addView(rewindButton,rewindLayout);addView(actions,new LinearLayout.LayoutParams(dp(54),-2));rewindButton.setOnClickListener(v->{if(group!=null&&!pending&&group.rewindable)rewind.accept(group);});playback.setOnClickListener(v->{if(group==null||pending)return;if(group.key.equals(playing)){stop.run();return;}if(group.playable){pending(true);selected.accept(group);}});}
        void bind(HistoryModel.Group value){group=value;boolean active=value.key.equals(playing);GradientDrawable background=new GradientDrawable();background.setColor(active?0x442d5178:Color.TRANSPARENT);background.setCornerRadius(0);if(active)background.setStroke(dp(1),0xff88b1da);setBackground(background);setLayoutParams(new AbsListView.LayoutParams(-1,-2));message.group(value);rewindButton.setEnabled(!pending&&value.rewindable);rewindButton.setAlpha(value.rewindable?1:.4f);rewindButton.setTooltipText(UiText.key(getContext(),value.rewindable?"history_rewind":"history_rewind_unavailable"));playback.setVisibility(value.hasVoice?VISIBLE:GONE);playback.setEnabled(!pending&&(active||value.playable));playback.setAlpha(active||value.playable?1:.4f);playback.setImageDrawable(new Symbol(active?"stop":"play"));String label=active?"音声を停止":value.playable?"音声を再生":value.reason;playback.setContentDescription(UiText.text(getContext(),label));playback.setTooltipText(UiText.text(getContext(),label));note.setText(value.hasVoice&&!value.playable?UiText.text(getContext(),value.reason):"");note.setVisibility(value.hasVoice&&!value.playable?VISIBLE:GONE);}
    }
    private ImageButton button(String symbol,String label){ImageButton button=new ImageButton(getContext());button.setImageDrawable(new Symbol(symbol));button.setContentDescription(UiText.text(getContext(),label));button.setTooltipText(UiText.text(getContext(),label));button.setPadding(dp(11),dp(11),dp(11),dp(11));GradientDrawable base=new GradientDrawable();base.setColor(0xb32c4a6b);base.setCornerRadius(dp(12));button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x448dbbe9),base,null));return button;}
    static final class Symbol extends Drawable {
        final String kind;final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        Symbol(String kind){this.kind=kind;paint.setColor(0xffe5eefb);paint.setStrokeWidth(1.8f);paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);}
        @Override public void draw(Canvas canvas){canvas.save();canvas.translate(getBounds().left,getBounds().top);canvas.scale(getBounds().width()/24f,getBounds().height()/24f);Path path=new Path();paint.setStyle(Paint.Style.FILL);if(kind.equals("play")){path.moveTo(7,4);path.lineTo(20,12);path.lineTo(7,20);path.close();canvas.drawPath(path,paint);}else if(kind.equals("rewind")){paint.setStyle(Paint.Style.STROKE);canvas.drawArc(4,4,20,20,-135,285,false,paint);path.moveTo(3,4);path.lineTo(3,11);path.lineTo(10,11);canvas.drawPath(path,paint);}else if(kind.equals("stop"))canvas.drawRoundRect(5,5,19,19,2,2,paint);else{paint.setStyle(Paint.Style.STROKE);if(kind.equals("close")){canvas.drawLine(6,6,18,18,paint);canvas.drawLine(18,6,6,18,paint);}else{path.moveTo(14,5);path.lineTo(7,12);path.lineTo(14,19);canvas.drawPath(path,paint);canvas.drawLine(7,12,20,12,paint);}}canvas.restore();}
        @Override public void setAlpha(int alpha){paint.setAlpha(alpha);invalidateSelf();}@Override public void setColorFilter(ColorFilter filter){paint.setColorFilter(filter);invalidateSelf();}@Override public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
    private int dp(float value){return Math.round(value*getContext().getResources().getDisplayMetrics().density);}

    static final class Glyph {
        final String base,reading;final float x,y,rx,ry;final int color,shadow;final boolean italic;
        Glyph(TextOverlay.NativeTextToken token,float x,float y,float rx,float ry,int color,int shadow){base=token.base;reading=token.ruby.text;this.x=x;this.y=y;this.rx=rx;this.ry=ry;this.color=color;this.shadow=shadow;italic=token.italic;}
    }
    static final class Layout {
        final Glyph[] glyphs;final float height;final String caption;
        Layout(HistoryModel.Group group,float width,float font,TextPaint paint){ArrayList<Glyph> out=new ArrayList<>();StringBuilder description=new StringBuilder();paint.setTypeface(NORMAL);paint.setTextSize(font);float x=0,y=-paint.ascent()+font*.7f;
            for(HistoryModel.Row row:group.rows){y+=row.leadingBreaks*font*1.65f;if(row.leadingBreaks>0){description.append('\n');x=0;}
                for(HistoryModel.Run run:HistoryModel.runs(row)){
                    List<TextOverlay.NativeTextToken> tokens;
                    try{tokens=TextOverlay.NativeTextToken.parse((run.italic?"@i":"")+run.text);}catch(IllegalArgumentException malformed){tokens=TextOverlay.NativeTextToken.parse((run.italic?"@i":"")+plain(run.text));}
                    for(TextOverlay.NativeTextToken token:tokens){if(token.newline){description.append('\n');x=0;y+=font*1.65f;continue;}description.append(token.base);paint.setTypeface(token.italic?ITALIC:NORMAL);paint.setTextSize(font);float measured=paint.measureText(token.base);
                        // History is a readable mobile reflow, with explicit LF/ruby intact.
                        if(x>0&&x+measured>width){x=0;y+=font*1.65f;}paint.setTextSize(font*.45f);float rx=token.ruby.left(x,measured,paint.measureText(token.ruby.text),font/row.nativeFont),ry=y-font*1.05f+token.ruby.y*font/row.nativeFont;out.add(new Glyph(token,x,y,rx,ry,run.color,run.shadow));x+=measured;
                    }
                }
            }
            glyphs=out.toArray(new Glyph[0]);height=Math.max(font*1.65f,y+font*.35f);caption=description.toString();
        }
        int first(float top){int lo=0,hi=glyphs.length;while(lo<hi){int mid=(lo+hi)>>>1;if(glyphs[mid].y<top)lo=mid+1;else hi=mid;}return lo;}
        private static String plain(String value){return value.replaceAll("<([^<>|]+)\\|[^<>]*>","$1").replaceAll("<[^>]*>","");}
    }
    private static final Typeface NORMAL=Typeface.create("serif",Typeface.NORMAL),ITALIC=Typeface.create("serif",Typeface.ITALIC);
    static final class LayoutCache extends LinkedHashMap<String,Layout> {
        LayoutCache(){super(32,.75f,true);}@Override protected boolean removeEldestEntry(Map.Entry<String,Layout> entry){return size()>32;}
    }
    private static final class MessageView extends View {
        final LayoutCache cache;final TextPaint paint=new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);final Rect clip=new Rect();HistoryModel.Group group;Layout layout;
        MessageView(Context context,LayoutCache cache){super(context);this.cache=cache;paint.setTypeface(NORMAL);paint.setTextLocale(java.util.Locale.forLanguageTag(UiText.language()));setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);}
        void group(HistoryModel.Group value){if(group==value)return;group=value;layout=null;requestLayout();invalidate();}
        @Override protected void onMeasure(int widthSpec,int heightSpec){int width=Math.max(1,MeasureSpec.getSize(widthSpec));float font=18*getResources().getDisplayMetrics().scaledDensity;paint.setTextLocale(java.util.Locale.forLanguageTag(UiText.language()));String key=group==null?"":UiText.language()+":"+group.key+":"+width+":"+Float.floatToIntBits(font);layout=cache.get(key);if(layout==null&&group!=null){layout=new Layout(group,width,font,paint);cache.put(key,layout);}setContentDescription(layout==null?"":layout.caption);setMeasuredDimension(width,Math.max(1,(int)Math.ceil(layout==null?font*1.65:layout.height)));}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);if(layout==null)return;float font=18*getResources().getDisplayMetrics().scaledDensity;canvas.getClipBounds(clip);float lower=clip.top-font*1.5f,upper=clip.bottom+font*1.5f;boolean italic=false,styled=false;int color=0,shadow=0;paint.setTypeface(NORMAL);paint.setTextSize(font);
            for(int i=layout.first(lower);i<layout.glyphs.length;i++){Glyph glyph=layout.glyphs[i];if(glyph.y>upper)break;if(italic!=glyph.italic){italic=glyph.italic;paint.setTypeface(italic?ITALIC:NORMAL);}if(!styled||color!=glyph.color||shadow!=glyph.shadow){styled=true;color=glyph.color;shadow=glyph.shadow;paint.setColor(color);paint.setShadowLayer(Math.max(1,font*.04f),0,font*.025f,shadow);}canvas.drawText(glyph.base,glyph.x,glyph.y,paint);if(!glyph.reading.isEmpty()){paint.setTextSize(font*.45f);canvas.drawText(glyph.reading,glyph.rx,glyph.ry,paint);paint.setTextSize(font);}}
        }
    }
}
