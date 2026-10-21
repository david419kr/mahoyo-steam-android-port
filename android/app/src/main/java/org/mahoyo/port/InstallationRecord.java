package org.mahoyo.port;
import org.json.JSONObject;
import java.io.IOException;
/** Non-expiring, local state. Unknown session-token fields never affect playback. */
final class InstallationRecord {
 static final int APP_ID=2052410;
 static JSONObject empty()throws Exception{return new JSONObject().put("schema",1).put("appId",APP_ID).put("owned",false);}
 static JSONObject validate(JSONObject value)throws Exception{
  if(value.getInt("schema")!=1||value.getInt("appId")!=APP_ID)throw new IOException("Unsupported local installation record");
  if(value.optBoolean("ready")){if(!value.optBoolean("owned"))throw new IOException("Installation is not activated");leaf(value.getString("root"));String edition=value.getString("edition"),backend=value.getString("backend");if(!edition.equals("vanilla")&&!edition.equals("korean")||!backend.equals("steam"))throw new IOException("Invalid installed content profile");String archive=value.optString("archive");if(!archive.isEmpty())throw new IOException("Unsupported installed content profile");}
  return value;
 }
 static boolean owned(JSONObject value)throws Exception{return validate(value).optBoolean("owned");}
 static boolean ready(JSONObject value)throws Exception{return owned(value)&&value.optBoolean("ready");}
 static JSONObject confirm(JSONObject value,String steamId)throws Exception{validate(value);if(steamId==null||!steamId.matches("[0-9]{16,20}"))throw new IOException("Invalid Steam identity");return new JSONObject(value.toString()).put("owned",true).put("steamId",steamId);}
 static JSONObject activate(JSONObject value,String root,String edition)throws Exception{if(!owned(value))throw new IOException("Steam ownership has not been confirmed");JSONObject result=new JSONObject(value.toString()).put("root",root).put("edition",edition).put("backend","steam").put("ready",true);result.remove("archive");return validate(result);}
 static void leaf(String name)throws IOException{if(name==null||name.isEmpty()||name.equals(".")||name.equals("..")||!name.matches("[a-zA-Z0-9_.-]+"))throw new IOException("Unsafe installation path");}
 private InstallationRecord(){}
}
