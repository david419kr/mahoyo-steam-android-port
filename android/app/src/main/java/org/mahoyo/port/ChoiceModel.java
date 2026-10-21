package org.mahoyo.port;

import org.json.*;
import java.util.*;

/** UI ordinals never contain a VM value, variable address or branch target. */
final class ChoiceModel {
    final String revision,profile,coordinateUnits;final List<Item> items;
    static final class Item {
        final int ordinal,x,y,width,height,priority,hitMode,anchorX,anchorY,sourceWidth,sourceHeight;
        final String label,image,colorImage;final boolean enabled;
        Item(JSONObject value,boolean picture){
            ordinal=integer(value,"ordinal");Object text=value.opt("label"),enable=value.opt("enabled");if(!(text instanceof String)||!(enable instanceof Boolean))throw new IllegalArgumentException("Invalid choice label/enable");label=(String)text;enabled=(Boolean)enable;
            if(picture){image=asset(value,"image");colorImage=asset(value,"colorImage");x=integer(value,"x");y=integer(value,"y");width=positive(value,"width");height=positive(value,"height");priority=integer(value,"priority");hitMode=integer(value,"hitMode");anchorX=integer(value,"anchorX");anchorY=integer(value,"anchorY");sourceWidth=positive(value,"sourceWidth");sourceHeight=positive(value,"sourceHeight");if(!enabled||hitMode!=1||anchorX!=0||anchorY!=0)throw new IllegalArgumentException("Unsupported WIK choice profile");}
            else{image=colorImage="";x=y=width=height=priority=hitMode=anchorX=anchorY=sourceWidth=sourceHeight=0;}
        }
    }
    private ChoiceModel(JSONObject value){
        Object token=value.opt("revision");if(!(token instanceof String)||!((String)token).matches("[!-~]{1,128}"))throw new IllegalArgumentException("Invalid choice revision");revision=(String)token;profile=value.optString("profile");boolean picture=profile.equals("wik164");if(!picture&&!profile.equals("text161"))throw new IllegalArgumentException("Unsupported choice profile");if(integer(value,"initialFocus")!=-1)throw new IllegalArgumentException("Unsupported initial choice focus");coordinateUnits=value.optString("coordinateUnits");
        if(picture&&(!coordinateUnits.equals("parent-canvas-pixels")||integer(value,"canvasWidth")!=1920||integer(value,"canvasHeight")!=1080||integer(value,"monoState")!=0||integer(value,"colorState")!=1))throw new IllegalArgumentException("Unsupported WIK coordinate space");
        JSONArray rows=value.optJSONArray("items");if(rows==null||rows.length()==0||rows.length()>4096||picture&&rows.length()!=14)throw new IllegalArgumentException("Invalid choice count");ArrayList<Item> list=new ArrayList<>();HashSet<Integer> priorities=new HashSet<>();for(int i=0;i<rows.length();i++){JSONObject row=rows.optJSONObject(i);if(row==null)throw new IllegalArgumentException("Invalid choice item");Item item=new Item(row,picture);if(item.ordinal!=i)throw new IllegalArgumentException("Unordered choice ordinal");if(picture&&(item.priority<0||item.priority>=14||!priorities.add(item.priority)))throw new IllegalArgumentException("Invalid WIK priority");list.add(item);}items=Collections.unmodifiableList(list);
    }
    static ChoiceModel parse(JSONObject value){return value==null?null:new ChoiceModel(value);}
    String payload(int ordinal){if(ordinal<0||ordinal>=items.size())throw new IllegalArgumentException("Choice ordinal outside list");return revision+" "+ordinal;}
    private static int integer(JSONObject value,String name){Object field=value.opt(name);if(!(field instanceof Number))throw new IllegalArgumentException("Missing choice integer "+name);double number=((Number)field).doubleValue();if(!Double.isFinite(number)||number!=Math.rint(number)||number<Integer.MIN_VALUE||number>Integer.MAX_VALUE)throw new IllegalArgumentException("Invalid choice integer "+name);return (int)number;}
    private static int positive(JSONObject value,String name){int number=integer(value,name);if(number<=0||number>32768)throw new IllegalArgumentException("Invalid choice extent "+name);return number;}
    private static String asset(JSONObject value,String name){Object field=value.opt(name);if(!(field instanceof String))throw new IllegalArgumentException("Missing choice image");String key=(String)field;if(key.isEmpty()||!key.matches("[A-Za-z0-9_./-]+")||key.startsWith("/")||Arrays.asList(key.split("/")).contains(".."))throw new IllegalArgumentException("Invalid choice image");return key;}
    static int columns(int width,float density){return Math.max(1,Math.min(7,(int)Math.floor((width/Math.max(.1f,density)+8)/184f)));}
}
