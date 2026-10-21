package org.mahoyo.port;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** A transparent input shield with only the intermission's supported actions. */
final class TeatimeControlsView extends FrameLayout {
    static final class Action {
        final String id,label;final boolean enabled;final float alpha;
        Action(String id,String label,boolean enabled){this(id,label,enabled,1);}
        Action(String id,String label,boolean enabled,float alpha){this.id=id;this.label=label;this.enabled=enabled;this.alpha=Math.max(0,Math.min(1,alpha));}
    }
    static final class Model {
        final String id,phase,theme,returnAction,backAction,initialFocus;final long revision;final float alpha;final List<Action> actions;
        Model(String id,long revision,String phase,String theme,float alpha,List<Action> actions,String returnAction,String backAction,String initialFocus){this.id=id;this.revision=revision;this.phase=phase;this.theme=theme;this.alpha=Math.max(0,Math.min(1,alpha));this.actions=Collections.unmodifiableList(new ArrayList<>(actions));this.returnAction=returnAction;this.backAction=backAction;this.initialFocus=initialFocus;}
        static Model parse(JSONObject value){if(value==null||!"teatime".equals(value.optString("kind","")))return null;String id=value.optString("id","");if(id.isEmpty()||id.indexOf(' ')>=0)throw new IllegalArgumentException("Invalid TEATIME generation");String phase=value.optString("phase","");if(!Arrays.asList("opening","ready","saving","closing").contains(phase))throw new IllegalArgumentException("Unsupported TEATIME phase "+phase);ArrayList<Action> actions=new ArrayList<>();JSONArray list=value.optJSONArray("actions");if(list!=null)for(int i=0;i<list.length();i++){JSONObject action=list.optJSONObject(i);if(action==null)continue;String key=action.optString("id","");if(Arrays.asList("archive","save","title","next").contains(key))actions.add(new Action(key,action.optString("label",key),action.optBoolean("enabled",false),(float)action.optDouble("alpha",1)));}Object back=value.opt("backAction");String backId=back instanceof JSONObject?((JSONObject)back).optString("id",""):back instanceof String?(String)back:"";return new Model(id,value.optLong("revision",0),phase,value.optString("theme","black"),(float)value.optDouble("alpha",1),actions,value.optString("returnAction",""),backId,value.optString("initialFocusAction","save"));}
        String actionKey(){StringBuilder key=new StringBuilder();for(Action action:actions)key.append(action.id).append(';');return key.toString();}
    }
    static final class Request {
        final String id,action;final long revision;
        Request(String id,long revision,String action){this.id=id;this.revision=revision;this.action=action;}
        String payload(){return id+" "+revision+" "+action;}
    }
    static final class Session {
        Model model;private boolean busy;private String saveKey="",returnedKey="";
        void update(Model next){if(next==null||model==null||!next.id.equals(model.id)||next.revision!=model.revision||!next.phase.equals(model.phase))busy=false;if(next==null||!next.phase.equals("saving")||model!=null&&!next.id.equals(model.id)){saveKey=returnedKey="";}model=next;}
        boolean active(){return model!=null;}
        boolean enabled(Action action){return model!=null&&model.phase.equals("ready")&&!busy&&action.enabled;}
        Request press(String id){if(model==null)return null;for(Action action:model.actions)if(action.id.equals(id)&&enabled(action)){busy=true;return new Request(model.id,model.revision,id);}return null;}
        Request back(){if(model==null||!model.phase.equals("ready")||busy||model.backAction.isEmpty())return null;busy=true;return new Request(model.id,model.revision,model.backAction);}
        void complete(Request request){if(model!=null&&model.id.equals(request.id)&&model.revision==request.revision)busy=false;}
        String takeSave(){if(model==null||!model.phase.equals("saving")||!saveKey.isEmpty())return null;saveKey=model.id+":"+model.revision;return saveKey;}
        Request finishSave(String key){if(key==null||model==null||!model.phase.equals("saving")||!key.equals(saveKey)||key.equals(returnedKey)||!key.startsWith(model.id+":")||model.returnAction.isEmpty())return null;returnedKey=key;return new Request(model.id,model.revision,model.returnAction);}
        boolean saving(String id){return model!=null&&model.id.equals(id)&&model.phase.equals("saving");}
    }
    static final class Geometry {
        private String key="";
        boolean changed(int width,int height,int bar,boolean rails,float density,float scaledDensity,Model model){String next=width+":"+height+":"+bar+":"+rails+":"+Float.floatToIntBits(density)+":"+Float.floatToIntBits(scaledDensity)+":"+model.theme+":"+model.actionKey();if(key.equals(next))return false;key=next;return true;}
    }
    static boolean reserve(boolean active,boolean menuPreference){return active||menuPreference;}
    static final class TouchGate {
        private boolean multiple,cancel;
        boolean consume(int action,int count){cancel=false;if(action==MotionEvent.ACTION_DOWN)multiple=false;if(count>1||action==MotionEvent.ACTION_POINTER_DOWN){cancel=!multiple;multiple=true;}boolean consume=multiple;if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_CANCEL)multiple=false;return consume;}
        boolean cancelChild(){return cancel;}
    }
    private final Consumer<Request> listener;
    private final Session session=new Session();
    private final Geometry geometry=new Geometry();
    private final TouchGate touches=new TouchGate();
    private final Map<String,ActionButton> buttons=new LinkedHashMap<>();
    private final List<View> strips=new ArrayList<>();
    private String focusedGeneration="";
    TeatimeControlsView(Context context,Consumer<Request> listener){super(context);this.listener=listener;setClickable(true);setFocusable(false);setVisibility(GONE);}
    boolean active(){return session.active();}
    void state(JSONObject modal){session.update(Model.parse(modal));setVisibility(active()?VISIBLE:GONE);refresh();}
    boolean saving(String id){return session.saving(id);}
    String takeSave(){return session.takeSave();}
    String generation(){return session.model==null?"":session.model.id;}
    Request finishSave(String key){return session.finishSave(key);}
    void complete(Request request){session.complete(request);refresh();}
    void back(){Request request=session.back();if(request!=null){refresh();listener.accept(request);}}
    void layout(int width,int height,int bar,boolean rails){if(!active())return;Model model=session.model;float density=getResources().getDisplayMetrics().density,scaledDensity=getResources().getDisplayMetrics().scaledDensity;
        if(geometry.changed(width,height,bar,rails,density,scaledDensity,model)){removeAllViews();buttons.clear();strips.clear();int groups=rails?2:1,split=(model.actions.size()+1)/2;boolean white=model.theme.equals("black");
            for(int group=0;group<groups;group++){LinearLayout strip=new LinearLayout(getContext());strip.setOrientation(rails?LinearLayout.VERTICAL:LinearLayout.HORIZONTAL);strip.setGravity(Gravity.CENTER);strip.setPadding(dp(rails?7:4),dp(4),dp(rails?7:4),dp(4));strip.setClickable(true);GradientDrawable panel=shape(white?0xcc101e33:0xcc101e33,rails?14:0);strip.setBackground(panel);FrameLayout.LayoutParams position=new FrameLayout.LayoutParams(rails?bar:-1,rails?-1:bar);position.gravity=rails?(group==0?Gravity.LEFT:Gravity.RIGHT):Gravity.BOTTOM;addView(strip,position);strips.add(strip);
                int from=rails?(group==0?0:split):0,to=rails?(group==0?split:model.actions.size()):model.actions.size();for(int i=from;i<to;i++){Action action=model.actions.get(i);ActionButton button=new ActionButton(action.id,rails,white);buttons.put(action.id,button);LinearLayout.LayoutParams p=rails?new LinearLayout.LayoutParams(-1,Math.min(dp(78),Math.max(dp(48),(height-dp(32))/Math.max(1,to-from)))):new LinearLayout.LayoutParams(0,-1,1);if(rails)p.setMargins(0,dp(4),0,dp(4));else p.setMargins(dp(3),0,dp(3),0);strip.addView(button,p);button.setOnClickListener(v->{Request request=session.press(action.id);if(request!=null){refresh();listener.accept(request);}});}}
        }refresh();if(model.phase.equals("ready")&&!model.id.equals(focusedGeneration)){focusedGeneration=model.id;ActionButton focus=buttons.get(model.initialFocus);if(focus!=null)focus.requestFocus();}}
    private void refresh(){Model model=session.model;if(model==null)return;for(View strip:strips)strip.setAlpha(model.alpha);for(Action action:model.actions){ActionButton button=buttons.get(action.id);if(button!=null){button.setEnabled(session.enabled(action));button.setAlpha(action.alpha*(model.phase.equals("ready")&&!action.enabled?.58f:1));if(!UiText.text(getContext(),action.label).contentEquals(button.label.getText()))button.label.setText(UiText.text(getContext(),action.label));button.setContentDescription(UiText.text(getContext(),action.label));}}}
    @Override public boolean dispatchTouchEvent(MotionEvent event){if(touches.consume(event.getActionMasked(),event.getPointerCount())){if(touches.cancelChild()){MotionEvent cancel=MotionEvent.obtain(event);cancel.setAction(MotionEvent.ACTION_CANCEL);super.dispatchTouchEvent(cancel);cancel.recycle();}return true;}return super.dispatchTouchEvent(event);}
    private final class ActionButton extends LinearLayout {
        final TextView label;
        ActionButton(String id,boolean rails,boolean white){super(TeatimeControlsView.this.getContext());setOrientation(VERTICAL);setGravity(Gravity.CENTER);setClickable(true);setFocusable(true);setDefaultFocusHighlightEnabled(false);int ink=white?0xffe5eefb:0xffdceaff;StateListDrawable colors=new StateListDrawable();colors.addState(new int[]{android.R.attr.state_focused},shape(white?0xcc355f88:0xcc355f88,12));colors.addState(new int[]{},shape(white?0x402c4668:0x402c4668,12));setBackground(new RippleDrawable(ColorStateList.valueOf(white?0x338dbbe9:0x3fa4c8ee),colors,shape(0xffffffff,12)));ImageView icon=new ImageView(getContext());icon.setImageDrawable(new IconDrawable(id));icon.setImageTintList(ColorStateList.valueOf(ink));icon.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);addView(icon,new LinearLayout.LayoutParams(dp(rails?24:21),dp(rails?24:21)));label=new TextView(getContext());label.setTextColor(ink);label.setTextSize(rails?11:12);label.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));label.setSingleLine(true);label.setGravity(Gravity.CENTER);label.setIncludeFontPadding(false);label.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.topMargin=dp(4);addView(label,p);setAccessibilityDelegate(new AccessibilityDelegate(){@Override public void onInitializeAccessibilityNodeInfo(View host,android.view.accessibility.AccessibilityNodeInfo info){super.onInitializeAccessibilityNodeInfo(host,info);info.setClassName(Button.class.getName());}});}
    }
    private GradientDrawable shape(int color,float radius){GradientDrawable shape=new GradientDrawable();shape.setColor(color);shape.setCornerRadius(dp(radius));return shape;}
    private int dp(float value){return Math.round(value*getResources().getDisplayMetrics().density);}
}
