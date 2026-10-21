package org.mahoyo.port;

/** Scroll first, then pull the menu back toward its entry edge. */
final class MenuCloseGesture {
    private float x,y,density,drag;private int action;private boolean active,claimed;
    private final DragIntent intent=new DragIntent();
    void start(float x,float y,float density,int action,boolean eligible){this.x=x;this.y=y;this.density=density;this.action=action;active=eligible;claimed=false;drag=0;}
    boolean move(float x,float y,boolean atEdge){
        if(!active)return false;
        if(!claimed&&!atEdge){this.x=x;this.y=y;return false;}
        float dx=x-this.x,dy=y-this.y,value=action==ReaderGesture.SETTINGS?dy:action==ReaderGesture.HISTORY?-dy:action==ReaderGesture.SAVE?-dx:dx;
        float cross=action==ReaderGesture.SAVE||action==ReaderGesture.LOAD?Math.abs(dy):Math.abs(dx);
        if(!claimed){if(value<40*density||value<cross*1.5f)return false;claimed=true;intent.start(value,density);}
        else intent.move(value);
        drag=Math.max(0,value);return true;
    }
    float distance(){return drag;}
    boolean claimed(){return claimed;}
    boolean committed(){return claimed&&drag>0&&intent.forward();}
    void cancel(){active=claimed=false;drag=0;}
}
