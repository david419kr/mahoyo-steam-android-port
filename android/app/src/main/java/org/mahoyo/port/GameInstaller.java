package org.mahoyo.port;
import android.content.Context;
import android.net.Uri;
import android.database.Cursor;
import android.provider.OpenableColumns;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.function.Consumer;
import java.util.zip.*;
/** User-initiated import/verification. All destinations are app-owned, independent installations. */
final class GameInstaller {
 interface Progress {void update(String stage,long done,long total);}
 static final class Result {final File root,archive;final String edition,backend;Result(File r,File a,String e,String b){root=r;archive=a;edition=e;backend=b;}}
 private static void canceled()throws InterruptedIOException{if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Installation canceled");}
 static String digest(File file,Progress progress,long base,long total)throws Exception{
  MessageDigest digest=MessageDigest.getInstance("SHA-256");try(InputStream in=new FileInputStream(file)){byte[] buffer=new byte[1024*1024];int n;long done=0,reported=0;while((n=in.read(buffer))!=-1){canceled();digest.update(buffer,0,n);done+=n;if(done-reported>=16*1024*1024){reported=done;progress.update("verifying",base+done,total);}}}
  StringBuilder result=new StringBuilder();for(byte b:digest.digest())result.append(String.format(Locale.ROOT,"%02x",b&255));return result.toString();
 }
 static void verifySteam(Context context,File source,Progress progress)throws Exception{
  JSONObject profile=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024),runtime=SteamResources.assetJson(context,"steam-profile/profile.json",1024*1024);JSONArray rows=profile.getJSONArray("archives");Set<String> expected=new HashSet<>();long total=0,done=0;
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);expected.add(row.getString("name"));total+=row.getLong("bytes");}
  File[] archives=source.listFiles(f->f.isFile()&&f.getName().toLowerCase(Locale.ROOT).endsWith(".hfa"));Set<String> actual=new HashSet<>();if(archives!=null)for(File f:archives)actual.add(f.getName());if(!actual.equals(expected))throw new IOException("Unsupported or incomplete Steam game archives");
  File exe=new File(source,"WoH.exe");if(!exe.isFile()||!digest(exe,(s,d,t)->{},0,exe.length()).equals(runtime.getString("steamExeSha256")))throw new IOException("Unsupported Steam game version");
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);File file=new File(source,row.getString("name"));if(file.length()!=row.getLong("bytes")||!digest(file,progress,done,total).equals(row.getString("sha256")))throw new IOException("Unsupported or corrupt Steam file: "+file.getName());done+=file.length();progress.update("verifying",done,total);}
 }
 static Result steam(Context context,File source,Progress progress)throws Exception{
  verifySteam(context,source,progress);Installation install=new Installation(context);File root=install.child("game-steam-"+UUID.randomUUID().toString());try{SteamResources.prepare(context,source,root,message->{progress.update("preparing",0,0);});checkRuntime(root,"vanilla");return new Result(root,null,"vanilla","steam");}catch(Exception e){removeGenerated(install,root);throw e;}
 }
 static long uriSize(Context context,Uri uri){try(Cursor cursor=context.getContentResolver().query(uri,new String[]{OpenableColumns.SIZE},null,null,null)){return cursor!=null&&cursor.moveToFirst()&&!cursor.isNull(0)?cursor.getLong(0):-1;}catch(Exception ignored){return -1;}}
 static Result obb(Context context,Uri uri,Progress progress)throws Exception{
  Installation install=new Installation(context);String id=UUID.randomUUID().toString();File archive=install.child("game-"+id+".obb"),partial=install.child("game-"+id+".obb.partial"),root=install.child("game-obb-"+id);long expected=uriSize(context,uri);if(expected>64L*1024*1024*1024)throw new IOException("OBB file size budget");File storage=archive.getParentFile();if(expected>0&&storage.getUsableSpace()<expected+256L*1024*1024)throw new IOException("Not enough storage for OBB import");
  boolean complete=false;try{
   try(InputStream input=context.getContentResolver().openInputStream(uri);FileOutputStream output=new FileOutputStream(partial)){if(input==null)throw new IOException("Cannot read selected OBB");byte[] buffer=new byte[1024*1024];long done=0,reported=0;int n;while((n=input.read(buffer))!=-1){canceled();done+=n;if(done>64L*1024*1024*1024)throw new IOException("OBB file size budget");output.write(buffer,0,n);if(done-reported>=8*1024*1024){reported=done;progress.update("importing",done,expected);}}output.getFD().sync();if(expected>=0&&done!=expected)throw new IOException("Selected OBB changed during import");}
   if(!partial.renameTo(archive))throw new IOException("Cannot finish OBB import");
   String edition;try(ZipFile zip=new ZipFile(archive)){ZipEntry info=zip.getEntry("_obb/info.json");if(info==null||info.getSize()>65536)throw new IOException("OBB information missing");JSONObject value=new JSONObject(new String(SteamResources.read(zip.getInputStream(info),65536),StandardCharsets.UTF_8));edition=value.optString("edition","vanilla");}
   new GameResources(context,root,archive,edition);
   try(ZipFile zip=new ZipFile(archive)){Enumeration<? extends ZipEntry> entries=zip.entries();long total=archive.length(),done=0;byte[] buffer=new byte[1024*1024];while(entries.hasMoreElements()){ZipEntry entry=entries.nextElement();if(entry.isDirectory())continue;if(entry.getMethod()!=ZipEntry.STORED)throw new IOException("Unsupported compressed OBB entry");CRC32 crc=new CRC32();long count=0;try(InputStream in=zip.getInputStream(entry)){int n;while((n=in.read(buffer))!=-1){canceled();count+=n;done+=n;if(count>entry.getSize())throw new IOException("OBB entry changed");crc.update(buffer,0,n);progress.update("verifying",done,total);}}if(count!=entry.getSize()||crc.getValue()!=entry.getCrc())throw new IOException("Corrupt OBB resource: "+entry.getName());}}
   checkRuntime(root,edition);complete=true;return new Result(root,archive,edition,"obb");
  }finally{partial.delete();if(!complete){removeGenerated(install,root);removeGenerated(install,archive);}}
 }
 static Result patch(Context context,Uri uri,Progress progress)throws Exception{
  Installation install=new Installation(context);JSONObject active=install.read();
  if(!install.ready()||!active.getString("backend").equals("steam"))throw new IOException("Install the original Steam files before applying the Korean ZIP");
  File source=new SteamResources(context,install.dataRoot(active)).source();String id=UUID.randomUUID().toString();File patch=install.child("patch-"+id),root=install.child("game-steam-"+id);boolean complete=false;
  JSONObject profile=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024);JSONArray files=profile.getJSONObject("koreanPatch").getJSONArray("files");long bytes=0;for(int i=0;i<files.length();i++)bytes+=files.getJSONObject(i).getLong("bytes");
  if(patch.getParentFile().getUsableSpace()<bytes+512L*1024*1024)throw new IOException("Not enough storage for Korean patch");
  try{InputStream input=context.getContentResolver().openInputStream(uri);if(input==null)throw new IOException("Cannot read selected ZIP");KoreanPatch.unpack(input,patch,files,(d,t)->{canceled();progress.update("importing",d,t);});canceled();SteamResources.prepare(context,source,root,patch,message->{progress.update("preparing",0,0);});canceled();checkRuntime(root,"korean");complete=true;return new Result(root,null,"korean","steam");}
  finally{if(!complete){removeGenerated(install,root);removeGenerated(install,patch);}}
 }
 static void discard(Context context,Result result)throws Exception{
  Installation install=new Installation(context);JSONObject active=install.read();if(active.optString("root").equals(result.root.getName()))return;
  if(SteamResources.available(result.root)){JSONObject marker=new JSONObject(new String(Files.readAllBytes(new File(result.root,SteamResources.MARKER).toPath()),StandardCharsets.UTF_8));if(marker.has("patch"))removeGenerated(install,new File(marker.getString("patch")));}
  removeGenerated(install,result.root);if(result.archive!=null)removeGenerated(install,result.archive);
 }
 private static void removeGenerated(Installation install,File target)throws Exception{
  String name=target.getName();if(!name.matches("(?:game-(?:steam-|obb-)?|patch-)[a-f0-9-]{36}(?:\\.obb)?")||!install.child(name).equals(target.getCanonicalFile()))throw new IOException("Refusing cleanup outside generated installation");
  if(!target.exists())return;try(java.util.stream.Stream<java.nio.file.Path> paths=Files.walk(target.toPath())){for(java.nio.file.Path path:(Iterable<java.nio.file.Path>)paths.sorted(Comparator.reverseOrder())::iterator)Files.deleteIfExists(path);}
 }
 static void checkRuntime(File root,String edition)throws Exception{long handle=NativeBridge.createConfigured(root.getPath(),edition,edition.equals("korean")?"ko":"en");try{JSONObject state=new JSONObject(NativeBridge.snapshot(handle));if(!state.optString("error").isEmpty())throw new IOException(state.optString("error"));}finally{NativeBridge.destroy(handle);}}
 private GameInstaller(){}
}
