package org.mahoyo.port;

import android.content.Context;
import android.graphics.*;
import android.text.*;
import android.view.*;
import android.widget.OverScroller;
import java.util.*;
import org.json.JSONObject;

final class TextOverlay extends View {
    private Scene scene;private Viewport viewport;private boolean hidden;
    private final TextPainter painter=new TextPainter();
    private final OverScroller scroller;
    private final ScrollState scroll=new ScrollState();
    private final LinkedHashMap<String,PanelLayout> layouts=new LinkedHashMap<>(8,.75f,true);
    private final int touchSlop,maxFling,minFling;
    private VelocityTracker velocity;
    private float downX,downY,downScroll,panelFont,panelPadding;
    private boolean touching,dragging;
    private Runnable advance;
    private RenderState renderState;
    private RenderState presentedState;
    private String presentedVisualKey="";
    private final Runnable cursorFrame=()->invalidate();
    private Scene preparedScene;
    private int preferredPanelHeight;
    private float fontScale=1,marginScale=1;
    private float panelScale=1,effectiveScale=1,maximumScale=2,pinchSpan,pinchFont,pinchMargin,density=1;
    private boolean handleClaimed,handleChanged;private float handleDownY,handleStartHeight,handleStartScale;
    private final Paint handlePaint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private boolean pinchEligible,pinchClaimed;
    private Runnable resized,resizeFinished,typographyChanged,typographyFinished;
    TextOverlay(Context context){super(context);setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);scroller=new OverScroller(context);ViewConfiguration config=ViewConfiguration.get(context);touchSlop=config.getScaledTouchSlop();maxFling=config.getScaledMaximumFlingVelocity();minFling=config.getScaledMinimumFlingVelocity();}
    void setOnAdvance(Runnable callback){advance=callback;}
    void setPanelScale(float value){panelScale=Float.isFinite(value)?Math.max(.6f,value):1;}
    float panelScale(){return panelScale;}
    float backlogScrollBudget(float x,float y){return viewport!=null&&viewport.panel.contains(x,y)?scroll.offset:0;}
    float forwardScrollBudget(float x,float y){return viewport!=null&&viewport.panel.contains(x,y)?Math.max(0,scroll.maximum-scroll.offset):0;}
    static float horizontalInset(float width,float padding){return horizontalInset(width,padding,1);}
    static float horizontalInset(float width,float padding,float scale){return Math.max(padding,width*.12f*scale);}
    void setOnPanelResize(Runnable changed,Runnable finished){resized=changed;resizeFinished=finished;}
    void setOnTypographyChange(Runnable changed,Runnable finished){typographyChanged=changed;typographyFinished=finished;}
    int fontPercent(){return Math.round(fontScale*100);}
    int marginPercent(){return Math.round(marginScale*100);}
    int panelHeight(Scene scene,DisplayPrefs prefs,int contentWidth,int contentHeight,float density,float scaledDensity){
        this.density=density;if(!pinchClaimed){fontScale=prefs.fontPercent()/100f;marginScale=prefs.marginPercent()/100f;}
        effectiveScale=PanelSizing.scale(contentHeight,density,scaledDensity,panelScale);maximumScale=PanelSizing.scale(contentHeight,density,scaledDensity,Float.MAX_VALUE);
        // Typography and box height are independent controls.
        panelFont=PanelSizing.font(contentHeight,density,scaledDensity)*fontScale;panelPadding=20*density;
        boolean separate=prefs.separateDialogue(contentWidth,contentHeight);preferredPanelHeight=PanelSizing.height(separate,false,"play",contentHeight,density,scaledDensity,panelScale);
        return scene.wholeFrame?0:PanelSizing.height(separate,scene.movie!=null,scene.json.optString("scene",""),contentHeight,density,scaledDensity,panelScale);
    }
    private boolean typographyEligible(float x,float y){return !hidden&&scene!=null&&scene.movie==null&&scene.transition==null&&(preparedScene==null||preparedScene.transition==null)&&scene.json.optString("scene","").equals("play")&&viewport!=null&&viewport.content.contains(x,y);}
    // Activity forwards multi-touch before its two-finger tap recognizer consumes it.
    boolean pinch(MotionEvent event){
        int action=event.getActionMasked();
        if(action==MotionEvent.ACTION_DOWN){pinchClaimed=false;pinchSpan=0;pinchEligible=typographyEligible(event.getX(),event.getY());}
        if(pinchEligible&&event.getPointerCount()==2){
            if(pinchSpan==0){pinchEligible=typographyEligible(event.getX(1),event.getY(1));if(pinchEligible){pinchSpan=(float)Math.hypot(event.getX(0)-event.getX(1),event.getY(0)-event.getY(1));pinchFont=fontScale;pinchMargin=marginScale;}}
            float span=(float)Math.hypot(event.getX(0)-event.getX(1),event.getY(0)-event.getY(1));
            if(pinchEligible&&pinchSpan>0&&action==MotionEvent.ACTION_MOVE){
                if(Math.abs(span-pinchSpan)>touchSlop*2){pinchClaimed=true;scroller.abortAnimation();endTouch();}
                if(pinchClaimed){
                    int font=Math.round(Math.max(.75f,Math.min(1.5f,pinchFont*span/pinchSpan))*100);
                    float ratio=font/(100f*pinchFont);int margin=Math.round(Math.max(0,Math.min(1.6f,pinchMargin+1-ratio))*100);
                    if(font!=fontPercent()||margin!=marginPercent()){fontScale=font/100f;marginScale=margin/100f;if(typographyChanged!=null)typographyChanged.run();}
                }
            }
        }else if(event.getPointerCount()>2)pinchEligible=false;
        boolean consume=pinchClaimed;
        if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_CANCEL){if(pinchClaimed&&typographyFinished!=null)typographyFinished.run();pinchEligible=pinchClaimed=false;pinchSpan=0;}
        return consume;
    }
    RectF resizeHandleBounds(){return viewport==null?new RectF(0,0,0,0):new RectF(Math.max(viewport.panel.left,viewport.panel.centerX()-50*density),viewport.panel.top+2*density,Math.min(viewport.panel.right,viewport.panel.centerX()+50*density),Math.min(viewport.panel.bottom,viewport.panel.top+34*density));}
    boolean resizeHandle(MotionEvent event){
        int action=event.getActionMasked();
        if(action==MotionEvent.ACTION_DOWN){
            handleClaimed=typographyEligible(event.getX(),event.getY())&&viewport.panel.height()>0&&resizeHandleBounds().contains(event.getX(),event.getY());handleChanged=false;
            if(handleClaimed){handleDownY=event.getY();handleStartHeight=viewport.panel.height();handleStartScale=effectiveScale;scroller.abortAnimation();endTouch();invalidate();}
        }
        if(!handleClaimed)return false;
        if(action==MotionEvent.ACTION_MOVE&&event.getPointerCount()==1&&Math.abs(event.getY()-handleDownY)>touchSlop){
            float height=handleStartHeight+handleDownY-event.getY(),next=Math.min(maximumScale,Math.max(.6f,handleStartScale*height/handleStartHeight));
            if(Math.abs(next-panelScale)>.001f){panelScale=next;handleChanged=true;if(resized!=null)resized.run();}
        }
        if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_CANCEL){if(handleChanged&&resizeFinished!=null)resizeFinished.run();handleClaimed=handleChanged=false;invalidate();}
        return true;
    }
    private void drawResizeHandle(Canvas canvas){
        if(viewport.panel.height()<=0||scene.movie!=null||!scene.json.optString("scene","").equals("play"))return;
        float center=viewport.panel.centerX(),top=viewport.panel.top+4*density,verticalUnit=.85f*density;
        canvas.save();canvas.clipRect(viewport.panel);handlePaint.setColor(handleClaimed?0x99465f7d:0x55384b64);
        canvas.drawRect(new RectF(center-30*density,top,center+30*density,top+18*verticalUnit),handlePaint);
        handlePaint.setColor(handleClaimed?0xccd9e7f5:0x99b5c8dd);
        for(int row=0;row<2;row++)canvas.drawRect(new RectF(center-14*density,top+(5+6*row)*verticalUnit,center+14*density,top+(7+6*row)*verticalUnit),handlePaint);
        canvas.restore();
    }
    RenderState prepare(Scene scene,Viewport viewport,boolean hidden){
        PanelSnapshot previous=presentedState==null?null:presentedState.current;preparedScene=scene;ScrollState planned=scroll.copy();
        if(viewport.panel.height()>0){
            float font=panelFont>0?panelFont:40*viewport.scale,padding=panelPadding>0?panelPadding:font*.9f;
            PanelLayout layout=layout(scene,Math.max(1,viewport.panel.width()-2*horizontalInset(viewport.panel.width(),padding,marginScale)),font);
            if(previous!=null&&(previous.layout.width!=layout.width||previous.layout.font!=font)&&!planned.following)planned.offset=layout.offsetFor(previous.layout.anchor(planned.offset));
            planned.update(layout.document,Math.max(0,layout.visibleHeight(scene)-(viewport.panel.height()-2*padding)));
            if(previous!=null&&!previous.source.json.optString("language","ja").equals(scene.json.optString("language","ja"))){
                // Keep the reader's position when translated text no longer shares a prefix.
                planned.following=previous.following;planned.offset=previous.following?planned.maximum:Math.min(planned.maximum,layout.offsetFor(previous.layout.anchor(previous.scroll)));
            }
            PanelSnapshot current=new PanelSnapshot(scene,layout,planned.offset,planned.following,Math.round(viewport.panel.height()));
            renderState=new RenderState(current,oldSnapshot(scene,previous,viewport,font,padding),font,padding,scroll.revision,fontScale,marginScale);
        }else{float font=panelFont>0?panelFont:40*viewport.scale,padding=panelPadding>0?panelPadding:font*.9f;renderState=new RenderState(null,oldSnapshot(scene,previous,viewport,font,padding),font,padding,scroll.revision,fontScale,marginScale);}
        if(hidden||scene.transition!=null){scroller.abortAnimation();endTouch();}
        return renderState;
    }
    private PanelSnapshot oldSnapshot(Scene scene,PanelSnapshot previous,Viewport viewport,float font,float padding){if(scene.transition==null||scene.transition.old==null||preferredPanelHeight<=0||scene.transition.old.movie!=null)return null;Scene source=scene.transition.old;String key=contentKey(source);PanelSnapshot saved=previous!=null&&previous.key.equals(key)?previous:renderState==null?null:renderState.snapshot(source);if(saved==null)return null;PanelLayout oldLayout=layout(source,Math.max(1,viewport.content.width()-2*horizontalInset(viewport.content.width(),padding,marginScale)),font);float oldScroll=saved.layout==oldLayout?saved.scroll:oldLayout.offsetFor(saved.layout.anchor(saved.scroll));return new PanelSnapshot(source,oldLayout,Math.min(oldScroll,Math.max(0,oldLayout.visibleHeight(source)-(preferredPanelHeight-2*padding))),saved.following,preferredPanelHeight);}
    void present(Scene scene,Viewport viewport,boolean hidden){if(preparedScene!=scene||renderState==null)prepare(scene,viewport,hidden);RenderState next=renderState;
        if(next.current!=null){PanelSnapshot current=next.current;boolean preserve=scroll.revision!=next.revision&&current.layout.document.startsWith(scroll.document);if(!current.layout.document.startsWith(scroll.document))scroller.abortAnimation();float previousMaximum=scroll.maximum;scroll.update(current.layout.document,Math.max(0,current.layout.visibleHeight(scene)-(viewport.panel.height()-2*next.padding)));if(scroll.following&&scroll.maximum>previousMaximum)scroller.abortAnimation();if(!preserve){scroll.offset=current.scroll;scroll.following=current.following;}next=new RenderState(current.withScroll(scroll.offset,scroll.following),next.old,next.font,next.padding,scroll.revision,next.fontScale,next.marginScale);
        }else{scroller.abortAnimation();scroll.clear();endTouch();}
        String visualKey=contentKey(scene)+":"+fullscreenBackdropAlpha(scene)+":"+(scene.transition!=null)+":"+hidden+":"+viewport.area+":"+viewport.frame+":"+viewport.panel+":"+viewport.scale+":"+next.font+":"+next.padding+":"+next.fontScale+":"+next.marginScale+":"+(next.current==null?"":next.current.scroll+":"+next.current.following);
        this.scene=scene;this.viewport=viewport;this.hidden=hidden;presentedState=next;
        // Graphics-only ticks retain the Canvas display list; dialogue/cursor/scroll changes still redraw at native resolution.
        if(!visualKey.equals(presentedVisualKey)){presentedVisualKey=visualKey;invalidate();}}
    RenderState update(Scene scene,Viewport viewport,boolean hidden){present(scene,viewport,hidden);return presentedState;}
    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);removeCallbacks(cursorFrame);
        // Keep live dialogue on Canvas across graphics transitions; outgoing movie/title can capture it.
        if(scene==null||viewport==null||hidden)return;
        painter.draw(canvas,scene,viewport,presentedState,!touching&&scroller.isFinished());drawResizeHandle(canvas);
        if(scene.transition==null&&!touching&&(viewport.panel.height()<=0||scroll.following))for(JSONObject item:scene.texts)if(item.optBoolean("cursor",false)){postDelayed(cursorFrame,50-android.os.SystemClock.uptimeMillis()%50);break;}
    }
    @Override protected void onDetachedFromWindow(){removeCallbacks(cursorFrame);super.onDetachedFromWindow();}
    @Override public boolean onTouchEvent(MotionEvent event){
        int action=event.getActionMasked();
        if(action==MotionEvent.ACTION_DOWN){if(hidden||scene==null||scene.transition!=null||preparedScene!=null&&preparedScene.transition!=null||viewport==null||viewport.panel.height()<=0||!viewport.panel.contains(event.getX(),event.getY()))return false;scroller.abortAnimation();touching=true;dragging=false;downX=event.getX();downY=event.getY();downScroll=scroll.offset;velocity=VelocityTracker.obtain();velocity.addMovement(event);return true;}
        if(!touching)return false;if(velocity!=null)velocity.addMovement(event);
        if(action==MotionEvent.ACTION_MOVE){if(Math.hypot(event.getX()-downX,event.getY()-downY)>touchSlop)dragging=true;if(dragging){scroll.manual(downScroll+downY-event.getY());publishScroll();}return true;}
        if(action==MotionEvent.ACTION_UP){if(Math.hypot(event.getX()-downX,event.getY()-downY)>touchSlop)dragging=true;if(dragging&&velocity!=null){velocity.computeCurrentVelocity(1000,maxFling);int speed=Math.round(-velocity.getYVelocity());if(Math.abs(speed)>=minFling){scroller.fling(0,Math.round(scroll.offset),0,speed,0,0,0,Math.round(scroll.maximum));postInvalidateOnAnimation();}}else performClick();endTouch();return true;}
        if(action==MotionEvent.ACTION_CANCEL){endTouch();return true;}return true;
    }
    private void endTouch(){touching=false;dragging=false;if(velocity!=null){velocity.recycle();velocity=null;}}
    @Override public boolean performClick(){super.performClick();if(advance!=null)advance.run();return true;}
    @Override public void computeScroll(){if(scroller.computeScrollOffset()){scroll.manual(scroller.getCurrY());publishScroll();postInvalidateOnAnimation();}}
    private void publishScroll(){if(presentedState!=null&&presentedState.current!=null)presentedState=new RenderState(presentedState.current.withScroll(scroll.offset,scroll.following),presentedState.old,presentedState.font,presentedState.padding,scroll.revision,presentedState.fontScale,presentedState.marginScale);invalidate();}
    private PanelLayout layout(Scene scene,float width,float font){String key=layoutKey(scene)+":"+width+":"+font;PanelLayout value=layouts.get(key);if(value==null){value=new PanelLayout(scene,width,font,painter.paint);layouts.put(key,value);while(layouts.size()>6)layouts.remove(layouts.keySet().iterator().next());}return value;}
    private static String contentKey(Scene scene){return scene.json.optString("language","ja")+":"+scene.texts.toString();}
    private static String layoutKey(Scene scene){StringBuilder key=new StringBuilder(scene.json.optString("language","ja"));for(JSONObject item:scene.texts)key.append(item.optString("text","")).append('\u0000').append(item.opt("color")).append('\u0000').append(item.optDouble("fontSize",40)).append('\u0001');return key.toString();}
    static final class RenderState {
        final PanelSnapshot current,old;final float font,padding,fontScale,marginScale;final long revision;
        RenderState(PanelSnapshot current,PanelSnapshot old,float font,float padding,long revision,float fontScale,float marginScale){this.fontScale=fontScale;this.marginScale=marginScale;this.current=current;this.old=old;this.font=font;this.padding=padding;this.revision=revision;}
        PanelSnapshot snapshot(Scene source){if(current!=null&&current.source==source)return current;if(old!=null&&old.source==source)return old;String key=contentKey(source);if(old!=null&&old.key.equals(key))return old;return current!=null&&current.key.equals(key)?current:null;}
        String captureKey(Scene source){PanelSnapshot value=snapshot(source);return fontScale+":"+marginScale+":"+(value==null?"":value.layout.width+":"+font+":"+padding+":"+value.scroll+":"+value.panelHeight);}
        Viewport viewportFor(Scene source,Viewport viewport){PanelSnapshot value=snapshot(source);return source.movie!=null||source.wholeFrame?viewport.withPanel(0):value==null?viewport:viewport.withPanel(value.panelHeight);}
    }
    private static final class PanelSnapshot {
        final Scene source;final String key;final PanelLayout layout;final float scroll;final boolean following;final int panelHeight;
        PanelSnapshot(Scene scene,PanelLayout layout,float scroll,boolean following,int panelHeight){this(scene,contentKey(scene),layout,scroll,following,panelHeight);}
        private PanelSnapshot(Scene source,String key,PanelLayout layout,float scroll,boolean following,int panelHeight){this.source=source;this.key=key;this.layout=layout;this.scroll=scroll;this.following=following;this.panelHeight=panelHeight;}
        PanelSnapshot withScroll(float scroll,boolean following){return new PanelSnapshot(source,key,layout,scroll,following,panelHeight);}
    }
    static final class PanelSizing {
        static float font(int height,float density,float scaledDensity){return (height/density>=600?24:22)*scaledDensity;}
        static float baseHeight(int height,float density,float scaledDensity){int lines=height/density>=600?4:3;return (lines*1.65f+.25f)*font(height,density,scaledDensity)+40*density;}
        static float scale(int height,float density,float scaledDensity,float requested){float value=Float.isFinite(requested)?Math.max(.6f,requested):1;return Math.max(0,Math.min(value,height/3f/baseHeight(height,density,scaledDensity)));}
        static int height(boolean enabled,boolean movie,String mode,int height,float density,float scaledDensity){return height(enabled,movie,mode,height,density,scaledDensity,1);}
        static int height(boolean enabled,boolean movie,String mode,int height,float density,float scaledDensity,float requested){if(!enabled||movie||!(mode.equals("play")||mode.equals("history")))return 0;return Math.max(0,Math.min(height/3,Math.round(baseHeight(height,density,scaledDensity)*scale(height,density,scaledDensity,requested))));}
    }
    static final class ScrollState {
        private String document="";float offset,maximum;boolean following=true;long revision;
        ScrollState copy(){ScrollState state=new ScrollState();state.document=document;state.offset=offset;state.maximum=maximum;state.following=following;state.revision=revision;return state;}
        void update(String next,float limit){if(!next.startsWith(document)){offset=0;following=true;}document=next;maximum=Math.max(0,limit);offset=following?maximum:Math.max(0,Math.min(maximum,offset));}
        void manual(float value){offset=Math.max(0,Math.min(maximum,value));following=maximum-offset<1;revision++;}
        void clear(){document="";offset=maximum=0;following=true;}
    }
    static final class RubyReading {
        final String text;final int x,y;final boolean fixedLeft;
        RubyReading(String text,int x,int y){this(text,x,y,false);}
        RubyReading(String text,int x,int y,boolean fixedLeft){this.text=text;this.x=x;this.y=y;this.fixedLeft=fixedLeft;}
        static RubyReading parse(String value){if(value.startsWith("@_"))return new RubyReading(value.substring(2),0,0,true);if(!value.startsWith("@p"))return new RubyReading(value,0,0);int slash=value.indexOf('/');if(slash<0)throw new IllegalArgumentException("Native ruby @p requires '/': "+value);int comma=value.indexOf(',');return new RubyReading(value.substring(slash+1),nativeLong(value,2),comma<0?0:nativeLong(value,comma+1));}
        // Native @_ uses a fixed reading anchor, 73 raster pixels from the text origin.
        float left(float baseX,float baseWidth,float readingWidth,float scale){return fixedLeft?73*scale:baseX+(baseWidth-readingWidth)/2+x*scale;}
        private static int nativeLong(String value,int index){while(index<value.length()&&" \t\n\r\f\u000b".indexOf(value.charAt(index))>=0)index++;boolean negative=index<value.length()&&value.charAt(index)=='-';if(index<value.length()&&(value.charAt(index)=='+'||negative))index++;long result=0,limit=negative?2147483648L:2147483647L;while(index<value.length()){char ch=value.charAt(index++);int digit=ch>='0'&&ch<='9'?ch-'0':ch>='０'&&ch<='９'?ch-'０':-1;if(digit<0)break;if(result>(limit-digit)/10)return negative?Integer.MIN_VALUE:Integer.MAX_VALUE;result=result*10+digit;}return (int)(negative?-result:result);}
    }
    static final class NativeTextToken {
        final String base;final RubyReading ruby;final boolean newline,noWrap,italic;final Integer color;final int shadow;
        NativeTextToken(String base,RubyReading ruby,boolean newline,boolean noWrap,boolean italic,Integer color,int shadow){this.base=base;this.ruby=ruby;this.newline=newline;this.noWrap=noWrap;this.italic=italic;this.color=color;this.shadow=shadow;}
        static List<NativeTextToken> parse(String text){
            ArrayList<NativeTextToken> tokens=new ArrayList<>();boolean noWrap=false,italic=false;int index=0,shadow=0xff000000;Integer color=null;
            while(index<text.length()){
                char ch=text.charAt(index);
                if(ch=='\n'){tokens.add(new NativeTextToken("",new RubyReading("",0,0),true,false,italic,color,shadow));noWrap=false;index++;continue;}
                if(text.startsWith("@o",index)){noWrap=true;index+=2;continue;}
                if(text.startsWith("@i",index)){italic=true;index+=2;continue;}
                if(text.startsWith("@r",index)){italic=false;color=null;shadow=0xff000000;index+=2;continue;}
                // Recognized native presentation commands never become literal glyphs.
                if(text.startsWith("@z",index)){index+=2;continue;}
                if(index+2<text.length()&&text.startsWith("@a",index)&&(text.charAt(index+2)=='1'||text.charAt(index+2)=='2')){index+=3;continue;}
                if(index+2<text.length()&&text.startsWith("@c",index)&&text.charAt(index+2)>='0'&&text.charAt(index+2)<='9'){
                    int value=text.charAt(index+2)-'0';if(value<=4)color=value==0?0xffffffff:value==1?0xffff0000:0xff646464;
                    if(value==0||value>=5)shadow=value==5?0xff002142:value==6?0xffff0000:0xff000000;
                    index+=3;continue;
                }
                int close=ch=='<'?text.indexOf('>',index):-1,split=close>index?text.indexOf('|',index):-1;
                String base;RubyReading ruby=new RubyReading("",0,0);
                if(split>index&&split<close){
                    // The native compiler turns raw ruby into <rREADING>BASE</r>
                    // without recursively compiling raw @i in BASE or READING.
                    base=text.substring(index+1,split);ruby=RubyReading.parse(text.substring(split+1,close));index=close+1;
                    if(base.indexOf('\n')>=0)throw new IllegalArgumentException("Unsupported native ruby base with explicit newline");
                    if(base.contains("@o")){base=base.replace("@o","");noWrap=true;}
                }else{
                    if(close>index){String tag=text.substring(index+1,close).toLowerCase(Locale.ROOT);
                        if(tag.startsWith("/i")){italic=false;index=close+1;continue;}
                        if(tag.startsWith("i")){italic=true;index=close+1;continue;}
                        if(tag.startsWith("fr")){italic=false;color=null;shadow=0xff000000;index=close+1;continue;}
                    }
                    int end=index+Character.charCount(text.codePointAt(index));base=text.substring(index,end);index=end;
                }
                tokens.add(new NativeTextToken(base,ruby,false,noWrap,italic,color,shadow));
            }
            return tokens;
        }
    }
    private static final class Glyph {
        final String base,reading;final int start,count;final float x,y,rubyX,rubyY;final boolean italic;final Integer color;final int shadow;
        Glyph(String base,String reading,int start,float x,float y,float rubyX,float rubyY,boolean italic,Integer color,int shadow){this.color=color;this.shadow=shadow;this.italic=italic;this.base=base;this.reading=reading;this.start=start;count=base.codePointCount(0,base.length());this.x=x;this.y=y;this.rubyX=rubyX;this.rubyY=rubyY;}
    }
    private static final class Paragraph {
        final Glyph[] glyphs;final float top,height,firstBaseline;final int color;
        Paragraph(String text,float top,float width,float font,float nativeFont,int color,TextPaint paint){this.top=top;this.color=color;ArrayList<Glyph> glyphs=new ArrayList<>();paint.setTypeface(NORMAL_FACE);paint.setTextSize(font);float x=0,first=-paint.ascent()+font*.65f,y=first;int characters=0;
            // Panel offsets adapt native raster pixels to the independently sized mobile font.
            float offsetScale=font/nativeFont;
            for(NativeTextToken token:NativeTextToken.parse(text)){if(token.newline){x=0;y+=font*1.65f;continue;}String base=token.base,reading=token.ruby.text;
                paint.setTypeface(token.italic?ITALIC_FACE:NORMAL_FACE);float measured=paint.measureText(base);if(!token.noWrap&&x+measured>width&&x>0){x=0;y+=font*1.65f;}paint.setTextSize(font*.45f);float rubyX=token.ruby.left(x,measured,paint.measureText(reading),offsetScale);paint.setTextSize(font);Glyph glyph=new Glyph(base,reading,characters,x,y,rubyX,y-font*1.05f+token.ruby.y*offsetScale,token.italic,token.color,token.shadow);glyphs.add(glyph);characters+=glyph.count;x+=measured;}
            this.glyphs=glyphs.toArray(new Glyph[0]);firstBaseline=first;height=text.isEmpty()?0:y-first+font*1.65f;}
        int firstLine(float y){int lo=0,hi=glyphs.length;while(lo<hi){int mid=(lo+hi)>>>1;if(glyphs[mid].y<y)lo=mid+1;else hi=mid;}return lo;}
    }
    private static final class PanelLayout {
        final Paragraph[] paragraphs;final float width,font;final String document;
        PanelLayout(Scene scene,float width,float font,TextPaint paint){paint.setTextLocale(Locale.forLanguageTag(scene.json.optString("language","ja")));this.width=width;this.font=font;ArrayList<Paragraph> paragraphs=new ArrayList<>();StringBuilder document=new StringBuilder();float y=0;for(JSONObject item:scene.texts){String text=item.optString("text","");Paragraph paragraph=new Paragraph(text,y,width,font,(float)item.optDouble("fontSize",40),color(item.opt("color"),Color.WHITE),paint);paragraphs.add(paragraph);y+=paragraph.height;if(document.length()>0)document.append('\n');document.append(text);}this.paragraphs=paragraphs.toArray(new Paragraph[0]);this.document=document.toString();}
        float visibleHeight(Scene scene){float height=0;for(int i=0;i<paragraphs.length;i++){Paragraph paragraph=paragraphs[i];int visible=scene.texts.get(i).optInt("visibleCharacters",Integer.MAX_VALUE);for(int j=paragraph.glyphs.length-1;j>=0;j--){Glyph glyph=paragraph.glyphs[j];if(visible>glyph.start){height=Math.max(height,paragraph.top+glyph.y+font*.25f);break;}}}return height;}
        int anchor(float offset){int before=0;for(Paragraph paragraph:paragraphs){for(Glyph glyph:paragraph.glyphs)if(paragraph.top+glyph.y>=offset+font*.8f)return before+glyph.start;before+=paragraph.glyphs.length==0?0:paragraph.glyphs[paragraph.glyphs.length-1].start+paragraph.glyphs[paragraph.glyphs.length-1].count;}return before;}
        float offsetFor(int anchor){int before=0;for(Paragraph paragraph:paragraphs){for(Glyph glyph:paragraph.glyphs)if(before+glyph.start>=anchor)return Math.max(0,paragraph.top+glyph.y-paragraph.firstBaseline);before+=paragraph.glyphs.length==0?0:paragraph.glyphs[paragraph.glyphs.length-1].start+paragraph.glyphs[paragraph.glyphs.length-1].count;}return 0;}
    }
    // The same native-pixel glyph/ruby renderer is used inside the frozen GL frame.
    static Bitmap capture(Scene scene,Viewport viewport,int width,int height){return capture(scene,viewport,width,height,null);}
    static float fullscreenBackdropAlpha(Scene scene){
        if(scene.movie!=null||!scene.json.optString("scene","play").equals("play"))return 0;
        double fixed=scene.json.optDouble("windowAlphaFixed16",-1);
        return fixed>=0?(float)Math.max(0,Math.min(1,fixed/65536.)):scene.texts.isEmpty()?0:(float)scene.texts.get(0).optDouble("alpha",1);
    }
    static boolean captureInFrame(Scene source,Viewport captured,Scene target,Viewport current){
        if(captured.panel.height()<=0&&current!=null&&current.panel.height()<=0&&source.movie==null&&target!=null&&target.movie==null&&source.json.optString("scene","play").equals("play")&&target.json.optString("scene","play").equals("play"))return false;
        return captureInFrame(captured,current);
    }
    static boolean captureInFrame(Viewport captured,Viewport current){return captured.panel.height()<=0||current==null||current.panel.height()<=0;}
    static Bitmap capture(Scene scene,Viewport viewport,int width,int height,RenderState state){Bitmap bitmap=Bitmap.createBitmap(width,height,Bitmap.Config.ARGB_8888);new TextPainter().draw(new Canvas(bitmap),scene,viewport,state);return bitmap;}
    private static final Typeface NORMAL_FACE=Typeface.create("serif",Typeface.NORMAL);
    private static final Typeface ITALIC_FACE=Typeface.create("serif",Typeface.ITALIC);
    private static final class TextPainter {
    private final TextPaint paint=new TextPaint(Paint.ANTI_ALIAS_FLAG|Paint.SUBPIXEL_TEXT_FLAG);
    private LinearGradient panelGloss;private float glossTop=Float.NaN,glossBottom=Float.NaN;
    private float rubyOffsetScale=1,tailX,tailBaseline;private boolean italicFace;
    TextPainter(){paint.setTypeface(NORMAL_FACE);}
    private void style(boolean italic){if(italicFace!=italic){paint.setTypeface(italic?ITALIC_FACE:NORMAL_FACE);italicFace=italic;}}
    void draw(Canvas canvas,Scene scene,Viewport viewport,RenderState state){draw(canvas,scene,viewport,state,true);}
    void draw(Canvas canvas,Scene scene,Viewport viewport,RenderState state,boolean stationary){paint.setTextLocale(Locale.forLanguageTag(scene.json.optString("language","ja")));if(viewport.panel.height()>0){drawPanel(canvas,scene,viewport,state,stationary);return;}canvas.save();canvas.clipRect(viewport.area);float backdrop=fullscreenBackdropAlpha(scene);if(backdrop>0){paint.clearShadowLayer();paint.setShader(null);paint.setStyle(Paint.Style.FILL);paint.setColor(multiplyAlpha(0x99000000,backdrop));canvas.drawRect(viewport.area,paint);DialogueArt.frame(canvas,viewport.area,false,backdrop);}
        for(JSONObject item:scene.texts){String text=item.optString("text","");if(text.isEmpty())continue;
            float alpha=(float)item.optDouble("alpha",1);if(alpha<=0)continue;float margin=state==null?1:state.marginScale,nativeX=(float)item.optDouble("x",150),nativeWidth=(float)item.optDouble("w",1620),nativeFont=(float)item.optDouble("fontSize",40);RectF r=viewport.transform(nativeX*margin,(float)item.optDouble("y",100),nativeWidth+2*nativeX*(1-margin),(float)item.optDouble("h",850));style(false);paint.setTextSize(nativeFont*viewport.scale*(state==null?1:state.fontScale));paint.setColor(multiplyAlpha(color(item.opt("color"),Color.WHITE),alpha));paint.clearShadowLayer();rubyOffsetScale=viewport.scale;
            canvas.save();canvas.clipRect(r);canvas.translate(r.left,r.top);drawFullParagraph(canvas,text,r.width(),r.height(),nativeFont,item.optInt("visibleCharacters",Integer.MAX_VALUE),item.optInt("activeFrom",0),item.optBoolean("cursor",false),alpha,stationary);canvas.restore();
        }canvas.restore();}
    private void drawPanel(Canvas canvas,Scene scene,Viewport viewport,RenderState state,boolean stationary){float font=state==null?40*viewport.scale:state.font,padding=state==null?font*.9f:state.padding;PanelSnapshot saved=state==null?null:state.snapshot(scene);PanelLayout layout=saved==null?new PanelLayout(scene,Math.max(1,viewport.panel.width()-2*horizontalInset(viewport.panel.width(),padding,state==null?1:state.marginScale)),font,paint):saved.layout;float scroll=saved==null?Math.max(0,layout.visibleHeight(scene)-(viewport.panel.height()-2*padding)):saved.scroll;boolean cleanTop=stationary&&saved!=null&&saved.following;float horizontal=horizontalInset(viewport.panel.width(),padding,state==null?1:state.marginScale);
        paint.setTypeface(NORMAL_FACE);italicFace=false;paint.clearShadowLayer();paint.setColor(Color.WHITE);
        // One 80%-alpha fill keeps the glass highlight from raising total opacity.
        if(panelGloss==null||glossTop!=viewport.panel.top||glossBottom!=viewport.panel.bottom){glossTop=viewport.panel.top;glossBottom=viewport.panel.bottom;panelGloss=new LinearGradient(0,glossTop,0,glossBottom,new int[]{0xcc1a2435,0xcc080f1d,0xcc070e1b,0xcc0a1221},new float[]{0,.18f,.65f,1},Shader.TileMode.CLAMP);}paint.setShader(panelGloss);canvas.drawRect(viewport.panel,paint);paint.setShader(null);
        float unit=font/22,border=Math.max(1,unit*2),inset=unit*5;paint.setColor(0xff66748a);paint.setStrokeWidth(border);paint.setStyle(Paint.Style.STROKE);canvas.drawRect(new RectF(viewport.panel.left+border/2,viewport.panel.top+border/2,viewport.panel.right-border/2,viewport.panel.bottom-border/2),paint);
        paint.setColor(0xff34435c);paint.setStrokeWidth(Math.max(1,unit*.65f));canvas.drawRect(new RectF(viewport.panel.left+inset,viewport.panel.top+inset,viewport.panel.right-inset,viewport.panel.bottom-inset),paint);paint.setStyle(Paint.Style.FILL);DialogueArt.frame(canvas,new RectF(viewport.panel.left,viewport.panel.top+border,viewport.panel.right,viewport.panel.bottom-border),true,1);canvas.save();canvas.clipRect(viewport.panel.left+horizontal,viewport.panel.top+padding,viewport.panel.right-horizontal,viewport.panel.bottom-padding);canvas.translate(viewport.panel.left+horizontal,viewport.panel.top+padding-scroll);
        float lower=scroll-font,upper=scroll+viewport.panel.height()-2*padding+font*1.1f;
        for(int i=0;i<layout.paragraphs.length;i++){Paragraph paragraph=layout.paragraphs[i];if(paragraph.top+paragraph.height<lower||paragraph.top>upper)continue;float alpha=(float)scene.texts.get(i).optDouble("alpha",1);if(alpha<=0)continue;int visible=scene.texts.get(i).optInt("visibleCharacters",Integer.MAX_VALUE);style(false);paint.setTextSize(font);paint.setColor(multiplyAlpha(paragraph.color,alpha));paint.clearShadowLayer();
            float first=cleanTop?Math.max(lower,scroll-paint.ascent()):lower;
            for(int j=paragraph.firstLine(first-paragraph.top);j<paragraph.glyphs.length;j++){Glyph glyph=paragraph.glyphs[j];if(paragraph.top+glyph.y>upper)break;int count=Math.min(glyph.count,Math.max(0,visible-glyph.start));if(count<=0)continue;style(glyph.italic);outlineColor=glyph.shadow;paint.setColor(multiplyAlpha(glyph.color==null?paragraph.color:glyph.color,alpha*(glyph.start<scene.texts.get(i).optInt("activeFrom",0)?.45f:1)));outlined(canvas,visiblePrefix(glyph.base,count),glyph.x,paragraph.top+glyph.y);if(!glyph.reading.isEmpty()){paint.setTextSize(font*.45f);int reading=rubyVisibleCharacters(count,glyph.count,glyph.reading.codePointCount(0,glyph.reading.length()));if(reading>0)outlined(canvas,visiblePrefix(glyph.reading,reading),glyph.rubyX,paragraph.top+glyph.rubyY);paint.setTextSize(font);}}
            if(scene.texts.get(i).optBoolean("cursor",false)&&paragraph.glyphs.length>0){Glyph last=paragraph.glyphs[paragraph.glyphs.length-1];style(last.italic);DialogueArt.cursor(canvas,last.x+paint.measureText(last.base),paragraph.top+last.y,font,layout.width,alpha,stationary);}}
        canvas.restore();float available=viewport.panel.height()-2*padding,maximum=Math.max(0,layout.visibleHeight(scene)-available);if(maximum>1){float thumb=Math.max(font*.5f,available*available/(available+maximum)),top=viewport.panel.top+padding+(available-thumb)*scroll/maximum,left=viewport.panel.right-padding*.35f;paint.clearShadowLayer();paint.setColor(0x996c8878);canvas.drawRoundRect(left,top,left+Math.max(2,font*.07f),top+thumb,font*.035f,font*.035f,paint);}}
    private final LinkedHashMap<String,Paragraph> fullLayouts=new LinkedHashMap<>(8,.75f,true);
    private void drawFullParagraph(Canvas canvas,String text,float width,float height,float nativeFont,int visible,int activeFrom,boolean cursor,float alpha,boolean animated){
        float font=paint.getTextSize();int fill=paint.getColor();String key=paint.getTextLocale().toLanguageTag()+":"+text+"\u0000"+width+":"+font+":"+nativeFont;Paragraph paragraph=fullLayouts.get(key);
        if(paragraph==null){paragraph=new Paragraph(text,0,width,font,nativeFont,fill,paint);fullLayouts.put(key,paragraph);while(fullLayouts.size()>6)fullLayouts.remove(fullLayouts.keySet().iterator().next());}
        paint.setTypeface(NORMAL_FACE);italicFace=false;paint.setTextSize(font);float tail=0;for(Glyph glyph:paragraph.glyphs)if(visible>glyph.start)tail=glyph.y-font*.2f+font*.25f;float offset=Math.max(0,tail-height);canvas.save();canvas.translate(0,-offset-font*.2f);
        for(Glyph glyph:paragraph.glyphs){int count=Math.min(glyph.count,Math.max(0,visible-glyph.start));if(count<=0)break;if(glyph.y-offset<-font||glyph.y-offset>height+font)continue;style(glyph.italic);outlineColor=glyph.shadow;paint.setColor(multiplyAlpha(glyph.color==null?fill:multiplyAlpha(glyph.color,alpha),glyph.start<activeFrom?.45f:1));outlined(canvas,visiblePrefix(glyph.base,count),glyph.x,glyph.y);if(!glyph.reading.isEmpty()){paint.setTextSize(font*.45f);int reading=rubyVisibleCharacters(count,glyph.count,glyph.reading.codePointCount(0,glyph.reading.length()));if(reading>0)outlined(canvas,visiblePrefix(glyph.reading,reading),glyph.rubyX,glyph.rubyY);paint.setTextSize(font);}}
        if(cursor&&paragraph.glyphs.length>0){Glyph last=paragraph.glyphs[paragraph.glyphs.length-1];style(last.italic);DialogueArt.cursor(canvas,last.x+paint.measureText(last.base),last.y,font,width,alpha,animated);}canvas.restore();paint.setColor(fill);
    }
    private int outlineColor=0xff000000;
    private void outlined(Canvas canvas,String value,float x,float y){
        int fill=paint.getColor();paint.clearShadowLayer();paint.setStyle(Paint.Style.STROKE);paint.setStrokeJoin(Paint.Join.ROUND);paint.setStrokeWidth(Math.max(1,paint.getTextSize()*.065f));paint.setColor((fill&0xff000000)|(outlineColor&0xffffff));canvas.drawText(value,x,y,paint);paint.setStyle(Paint.Style.FILL);paint.setColor(fill);canvas.drawText(value,x,y,paint);
    }
    private void drawRuby(Canvas canvas,String text,float width,int visibleCharacters){drawRuby(canvas,text,width,visibleCharacters,0);}
    private void drawRuby(Canvas canvas,String text,float width,int visibleCharacters,int activeFrom){float font=paint.getTextSize(),x=0,baseline=-paint.ascent()+font*.45f;int remaining=Math.max(0,visibleCharacters),index=0,fill=paint.getColor();for(NativeTextToken token:NativeTextToken.parse(text)){
        if(token.newline){x=0;baseline+=font*1.65f;continue;}String base=token.base,reading=token.ruby.text;
        style(token.italic);float measured=paint.measureText(base);if(!token.noWrap&&x+measured>width&&x>0){x=0;baseline+=font*1.65f;}
        int baseCharacters=base.codePointCount(0,base.length()),visibleBase=Math.min(remaining,baseCharacters);outlineColor=token.shadow;paint.setColor(multiplyAlpha(token.color==null?fill:multiplyAlpha(token.color,(fill>>>24)/255f),index<activeFrom?.45f:1));
        if(visibleBase>0)outlined(canvas,visiblePrefix(base,visibleBase),x,baseline);
        if(!reading.isEmpty()){paint.setTextSize(font*.45f);float rubyWidth=paint.measureText(reading);int visibleReading=rubyVisibleCharacters(visibleBase,baseCharacters,reading.codePointCount(0,reading.length()));if(visibleReading>0)outlined(canvas,visiblePrefix(reading,visibleReading),token.ruby.left(x,measured,rubyWidth,rubyOffsetScale),baseline-font*1.05f+token.ruby.y*rubyOffsetScale);paint.setTextSize(font);}remaining=Math.max(0,remaining-baseCharacters);index+=baseCharacters;x+=measured;
    }tailX=x;tailBaseline=baseline;paint.setColor(fill);}

    }
    static String visiblePrefix(String value,int characters){int count=value.codePointCount(0,value.length());if(characters>=count)return value;if(characters<=0)return "";return value.substring(0,value.offsetByCodePoints(0,characters));}
    static int rubyVisibleCharacters(int visibleBase,int totalBase,int readingCharacters){if(visibleBase<=0||totalBase<=0||readingCharacters<=0)return 0;if(visibleBase>=totalBase)return readingCharacters;return (int)(((long)visibleBase*readingCharacters+totalBase-1)/totalBase);}
    static int multiplyAlpha(int color,float alpha){if(!Float.isFinite(alpha))throw new IllegalArgumentException("Non-finite text opacity");int a=Math.round((color>>>24)*Math.max(0,Math.min(1,alpha)));return (color&0xffffff)|(a<<24);}
    private static int color(Object value,int fallback){try{if(value instanceof Number){int c=((Number)value).intValue();return (c&0xff000000)==0?c|0xff000000:c;}if(value instanceof String)return Color.parseColor((String)value);}catch(IllegalArgumentException ignored){}return fallback;}
}
