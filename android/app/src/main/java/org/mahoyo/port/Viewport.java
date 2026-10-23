package org.mahoyo.port;

import android.graphics.RectF;

final class Viewport {
    final RectF area;
    final RectF content,panel;
    final RectF frame;
    final RectF[] letterboxes;
    final float scale,x,y;
    private final Viewport normalizedSource;
    private final RectF normalizationArea;
    private final boolean surfaceNormalized;
    final int width,height;private final int bar;private final boolean menu,dynamic;private final float designWidth,designHeight;
    Viewport(int width,int height,int bar,boolean menu,boolean dynamic,float designWidth,float designHeight){
        this(width,height,bar,menu,dynamic,designWidth,designHeight,0);
    }
    Viewport(int width,int height,int bar,boolean menu,boolean dynamic,float designWidth,float designHeight,int panelHeight){
        normalizedSource=null;normalizationArea=null;surfaceNormalized=false;
        this.width=width;this.height=height;this.bar=bar;this.menu=menu;this.dynamic=dynamic;this.designWidth=designWidth;this.designHeight=designHeight;
        boolean rails=width>height*16f/9f;content=new RectF(menu&&rails?bar:0,0,width-(menu&&rails?bar:0),height-(menu&&!rails?bar:0));
        int reserved=Math.max(0,Math.min(panelHeight,Math.round(content.height())-1));panel=new RectF(content.left,content.bottom-reserved,content.right,content.bottom);area=new RectF(content.left,content.top,content.right,panel.top);
        scale=Math.min(area.width()/designWidth,area.height()/designHeight);x=area.centerX()-designWidth*scale/2;y=reserved>0?area.bottom-designHeight*scale:area.centerY()-designHeight*scale/2;
        frame=transform(0,0,designWidth,designHeight);
        letterboxes=new RectF[]{new RectF(area.left,area.top,area.right,frame.top),new RectF(area.left,frame.bottom,area.right,area.bottom),new RectF(area.left,frame.top,frame.left,frame.bottom),new RectF(frame.right,frame.top,area.right,frame.bottom)};
    }
    /** Keep the existing dynamic composition, then normalize its content to a native capture grid. */
    private Viewport(Viewport source,int width,int height,boolean surfaceNormalized){
        normalizedSource=source;this.surfaceNormalized=surfaceNormalized;normalizationArea=surfaceNormalized?new RectF(0,0,source.width,source.height):source.content;this.width=width;this.height=height;bar=0;menu=false;dynamic=source.dynamic;designWidth=source.designWidth;designHeight=source.designHeight;
        content=normalized(source.content);area=normalized(source.area);panel=normalized(source.panel);frame=normalized(source.frame);
        scale=source.scale*Math.min(width/normalizationArea.width(),height/normalizationArea.height());x=frame.left;y=frame.top;
        letterboxes=new RectF[source.letterboxes.length];for(int i=0;i<letterboxes.length;i++)letterboxes[i]=normalized(source.letterboxes[i]);
    }
    Viewport normalizedContent(int width,int height){if(width<=0||height<=0||content.width()<=0||content.height()<=0)throw new IllegalArgumentException("Invalid content capture extent");return new Viewport(this,width,height,false);}
    Viewport scaledSurface(int width,int height){if(width<1||height<1||this.width==width&&this.height==height)return this;return new Viewport(surfaceLayout(),width,height,true);}
    Viewport surfaceLayout(){return surfaceNormalized?normalizedSource.surfaceLayout():this;}
    private RectF normalized(RectF value){return new RectF((value.left-normalizationArea.left)*width/normalizationArea.width(),(value.top-normalizationArea.top)*height/normalizationArea.height(),(value.right-normalizationArea.left)*width/normalizationArea.width(),(value.bottom-normalizationArea.top)*height/normalizationArea.height());}
    Viewport withPanel(int height){if(normalizedSource!=null)return new Viewport(normalizedSource.withPanel(height),width,this.height,surfaceNormalized);return Math.round(panel.height())==height?this:new Viewport(width,this.height,bar,menu,dynamic,designWidth,designHeight,height);}
    RectF clip(boolean background,boolean cover,boolean dynamic){if(normalizedSource!=null)return normalized(normalizedSource.clip(background,cover,dynamic));return dynamic&&(!background||cover)?area:frame;}
    boolean hasLetterboxes(){return frame.width()<area.width()-.5f||frame.height()<area.height()-.5f;}
    RectF transform(float left,float top,float width,float height){if(normalizedSource!=null)return normalized(normalizedSource.transform(left,top,width,height));return new RectF(x+left*scale,y+top*scale,x+(left+width)*scale,y+(top+height)*scale);}
    RectF image(float left,float top,float width,float height,boolean background,boolean cover,boolean dynamic,float designWidth,float designHeight){
        if(normalizedSource!=null)return normalized(normalizedSource.image(left,top,width,height,background,cover,dynamic,designWidth,designHeight));
        if(!background||!dynamic)return transform(left,top,width,height);
        float ratio=cover?Math.max(area.width()/designWidth,area.height()/designHeight):Math.min(area.width()/designWidth,area.height()/designHeight);
        float centerY=!cover&&panel.height()>0?area.bottom-designHeight*ratio/2:area.centerY();
        return new RectF(area.centerX()+(left-designWidth/2)*ratio,centerY+(top-designHeight/2)*ratio,area.centerX()+(left+width-designWidth/2)*ratio,centerY+(top+height-designHeight/2)*ratio);
    }
}
