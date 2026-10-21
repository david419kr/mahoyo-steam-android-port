package org.mahoyo.port;

import android.content.Context;
import android.content.SharedPreferences;

final class DisplayPrefs {
    final SharedPreferences values;
    DisplayPrefs(Context context) { values=context.getSharedPreferences("display",Context.MODE_PRIVATE); }
    boolean dynamic(){return values.getBoolean("dynamic",true);}
    boolean cover(boolean event){return values.getBoolean(event?"eventCover":"bgCover",true);}
    boolean ambient(){return values.getBoolean("ambient",true);}
    int filter(){return values.getInt("filter",0);}
    int dialogueMode(){return Math.max(0,Math.min(2,values.getInt("dialogue",2)));}
    boolean separateDialogue(){return dialogueMode()==1;}
    // A 10:7 or taller usable area leaves a practical detached reading panel.
    boolean separateDialogue(int width,int height){return dialogueMode()==1 || dialogueMode()==2 && width>0 && height>0 && height>=width*.70f;}
    int fontPercent(){return Math.max(75,Math.min(150,values.getInt("fontPercent",100)));}
    int marginPercent(){return Math.max(0,Math.min(160,values.getInt("marginPercent",100)));}
    float dialogueScale(){float value=values.getFloat("dialogueScale",1f);return Float.isFinite(value)?Math.max(.6f,value):1f;}
    int volume(String channel){return values.getInt("volume_"+channel,100);}
    int textSpeed(){int speed=values.getInt("textSpeed",40);return speed==0?0:Math.max(10,Math.min(100,speed));}
    int autoDelay(){return Math.max(500,Math.min(10000,values.getInt("autoDelay",2400)));}
    boolean menuVisible(int width,int height,int bar){
        if(width<=0||height<=0)return false;
        return width-2*bar>=height*16f/9f || height-bar>=width*9f/16f+height*.23f;
    }
}
