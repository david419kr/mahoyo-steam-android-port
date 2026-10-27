package org.mahoyo.port;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.*;
import android.graphics.drawable.*;
import android.text.TextPaint;
import android.view.*;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.*;
import org.json.JSONObject;
import java.io.File;
import java.util.*;
import java.util.function.*;

/** Mobile choices consume background taps; the sibling game menu remains above us. */
final class ChoiceView extends FrameLayout {
    private final ChoiceSession session=new ChoiceSession();private final FrontendImages images;private final Consumer<ChoiceSession.Request> selected;
    private final FrameLayout host;private AbsListView list;private RectF bounds=new RectF();private ChoiceModel rendered;private String revision="";
    private final LinkedHashMap<String,CaptionLayout> layouts=new LinkedHashMap<String,CaptionLayout>(32,.75f,true){@Override protected boolean removeEldestEntry(Map.Entry<String,CaptionLayout> e){return size()>32;}};
    ChoiceView(Context context,Function<String,GameAsset> files,Consumer<ChoiceSession.Request> selected,Consumer<String> errors){super(context);this.selected=selected;images=new FrontendImages(files,errors);setClickable(true);setFocusableInTouchMode(true);setBackgroundColor(0x70101e33);setVisibility(GONE);host=new FrameLayout(context);addView(host,new FrameLayout.LayoutParams(-1,-1));}
    void state(JSONObject value){String next=value==null?"":value.optString("revision");if(value!=null&&next.isEmpty())throw new IllegalArgumentException("Missing choice revision");if(next.equals(revision))return;ChoiceModel parsed=ChoiceModel.parse(value);if(parsed!=null)for(ChoiceModel.Item item:parsed.items)validateCaption(item.label);revision=next;if(session.state(parsed))refresh();}
    void presented(JSONObject value){if(session.presented(value==null?"":value.optString("revision")))render();}
    boolean active(){return session.active();}
    void interactive(boolean value){if(session.blocked(!value))refresh();}
    void complete(ChoiceSession.Request request){if(session.complete(request))refresh();}
    void bounds(RectF area){if(bounds.equals(area))return;bounds=new RectF(area);FrameLayout.LayoutParams p=new FrameLayout.LayoutParams(Math.max(1,Math.round(area.width())),Math.max(1,Math.round(area.height())));p.leftMargin=Math.round(area.left);p.topMargin=Math.round(area.top);host.setLayoutParams(p);resize();}
    private void render(){ChoiceModel next=session.shown();if(rendered==next)return;rendered=next;images.invalidate();layouts.clear();host.removeAllViews();list=null;if(next==null){setVisibility(GONE);return;}setVisibility(VISIBLE);requestFocus();
        if(next.profile.equals("text161")){ListView rows=new ListView(getContext());rows.setDivider(new ColorDrawable(Color.TRANSPARENT));rows.setDividerHeight(dp(8));rows.setCacheColorHint(Color.TRANSPARENT);rows.setSelector(new ColorDrawable(Color.TRANSPARENT));rows.setItemsCanFocus(true);rows.setPadding(dp(8),dp(8),dp(8),dp(8));rows.setClipToPadding(false);list=rows;}
        else{GridView grid=new GridView(getContext());grid.setHorizontalSpacing(dp(8));grid.setVerticalSpacing(dp(8));grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);grid.setSelector(new ColorDrawable(Color.TRANSPARENT));grid.setPadding(dp(8),dp(8),dp(8),dp(8));grid.setClipToPadding(false);list=grid;}
        list.setAdapter(adapter);host.addView(list);resize();refresh();
    }
    private void resize(){if(list==null||rendered==null)return;int width=Math.max(1,Math.round(bounds.width())-dp(24)),height=Math.max(1,(int)((bounds.height()-dp(24))*.8f));boolean text=rendered.profile.equals("text161");if(text){width=Math.min(dp(720),width);int needed=dp(16);for(ChoiceModel.Item item:rendered.items){needed+=Math.max(dp(52),(int)Math.ceil(caption(item,Math.max(1,width-dp(16))).height)+dp(20))+dp(8);if(needed>=height)break;}height=Math.min(height,Math.max(1,needed-dp(8)));}else ((GridView)list).setNumColumns(ChoiceModel.columns(width,getResources().getDisplayMetrics().density));FrameLayout.LayoutParams p=new FrameLayout.LayoutParams(width,height,Gravity.CENTER);list.setLayoutParams(p);list.setBackground(shape(0xd51a2a41,16));}
    private void refresh(){if(list!=null)((BaseAdapter)list.getAdapter()).notifyDataSetChanged();}
    private void select(ChoiceModel.Item item){ChoiceSession.Request request=session.begin(item.ordinal);if(request==null)return;refresh();selected.accept(request);}
    private final BaseAdapter adapter=new BaseAdapter(){
        @Override public int getCount(){return rendered==null?0:rendered.items.size();}@Override public Object getItem(int i){return rendered.items.get(i);}@Override public long getItemId(int i){return rendered.items.get(i).ordinal;}
        @Override public boolean isEnabled(int i){return session.enabled()&&rendered.items.get(i).enabled;}
        @Override public View getView(int index,View old,ViewGroup parent){ChoiceModel.Item item=rendered.items.get(index);if(rendered.profile.equals("text161")){Caption cell=old instanceof Caption?(Caption)old:new Caption(getContext());cell.bind(item);return cell;}Picture cell=old instanceof Picture?(Picture)old:new Picture(getContext());cell.bind(item);return cell;}
    };
    private void button(View view,ChoiceModel.Item item){view.setEnabled(session.enabled()&&item.enabled);view.setAlpha(item.enabled?1:.45f);view.setContentDescription(item.label.isEmpty()?UiText.format(getContext(),"choice_number",item.ordinal+1):plainCaption(item.label));view.setOnClickListener(v->select(item));view.setFocusable(true);}
    private GradientDrawable shape(int color,int radius){GradientDrawable value=new GradientDrawable();value.setColor(color);value.setCornerRadius(dp(radius));return value;}
    private Drawable card(){return new RippleDrawable(ColorStateList.valueOf(0x558dbbe9),shape(0xb32c4a6b,12),null);}
    private int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
    private CaptionLayout caption(ChoiceModel.Item item,int width){float font=18*getResources().getDisplayMetrics().scaledDensity;String key=rendered.revision+":"+item.ordinal+":"+width+":"+font;CaptionLayout layout=layouts.get(key);if(layout==null){layout=new CaptionLayout(item.label,Math.max(1,width-dp(32)),font,new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG));layouts.put(key,layout);}return layout;}
    void close(){session.close();images.close();layouts.clear();host.removeAllViews();setVisibility(GONE);}
    private final class Picture extends FrameLayout {
        final ImageView mono,color;ChoiceModel.Item item;boolean ready,colorReady;String key="",colorKey="";
        Picture(Context context){super(context);setBackground(card());setPadding(dp(8),dp(8),dp(8),dp(8));mono=new ImageView(context);color=new ImageView(context);for(ImageView image:new ImageView[]{mono,color}){image.setScaleType(ImageView.ScaleType.FIT_CENTER);image.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);addView(image,new FrameLayout.LayoutParams(-1,-1));}setOnFocusChangeListener((v,focus)->appearance());}
        void bind(ChoiceModel.Item value){item=value;button(this,value);setEnabled(isEnabled()&&ready);retainCellLayout();String next=rendered.revision+":"+value.ordinal;if(!next.equals(key)){key=next;colorKey="";ready=colorReady=false;color.setVisibility(GONE);setEnabled(false);ChoiceModel.Item expected=value;images.bind(value.image,null,mono,dp(200),dp(360),()->{if(item!=expected)return;ready=mono.getDrawable()!=null;setEnabled(session.enabled()&&expected.enabled&&ready);});}appearance();}
        // GridView measures and scraps item0 before its first window attachment.
        private void retainCellLayout(){ViewGroup.LayoutParams size=getLayoutParams();int height=dp(320);if(size==null)setLayoutParams(new AbsListView.LayoutParams(-1,height));else if(size.width!=-1||size.height!=height){size.width=-1;size.height=height;setLayoutParams(size);}}
        @Override public void setPressed(boolean value){super.setPressed(value);if(color!=null)appearance();}
        private void appearance(){if(item==null)return;boolean active=isPressed()||isFocused();if(!active){color.setVisibility(GONE);return;}if(!colorKey.equals(key)){colorKey=key;ChoiceModel.Item expected=item;images.bind(item.colorImage,null,color,dp(200),dp(360),()->{if(item!=expected)return;colorReady=color.getDrawable()!=null;color.setVisibility(colorReady&&(isPressed()||isFocused())?VISIBLE:GONE);});}color.setVisibility(colorReady?VISIBLE:GONE);}
    }
    private static final Typeface NORMAL=Typeface.create("serif",Typeface.NORMAL),ITALIC=Typeface.create("serif",Typeface.ITALIC);
    static final class Mark {final String base,ruby;final float x,y,rx,ry;final int color,shadow;final boolean italic;Mark(TextOverlay.NativeTextToken token,float x,float y,float rx,float ry,int color,int shadow){base=token.base;ruby=token.ruby.text;this.x=x;this.y=y;this.rx=rx;this.ry=ry;this.color=color;this.shadow=shadow;italic=token.italic;}}
    static final class CaptionLayout {
        final List<Mark> marks;final float height;final String caption;
        CaptionLayout(String source,float width,float font,TextPaint paint){ArrayList<Mark> out=new ArrayList<>();StringBuilder text=new StringBuilder();paint.setTextSize(font);paint.setTypeface(NORMAL);float x=0,y=-paint.ascent()+font*.7f;
            for(HistoryModel.Run run:HistoryModel.runs(source,false,0xfff0eee5))for(TextOverlay.NativeTextToken token:TextOverlay.NativeTextToken.parse((run.italic?"@i":"")+run.text)){if(token.newline){x=0;y+=font*1.65f;text.append('\n');continue;}paint.setTextSize(font);paint.setTypeface(token.italic?ITALIC:NORMAL);float advance=paint.measureText(token.base);if(x>0&&x+advance>width){x=0;y+=font*1.65f;}paint.setTextSize(font*.45f);out.add(new Mark(token,x,y,token.ruby.left(x,advance,paint.measureText(token.ruby.text),font/48),y-font*1.05f+token.ruby.y*font/48,run.color,run.shadow));text.append(token.base);x+=advance;}
            marks=Collections.unmodifiableList(out);height=Math.max(font*1.65f,y+font*.35f);caption=text.toString();
        }
    }
    private final class Caption extends View {
        ChoiceModel.Item item;CaptionLayout layout;final TextPaint paint=new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);
        Caption(Context context){super(context);setBackground(card());setPadding(dp(16),dp(10),dp(16),dp(10));setAccessibilityDelegate(new AccessibilityDelegate(){@Override public void onInitializeAccessibilityNodeInfo(View host,AccessibilityNodeInfo info){super.onInitializeAccessibilityNodeInfo(host,info);info.setClassName(Button.class.getName());}});}
        void bind(ChoiceModel.Item value){if(item!=value){item=value;layout=null;requestLayout();}button(this,value);}
        @Override protected void onMeasure(int w,int h){int width=Math.max(1,MeasureSpec.getSize(w));layout=caption(item,width);setContentDescription(layout.caption.isEmpty()?UiText.format(getContext(),"choice_number",item.ordinal+1):layout.caption);setMeasuredDimension(width,Math.max(dp(52),(int)Math.ceil(layout.height)+getPaddingTop()+getPaddingBottom()));}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);if(layout==null)return;canvas.save();canvas.translate(getPaddingLeft(),getPaddingTop());float font=18*getResources().getDisplayMetrics().scaledDensity;for(Mark mark:layout.marks){paint.setTypeface(mark.italic?ITALIC:NORMAL);paint.setTextSize(font);paint.setColor(mark.color);paint.setShadowLayer(Math.max(1,font*.04f),0,font*.025f,mark.shadow);canvas.drawText(mark.base,mark.x,mark.y,paint);if(!mark.ruby.isEmpty()){paint.setTextSize(font*.45f);canvas.drawText(mark.ruby,mark.rx,mark.ry,paint);}}canvas.restore();}
    }
    static void validateCaption(String text){for(HistoryModel.Run run:HistoryModel.runs(text,false,0xffffffff))TextOverlay.NativeTextToken.parse((run.italic?"@i":"")+run.text);}
    private static String plainCaption(String text){StringBuilder out=new StringBuilder();for(HistoryModel.Run run:HistoryModel.runs(text,false,0xffffffff))for(TextOverlay.NativeTextToken token:TextOverlay.NativeTextToken.parse(run.text))out.append(token.newline?"\n":token.base);return out.toString();}
}
