package org.mahoyo.port;

import android.graphics.*;
import android.graphics.drawable.Drawable;

/** Resolution-independent menu symbols in a shared 24-unit coordinate system. */
final class IconDrawable extends Drawable {
    private final String id;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    IconDrawable(String id){this.id=id;paint.setColor(0xffe9f0e9);paint.setStrokeWidth(1.7f);paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);}
    @Override public void draw(Canvas canvas){canvas.save();canvas.translate(getBounds().left,getBounds().top);canvas.scale(getBounds().width()/24f,getBounds().height()/24f);paint.setStyle(Paint.Style.STROKE);Path p=new Path();
        switch(id){
            case "previous":p.moveTo(18,5);p.lineTo(8,12);p.lineTo(18,19);p.close();canvas.drawPath(p,paint);canvas.drawLine(5,5,5,19,paint);break;
            case "stop":canvas.drawRoundRect(5,5,19,19,1,1,paint);break;
            case "repeat":p.moveTo(5,8);p.lineTo(18,8);p.lineTo(15,5);p.moveTo(18,8);p.lineTo(15,11);p.moveTo(19,16);p.lineTo(6,16);p.lineTo(9,19);p.moveTo(6,16);p.lineTo(9,13);canvas.drawPath(p,paint);break;
            case "fit":for(int x:new int[]{4,20})for(int y:new int[]{4,20}){int sx=x==4?1:-1,sy=y==4?1:-1;p.moveTo(x,y+5*sy);p.lineTo(x,y);p.lineTo(x+5*sx,y);}canvas.drawPath(p,paint);break;
            case "help":canvas.drawCircle(12,12,9,paint);p.moveTo(9,8);p.cubicTo(9,4.5f,16,4.5f,16,8);p.cubicTo(16,10.5f,12,10.5f,12,14);canvas.drawPath(p,paint);canvas.drawCircle(12,17,.6f,paint);break;
            case "lock":canvas.drawRoundRect(5,10,19,21,2,2,paint);canvas.drawArc(8,2,16,16,180,180,false,paint);canvas.drawLine(12,14,12,17,paint);break;
            case "sound":p.moveTo(3,9);p.lineTo(7,9);p.lineTo(12,5);p.lineTo(12,19);p.lineTo(7,15);p.lineTo(3,15);p.close();canvas.drawPath(p,paint);canvas.drawArc(10,5,23,19,-65,130,false,paint);canvas.drawArc(12,8,20,16,-65,130,false,paint);break;
            case "back":canvas.drawLine(4,12,20,12,paint);p.moveTo(11,5);p.lineTo(4,12);p.lineTo(11,19);canvas.drawPath(p,paint);break;
            case "archive":p.moveTo(12,5);p.cubicTo(8,2,5,3,3,4);p.lineTo(3,20);p.cubicTo(6,18,9,18,12,21);p.cubicTo(15,18,18,18,21,20);p.lineTo(21,4);p.cubicTo(18,3,15,2,12,5);p.lineTo(12,21);canvas.drawPath(p,paint);break;
            case "next":canvas.drawLine(4,12,20,12,paint);p.moveTo(13,5);p.lineTo(20,12);p.lineTo(13,19);canvas.drawPath(p,paint);break;
            case "save":case "load":canvas.drawLine(12,3,12,16,paint);p.moveTo(7,id.equals("save")?11:8);p.lineTo(12,id.equals("save")?16:3);p.lineTo(17,id.equals("save")?11:8);canvas.drawPath(p,paint);p.reset();p.moveTo(4,16);p.lineTo(4,21);p.lineTo(20,21);p.lineTo(20,16);canvas.drawPath(p,paint);break;
            case "history":for(int y=6;y<=18;y+=6){canvas.drawCircle(4,y,.7f,paint);canvas.drawLine(9,y,21,y,paint);}break;
            case "auto":p.moveTo(7,4);p.lineTo(20,12);p.lineTo(7,20);p.close();canvas.drawPath(p,paint);break;
            case "pause":canvas.drawLine(8,5,8,19,paint);canvas.drawLine(16,5,16,19,paint);break;
            case "skip":for(int x=3;x<=12;x+=9){p.reset();p.moveTo(x,5);p.lineTo(x+8,12);p.lineTo(x,19);p.close();canvas.drawPath(p,paint);}break;
            case "hide":case "show":p.moveTo(2,12);p.cubicTo(8,3,16,3,22,12);p.cubicTo(16,21,8,21,2,12);canvas.drawPath(p,paint);canvas.drawCircle(12,12,3,paint);if(id.equals("hide"))canvas.drawLine(3,3,21,21,paint);break;
            case "title":p.moveTo(3,11);p.lineTo(12,3);p.lineTo(21,11);p.moveTo(5,10);p.lineTo(5,21);p.lineTo(19,21);p.lineTo(19,10);p.moveTo(10,21);p.lineTo(10,15);p.lineTo(14,15);p.lineTo(14,21);canvas.drawPath(p,paint);break;
            case "settings":for(int i=0;i<32;i++){double a=(i-1)*Math.PI/16;double radius=i%4==0||i%4==3?8.2:10.5;float x=12+(float)(Math.cos(a)*radius),y=12+(float)(Math.sin(a)*radius);if(i==0)p.moveTo(x,y);else p.lineTo(x,y);}p.close();canvas.drawPath(p,paint);canvas.drawCircle(12,12,3.3f,paint);break;
        }canvas.restore();}
    @Override public void setAlpha(int alpha){paint.setAlpha(alpha);invalidateSelf();}
    @Override public void setColorFilter(ColorFilter filter){paint.setColorFilter(filter);invalidateSelf();}
    @Override public int getOpacity(){return PixelFormat.TRANSLUCENT;}
}
