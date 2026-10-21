package org.mahoyo.port;

import org.json.JSONObject;
import java.io.IOException;

/** A resumable staging directory is never the active game's media directory. */
final class DownloadRecord {
    static String reusable(JSONObject value,String activeSource){
        if(value==null||value.optInt("schema")!=1||value.optInt("appId")!=InstallationRecord.APP_ID)return null;
        String source=value.optString("source");
        return source.matches("steam-download-[a-f0-9-]{36}")&&!source.equals(activeSource)?source:null;
    }
    static JSONObject create(String source)throws Exception{
        JSONObject value=new JSONObject().put("schema",1).put("appId",InstallationRecord.APP_ID).put("source",source);
        if(reusable(value,"")==null)throw new IOException("Invalid download staging directory");
        return value;
    }
    private DownloadRecord(){}
}
