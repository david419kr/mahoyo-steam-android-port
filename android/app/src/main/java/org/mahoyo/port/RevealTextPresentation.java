package org.mahoyo.port;

import org.json.*;

/** A delayed image commit can still present its own reveal step, never a different page or job. */
final class RevealTextPresentation {
    static boolean compatible(JSONObject latest,JSONObject rendered){
        if(!normalInput(latest)||!normalInput(rendered))return false;
        if(rendered.optLong("version",-1)<0||rendered.optLong("version")>latest.optLong("version",-1))return false;
        for(String key:new String[]{"runtimeEpoch","language","module","modulePc","instructions","textLine","windowMode"})
            if(!latest.has(key)||!rendered.has(key)||!latest.optString(key).equals(rendered.optString(key)))return false;
        try{
            JSONArray a=latest.optJSONArray("text"),b=rendered.optJSONArray("text");
            if(a==null||b==null||a.length()==0||a.length()!=b.length())return false;
            for(int i=0;i<a.length();i++){
                JSONObject x=new JSONObject(a.getJSONObject(i).toString()),y=new JSONObject(b.getJSONObject(i).toString());
                x.remove("visibleCharacters");x.remove("cursor");y.remove("visibleCharacters");y.remove("cursor");
                if(!x.toString().equals(y.toString()))return false;
            }
            return true;
        }catch(JSONException invalid){return false;}
    }
    private static boolean normalInput(JSONObject value){
        if(value==null||!"play".equals(value.optString("scene"))||!"input".equals(value.optString("waiting"))
            ||!value.optString("error","").isEmpty())return false;
        for(String key:new String[]{"movie","transition","choice","modal","archive","gallery","musicRoom"})
            if(value.optJSONObject(key)!=null)return false;
        return true;
    }
    private RevealTextPresentation(){}
}
