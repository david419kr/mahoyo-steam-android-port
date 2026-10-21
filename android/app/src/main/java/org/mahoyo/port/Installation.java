package org.mahoyo.port;
import android.content.Context;
import android.util.AtomicFile;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.UUID;
/** Local, non-expiring entitlement. Playback never consults a Steam token or clock. */
final class Installation {
 static final int APP_ID=InstallationRecord.APP_ID;
 // Activities and the service construct separate wrappers for the same AtomicFiles.
 private static final Object RECORDS=new Object();
 private final Context context;
 Installation(Context context){this.context=context.getApplicationContext();}
 private AtomicFile file(){return new AtomicFile(new File(context.getFilesDir(),"installation.json"));}
 private AtomicFile downloadFile(){return new AtomicFile(new File(context.getFilesDir(),"download.json"));}
 JSONObject read()throws Exception{synchronized(RECORDS){
  if(!file().getBaseFile().isFile()&&!new File(file().getBaseFile()+".bak").isFile())return InstallationRecord.empty();
  byte[] bytes=SteamResources.read(file().openRead(),16384);JSONObject record=new JSONObject(new String(bytes,StandardCharsets.UTF_8));
  return InstallationRecord.validate(record);
 }}
 private static void write(AtomicFile target,JSONObject value)throws Exception{FileOutputStream stream=null;try{stream=target.startWrite();stream.write(value.toString().getBytes(StandardCharsets.UTF_8));target.finishWrite(stream);}catch(Exception e){if(stream!=null)target.failWrite(stream);throw e;}}
 private void save(JSONObject value)throws Exception{write(file(),value);}
 private JSONObject downloadRecord(){try{return new JSONObject(new String(SteamResources.read(downloadFile().openRead(),4096),StandardCharsets.UTF_8));}catch(Exception invalid){return null;}}
 File downloadSource()throws Exception{synchronized(RECORDS){
  JSONObject active=read(),pending=downloadRecord();String activeSource="";
  if(active.optBoolean("ready")&&active.optString("backend").equals("steam")){
   try{JSONObject marker=new JSONObject(new String(SteamResources.read(new FileInputStream(new File(dataRoot(active),SteamResources.MARKER)),4096),StandardCharsets.UTF_8));File source=new File(marker.getString("source")).getCanonicalFile();activeSource=source.getName();}
   catch(Exception invalidActiveMarker){pending=null;}
  }
  String name=DownloadRecord.reusable(pending,activeSource);if(name==null)name="steam-download-"+UUID.randomUUID();File source=child(name);
  if(!source.isDirectory()&&!source.mkdirs())throw new IOException("Cannot create Steam download directory");write(downloadFile(),DownloadRecord.create(name));return source;
 }}
 long allocatedSteamBytes(File source)throws Exception{
  JSONObject profile=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024);org.json.JSONArray rows=profile.getJSONArray("archives");long bytes=0;
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);File file=new File(source,row.getString("name"));if(!file.isFile())continue;android.system.StructStat stat=android.system.Os.stat(file.getPath());bytes+=Math.min(row.getLong("bytes"),Math.min(file.length(),Math.max(0,stat.st_blocks)*512L));}
  return bytes;
 }
 void preservePartialSteamFiles(File source)throws Exception{
  org.json.JSONArray rows=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024).getJSONArray("archives");
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);File file=new File(source,row.getString("name"));long expected=row.getLong("bytes");
   // Upstream truncates short large files to zero before validating. Sparse extension preserves completed chunks.
   if(file.isFile()&&file.length()>0&&file.length()<expected&&expected>100L*1024*1024)try(RandomAccessFile output=new RandomAccessFile(file,"rw")){output.setLength(expected);}
  }
 }
 boolean owned()throws Exception{synchronized(RECORDS){return InstallationRecord.owned(read());}}
 void confirmOwnership(String steamId)throws Exception{synchronized(RECORDS){
  save(InstallationRecord.confirm(read(),steamId));
 }}
 File dataRoot(JSONObject record)throws Exception{return child(record.getString("root"));}
 File child(String leaf)throws Exception{
  if(leaf.isEmpty()||leaf.equals(".")||leaf.equals("..")||!leaf.matches("[a-zA-Z0-9_.-]+"))throw new IOException("Unsafe installation path");File parent=context.getExternalFilesDir(null);if(parent==null)throw new IOException("Game storage unavailable");File root=parent.getCanonicalFile(),child=new File(root,leaf).getCanonicalFile();if(!root.equals(child.getParentFile()))throw new IOException("Installation path escaped storage");return child;
 }
 boolean ready()throws Exception{synchronized(RECORDS){return InstallationRecord.ready(read());}}
 boolean dataAvailable()throws Exception{synchronized(RECORDS){JSONObject record=read();if(!record.optBoolean("ready"))return false;try{File root=dataRoot(record);if(!new File(root,"assets.json").isFile())return false;return SteamResources.available(root)&&SteamResources.sourceAvailable(context,root);}catch(IOException|org.json.JSONException invalidData){return false;}}}
 void activate(File root,String edition)throws Exception{synchronized(RECORDS){
  JSONObject record=read();if(!record.optBoolean("owned"))throw new IOException("Steam ownership has not been confirmed");if(!edition.equals("vanilla")&&!edition.equals("korean"))throw new IOException("Unknown installed edition");if(!child(root.getName()).equals(root.getCanonicalFile()))throw new IOException("Invalid installation location");
  if(!new File(root,"assets.json").isFile())throw new IOException("Prepared game metadata missing");
  JSONObject active=InstallationRecord.activate(record,root.getName(),edition);save(active);syncLanguage(active);
  try{JSONObject pending=downloadRecord(),marker=new JSONObject(new String(Files.readAllBytes(new File(root,SteamResources.MARKER).toPath()),StandardCharsets.UTF_8));String completed=new File(marker.getString("source")).getCanonicalFile().getName();if(pending!=null&&completed.equals(pending.optString("source")))downloadFile().delete();}catch(Exception ignored){/* A stale pointer still cannot select active files on the next attempt. */}
 }}
 void syncLanguage(JSONObject record){String edition=record.optString("edition","vanilla");context.getSharedPreferences("installation",Context.MODE_PRIVATE).edit().putString("activeEdition",edition).commit();}
}
