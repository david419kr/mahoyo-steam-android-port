package org.mahoyo.port;

import org.json.*;
import java.util.*;

final class Scene {
    final JSONObject json;
    final List<JSONObject> layers=new ArrayList<>(),texts=new ArrayList<>(),actions=new ArrayList<>();
    final List<JSONObject> preloads=new ArrayList<>();
    final String preloadKey;
    final float width,height;
    final Transition transition;
    final JSONObject movie;
    final GlobalScreenWave.Spec screenWave;
    final ParticleFrame particles;
    Scene(String source)throws JSONException{
        json=new JSONObject(source);width=(float)json.optDouble("designWidth",1920);height=(float)json.optDouble("designHeight",1080);movie=json.optJSONObject("movie");screenWave=GlobalScreenWave.Spec.from(json.optJSONObject("screenWave"));
        particles=ParticleFrame.from(json.optJSONObject("particles"));
        collect(json.optJSONArray("layers"),layers);collect(json.optJSONArray("actions"),actions);
        collect(json.optJSONArray("preloads"),preloads);preloadKey=warmKey(preloads);
        Object text=json.opt("text");if(text instanceof JSONArray)collect((JSONArray)text,texts);else if(text instanceof String&&!((String)text).isEmpty()){JSONObject t=new JSONObject();t.put("text",text);t.put("x",150);t.put("y",100);t.put("w",1620);t.put("h",850);t.put("fontSize",40);texts.add(t);}else if(text instanceof JSONObject)texts.add((JSONObject)text);
        layers.sort(Comparator.comparingDouble(v->v.optDouble("z",0)));
        JSONObject pending=json.optJSONObject("transition");transition=pending==null?null:new Transition(pending);
    }
    // Color/tag changes share the same decoded pixels in this GL renderer.
    static String warmKey(List<JSONObject> preloads){TreeSet<String> hints=new TreeSet<>();for(JSONObject hint:preloads){String kind=hint.optString("kind",""),base=hint.optString("base",hint.optString("name",""));if(base.isEmpty())continue;if(kind.equals("image"))hints.add(kind+":"+base+":"+hint.optInt("variantX",0)+":"+hint.optInt("variantY",0));else if(kind.equals("range"))hints.add(kind+":"+base+":"+hint.optInt("xMin",0)+":"+hint.optInt("yMin",0)+":"+hint.optInt("xMax",0)+":"+hint.optInt("yMax",0));}return hints.toString();}
    static final class Transition {
        final String id,mask;final int mode,maskId,strength,progress256;final float oldOpacity;final Scene old;
        Transition(JSONObject value)throws JSONException{
            id=value.optString("id","");mode=value.optInt("mode",0);oldOpacity=(float)Math.max(0,Math.min(1,value.optDouble("oldOpacity",1)));
            mask=value.optString("mask","");maskId=value.optInt("maskId",0);strength=value.optInt("strength",256);progress256=value.optInt("progress256",0);
            JSONObject previous=value.optJSONObject("old");old=previous==null?null:new Scene(previous.toString());
        }
    }
    private static void collect(JSONArray array,List<JSONObject> list){if(array!=null)for(int i=0;i<array.length();i++){JSONObject item=array.optJSONObject(i);if(item!=null)list.add(item);}}
}
