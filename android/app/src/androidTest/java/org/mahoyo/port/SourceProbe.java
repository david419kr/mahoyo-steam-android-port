package org.mahoyo.port;
import android.app.*;import android.os.Bundle;import java.io.*;import org.json.JSONObject;
/** Explicit instrumentation only: uses a supplied owned source, never compiled into the release APK. */
public final class SourceProbe extends Instrumentation {
 private Bundle args;
 @Override public void onCreate(Bundle args){this.args=args;start();}
 @Override public void onStart(){Bundle result=new Bundle();try{
  if("installed".equals(args.getString("mode"))){checkInstalled(result);finish(Activity.RESULT_OK,result);return;}
  if("obb".equals(args.getString("mode"))){checkObb(result);finish(Activity.RESULT_OK,result);return;}
  if("failures".equals(args.getString("mode"))){result.putString("stream",InstallFailureCheck.run(getTargetContext()));finish(Activity.RESULT_OK,result);return;}
  if("media".equals(args.getString("mode"))){result.putString("stream",OriginalMediaCheck.run(getTargetContext()));finish(Activity.RESULT_OK,result);return;}
  String source=args.getString("source");if(source==null)throw new IllegalArgumentException("Pass -e source with an owned Steam folder");
  File root=new File(getTargetContext().getExternalFilesDir(null),"game");SteamResources.prepare(getTargetContext(),new File(source),root,message->{Bundle status=new Bundle();status.putString("stream",message+"\n");sendStatus(0,status);});
  long handle=NativeBridge.createConfigured(root.getPath(),"vanilla","ja");try{JSONObject state=new JSONObject(NativeBridge.snapshot(handle));if(!state.optString("error").isEmpty())throw new IOException(state.optString("error"));if(!state.optString("scene").equals("title"))throw new IOException("Title bootstrap failed");}finally{NativeBridge.destroy(handle);}
  GameResources resources=new GameResources(getTargetContext(),root);android.graphics.BitmapFactory.Options options=new android.graphics.BitmapFactory.Options();options.inPremultiplied=false;
  android.graphics.Bitmap bitmap=resources.file("images/title_bg1.cbg.png").bitmap(options);if(bitmap==null||bitmap.getWidth()!=1920||bitmap.getHeight()!=1080)throw new IOException("Native background bitmap failed");bitmap.recycle();
  result.putString("stream","Original source bootstrap and Android native bitmap passed.\n");finish(Activity.RESULT_OK,result);
 }catch(Throwable e){result.putString("stream",android.util.Log.getStackTraceString(e));finish(Activity.RESULT_CANCELED,result);}}
 private void checkInstalled(Bundle result)throws Exception{
  Installation install=new Installation(getTargetContext());if(!install.ready()||!install.dataAvailable())throw new IOException("A real owned, installed game is required");
  JSONObject record=install.read();File root=install.dataRoot(record);String edition=record.getString("edition");
  long handle=NativeBridge.createConfigured(root.getPath(),edition,GameLanguage.effective(getTargetContext()));try{JSONObject state=new JSONObject(NativeBridge.snapshot(handle));if(!state.optString("error").isEmpty())throw new IOException(state.optString("error"));if(!state.optString("scene").equals("title"))throw new IOException("Installed title bootstrap failed");}finally{NativeBridge.destroy(handle);}
  GameResources resources=new GameResources(getTargetContext(),root,install.archive(record),edition);android.graphics.BitmapFactory.Options options=new android.graphics.BitmapFactory.Options();options.inSampleSize=2;options.inPremultiplied=false;
  long start=android.os.SystemClock.elapsedRealtime();android.graphics.Bitmap bitmap=resources.file("images/img1800.mzp.png").bitmap(options);
  if(bitmap==null||bitmap.getWidth()!=2551||bitmap.getHeight()!=4336)throw new IOException("Sampled atlas dimensions changed");
  java.security.MessageDigest digest=java.security.MessageDigest.getInstance("SHA-256");int[] row=new int[bitmap.getWidth()];byte[] bytes=new byte[row.length*4];
  for(int y=0;y<bitmap.getHeight();++y){bitmap.getPixels(row,0,row.length,0,y,row.length,1);for(int x=0;x<row.length;++x){int pixel=row[x];bytes[x*4]=(byte)(pixel>>16);bytes[x*4+1]=(byte)(pixel>>8);bytes[x*4+2]=(byte)pixel;bytes[x*4+3]=(byte)(pixel>>>24);}digest.update(bytes);}
  StringBuilder sha=new StringBuilder();for(byte b:digest.digest())sha.append(String.format(java.util.Locale.ROOT,"%02x",b&255));bitmap.recycle();
  if(!sha.toString().equals("91b41018d410a6737ff2e80b52f2f36876bccc9135fd5943686d3c28e167f54c"))throw new IOException("Sampled atlas differs from reference: "+sha);
  result.putString("stream","Owned installation bootstrap and exact Android atlas pixels passed; "+(android.os.SystemClock.elapsedRealtime()-start)+" ms. No save slots modified.\n");
 }
 private void checkObb(Bundle output)throws Exception{
  Installation install=new Installation(getTargetContext());if(!install.ready()||!install.dataAvailable())throw new IOException("A real owned, installed game is required");
  JSONObject prior=install.read();File priorRoot=install.dataRoot(prior),priorArchive=install.archive(prior);String priorEdition=prior.getString("edition"),priorBackend=prior.getString("backend");
  String supplied=args.getString("source");File source=supplied==null?new File(getTargetContext().getExternalFilesDir(null),"probe-legacy.obb"):new File(supplied);GameInstaller.Result imported=GameInstaller.obb(getTargetContext(),android.net.Uri.fromFile(source),(s,d,t)->{});boolean activated=false;
  try{
   install.activate(imported.root,imported.archive,imported.edition,imported.backend);activated=true;if(!install.ready()||!install.dataAvailable())throw new IOException("Imported OBB activation failed");
   GameInstaller.checkRuntime(imported.root,imported.edition);GameResources resources=new GameResources(getTargetContext(),imported.root,imported.archive,imported.edition);
   android.graphics.Bitmap bitmap=resources.file("images/title_bg1.cbg.png").bitmap(new android.graphics.BitmapFactory.Options());if(bitmap==null||bitmap.getWidth()!=1920||bitmap.getHeight()!=1080)throw new IOException("OBB title decode failed");bitmap.recycle();
   String media=OriginalMediaCheck.run(resources,imported.edition);
   output.putString("stream","Real OBB import, CRC, activation, Native bootstrap and Android title bitmap passed. "+media+"Prior installation restored; no save slots modified.\n");
  }finally{if(activated)install.activate(priorRoot,priorArchive,priorEdition,priorBackend);GameInstaller.discard(getTargetContext(),imported);}
 }
}
