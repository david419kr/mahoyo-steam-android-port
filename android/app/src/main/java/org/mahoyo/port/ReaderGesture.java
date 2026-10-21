package org.mahoyo.port;

/** Axis-locked pull gestures consume panel scroll before revealing their screen. */
final class ReaderGesture {
    static final int NONE=0,HISTORY=1,SETTINGS=2,SAVE=3,LOAD=4,PULL=5;
    private float x,y,top,bottom,slop,density,pull;private int action;private final DragIntent intent=new DragIntent();
    private boolean eligible,claimed,holdEligible;
    void start(float x,float y,float density,float top,float bottom,boolean eligible){this.x=x;this.y=y;this.density=density;slop=25*density;this.top=Math.max(0,top);this.bottom=Math.max(0,bottom);this.eligible=eligible;claimed=false;holdEligible=eligible;action=NONE;pull=0;}
    boolean pulling(){return action!=NONE;}
    int action(){return action;}
    private float peek(){return 40*density;}
    float reveal(){return pulling()?Math.max(0,pull-peek()+16*density):0;}
    boolean committed(){return pulling()&&reveal()>0&&intent.forward();}
    private float distance(float dx,float dy){return action==HISTORY?dy-top:action==SETTINGS?-dy-bottom:action==SAVE?dx:-dx;}
    int move(float x,float y){
        if(!eligible)return NONE;float dx=x-this.x,dy=y-this.y;
        if(pulling()){pull=Math.max(0,distance(dx,dy));intent.move(pull);return PULL;}
        if(claimed)return NONE;
        if(Math.hypot(dx,dy)>slop)holdEligible=false;
        if(Math.abs(dy)>Math.abs(dx)*1.5f){if(dy>=peek()+top)action=HISTORY;else if(-dy>=peek()+bottom)action=SETTINGS;}
        else if(Math.abs(dx)>Math.abs(dy)*1.5f&&Math.abs(dx)>=peek())action=dx>0?SAVE:LOAD;
        if(pulling()){pull=Math.max(0,distance(dx,dy));intent.start(pull,density);claimed=true;holdEligible=false;return PULL;}return NONE;
    }
    static float[] offset(int action,float reveal,float width,float height){float extent=action==SAVE||action==LOAD?width:height;float remaining=Math.max(0,extent-Math.max(0,reveal));return new float[]{action==SAVE?-remaining:action==LOAD?remaining:0,action==HISTORY?-remaining:action==SETTINGS?remaining:0};}
    boolean longPress(){if(!eligible||claimed||!holdEligible)return false;claimed=true;holdEligible=false;return true;}
    void cancel(){eligible=claimed=holdEligible=false;action=NONE;pull=0;}
    boolean claimed(){return claimed;}
}
