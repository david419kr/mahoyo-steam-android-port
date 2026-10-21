package org.mahoyo.port;
import android.content.Context;
import android.net.Uri;


import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.function.Consumer;

/** User-initiated import/verification. All destinations are app-owned, independent installations. */
final class GameInstaller {
 interface Progress {void update(String stage,long done,long total);}
 static final class Result {final File root;final String edition;Result(File r,String e){root=r;edition=e;}}
 private static void canceled()throws InterruptedIOException{if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Installation canceled");}
 static String digest(File file,Progress progress,long base,long total)throws Exception{
  MessageDigest digest=MessageDigest.getInstance("SHA-256");try(InputStream in=new FileInputStream(file)){byte[] buffer=new byte[1024*1024];int n;long done=0,reported=0;while((n=in.read(buffer))!=-1){canceled();digest.update(buffer,0,n);done+=n;if(done-reported>=16*1024*1024){reported=done;progress.update("verifying",base+done,total);}}}
  StringBuilder result=new StringBuilder();for(byte b:digest.digest())result.append(String.format(Locale.ROOT,"%02x",b&255));return result.toString();
 }
 static void verifySteam(Context context,File source,Progress progress)throws Exception{
  JSONObject profile=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024);JSONArray rows=profile.getJSONArray("archives");Set<String> expected=new HashSet<>();long total=0,done=0;
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);expected.add(row.getString("name"));total+=row.getLong("bytes");}
  File[] archives=source.listFiles(f->f.isFile()&&f.getName().toLowerCase(Locale.ROOT).endsWith(".hfa"));Set<String> actual=new HashSet<>();if(archives!=null)for(File f:archives)actual.add(f.getName());if(!actual.equals(expected))throw new IOException("Unsupported or incomplete Steam game archives");
  File exe=new File(source,"WoH.exe");if(!exe.isFile()||!digest(exe,(s,d,t)->{},0,exe.length()).equals(SteamPe.SOURCE_SHA))throw new IOException("Unsupported Steam game version");
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);File file=new File(source,row.getString("name"));if(file.length()!=row.getLong("bytes")||!digest(file,progress,done,total).equals(row.getString("sha256")))throw new IOException("Unsupported or corrupt Steam file: "+file.getName());done+=file.length();progress.update("verifying",done,total);}
 }
 static Result steam(Context context,File source,Progress progress)throws Exception{
  verifySteam(context,source,progress);Installation install=new Installation(context);File root=install.child("game-steam-"+UUID.randomUUID().toString());try{SteamResources.prepare(context,source,root,message->{progress.update("preparing",0,0);});checkRuntime(root,"vanilla");return new Result(root,"vanilla");}catch(Exception e){removeGenerated(install,root);throw e;}
 }
 static Result patch(Context context,Uri uri,Progress progress)throws Exception{
  Installation install=new Installation(context);JSONObject active=install.read();
  if(!install.ready()||!active.getString("backend").equals("steam"))throw new IOException("Install the original Steam files before applying the Korean ZIP");
  File source=new SteamResources(context,install.dataRoot(active)).source();String id=UUID.randomUUID().toString();File patch=install.child("patch-"+id),root=install.child("game-steam-"+id);boolean complete=false;
  JSONObject profile=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024);JSONArray files=profile.getJSONObject("koreanPatch").getJSONArray("files");long bytes=0;for(int i=0;i<files.length();i++)bytes+=files.getJSONObject(i).getLong("bytes");
  if(!InstallSpace.enough(patch.getParentFile().getUsableSpace(),bytes,0,InstallSpace.STEAM_RESERVE))throw new IOException(context.getString(R.string.setup_storage));
  try{InputStream input=context.getContentResolver().openInputStream(uri);if(input==null)throw new IOException("Cannot read selected ZIP");KoreanPatch.unpack(input,patch,files,(d,t)->{canceled();progress.update("importing",d,t);});canceled();SteamResources.prepare(context,source,root,patch,message->{progress.update("preparing",0,0);});canceled();checkRuntime(root,"korean");complete=true;return new Result(root,"korean");}
  finally{if(!complete){removeGenerated(install,root);removeGenerated(install,patch);}}
 }
 static void discard(Context context,Result result)throws Exception{
  Installation install=new Installation(context);JSONObject active=install.read();if(active.optString("root").equals(result.root.getName()))return;
  if(SteamResources.available(result.root)){JSONObject marker=new JSONObject(new String(Files.readAllBytes(new File(result.root,SteamResources.MARKER).toPath()),StandardCharsets.UTF_8));if(marker.has("patch"))removeGenerated(install,new File(marker.getString("patch")));}
  removeGenerated(install,result.root);
 }
 private static void removeGenerated(Installation install,File target)throws Exception{
  String name=target.getName();if(!name.matches("(?:game-steam-|patch-)[a-f0-9-]{36}")||!install.child(name).equals(target.getCanonicalFile()))throw new IOException("Refusing cleanup outside generated installation");
  if(!target.exists())return;try(java.util.stream.Stream<java.nio.file.Path> paths=Files.walk(target.toPath())){for(java.nio.file.Path path:(Iterable<java.nio.file.Path>)paths.sorted(Comparator.reverseOrder())::iterator)Files.deleteIfExists(path);}
 }
 static void checkRuntime(File root,String edition)throws Exception{long handle=NativeBridge.createConfigured(root.getPath(),edition,edition.equals("korean")?"ko":"en");try{JSONObject state=new JSONObject(NativeBridge.snapshot(handle));if(!state.optString("error").isEmpty())throw new IOException(state.optString("error"));}finally{NativeBridge.destroy(handle);}}
 private GameInstaller(){}
}
