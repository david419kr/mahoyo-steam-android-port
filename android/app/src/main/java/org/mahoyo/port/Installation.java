package org.mahoyo.port;
import android.content.Context;
import android.util.AtomicFile;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
/** Local, non-expiring entitlement. Playback never consults a Steam token or clock. */
final class Installation {
 static final int APP_ID=InstallationRecord.APP_ID;
 private final Context context;
 Installation(Context context){this.context=context.getApplicationContext();}
 private AtomicFile file(){return new AtomicFile(new File(context.getFilesDir(),"installation.json"));}
 synchronized JSONObject read()throws Exception{
  if(!file().getBaseFile().isFile()&&!new File(file().getBaseFile()+".bak").isFile())return InstallationRecord.empty();
  byte[] bytes=SteamResources.read(file().openRead(),16384);JSONObject record=new JSONObject(new String(bytes,StandardCharsets.UTF_8));
  return InstallationRecord.validate(record);
 }
 private void save(JSONObject value)throws Exception{AtomicFile target=file();FileOutputStream stream=null;try{stream=target.startWrite();stream.write(value.toString().getBytes(StandardCharsets.UTF_8));target.finishWrite(stream);}catch(Exception e){if(stream!=null)target.failWrite(stream);throw e;}}
 synchronized boolean owned()throws Exception{return InstallationRecord.owned(read());}
 synchronized void confirmOwnership(String steamId)throws Exception{
  save(InstallationRecord.confirm(read(),steamId));
 }
 File dataRoot(JSONObject record)throws Exception{return child(record.getString("root"));}
 File archive(JSONObject record)throws Exception{return record.optString("archive").isEmpty()?null:child(record.getString("archive"));}
 File child(String leaf)throws Exception{
  if(leaf.isEmpty()||leaf.equals(".")||leaf.equals("..")||!leaf.matches("[a-zA-Z0-9_.-]+"))throw new IOException("Unsafe installation path");File parent=context.getExternalFilesDir(null);if(parent==null)throw new IOException("Game storage unavailable");File root=parent.getCanonicalFile(),child=new File(root,leaf).getCanonicalFile();if(!root.equals(child.getParentFile()))throw new IOException("Installation path escaped storage");return child;
 }
 synchronized boolean ready()throws Exception{return InstallationRecord.ready(read());}
 synchronized boolean dataAvailable()throws Exception{JSONObject record=read();if(!record.optBoolean("ready"))return false;try{File root=dataRoot(record);if(!new File(root,"assets.json").isFile())return false;File archive=archive(record);return archive==null?SteamResources.available(root)&&SteamResources.sourceAvailable(context,root):archive.isFile();}catch(IOException|org.json.JSONException invalidData){return false;}}
 synchronized void activate(File root,File archive,String edition,String backend)throws Exception{
  JSONObject record=read();if(!record.optBoolean("owned"))throw new IOException("Steam ownership has not been confirmed");if(!edition.equals("vanilla")&&!edition.equals("korean"))throw new IOException("Unknown installed edition");if(!backend.equals("obb")&&!backend.equals("steam"))throw new IOException("Unknown installation backend");if(!child(root.getName()).equals(root.getCanonicalFile())||archive!=null&&!child(archive.getName()).equals(archive.getCanonicalFile()))throw new IOException("Invalid installation location");
  if(!new File(root,"assets.json").isFile())throw new IOException("Prepared game metadata missing");
  JSONObject active=InstallationRecord.activate(record,root.getName(),archive==null?"":archive.getName(),edition,backend);save(active);syncLanguage(active);
 }
 void syncLanguage(JSONObject record){String edition=record.optString("edition","vanilla");context.getSharedPreferences("installation",Context.MODE_PRIVATE).edit().putString("activeEdition",edition).commit();}
 static File activeArchive(Context context){try{Installation install=new Installation(context);JSONObject record=install.read();return record.optBoolean("owned")&&record.optBoolean("ready")?install.archive(record):null;}catch(Exception ignored){return null;}}
}
