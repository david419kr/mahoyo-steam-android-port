package org.mahoyo.port;
import android.app.*;import android.os.Bundle;import java.io.*;import org.json.JSONObject;
/** Explicit instrumentation only: uses a supplied owned source, never compiled into the release APK. */
public final class SourceProbe extends Instrumentation {
 private Bundle args;
 @Override public void onCreate(Bundle args){this.args=args;start();}
 @Override public void onStart(){Bundle result=new Bundle();try{
  String source=args.getString("source");if(source==null)throw new IllegalArgumentException("Pass -e source with an owned Steam folder");
  File root=new File(getTargetContext().getExternalFilesDir(null),"game");SteamResources.prepare(getTargetContext(),new File(source),root,message->{Bundle status=new Bundle();status.putString("stream",message+"\n");sendStatus(0,status);});
  long handle=NativeBridge.createConfigured(root.getPath(),"vanilla","ja");try{JSONObject state=new JSONObject(NativeBridge.snapshot(handle));if(!state.optString("error").isEmpty())throw new IOException(state.optString("error"));if(!state.optString("scene").equals("title"))throw new IOException("Title bootstrap failed");}finally{NativeBridge.destroy(handle);}
  GameResources resources=new GameResources(getTargetContext(),root);android.graphics.BitmapFactory.Options options=new android.graphics.BitmapFactory.Options();options.inPremultiplied=false;
  android.graphics.Bitmap bitmap=resources.file("images/title_bg1.cbg.png").bitmap(options);if(bitmap==null||bitmap.getWidth()!=1920||bitmap.getHeight()!=1080)throw new IOException("Native background bitmap failed");bitmap.recycle();
  result.putString("stream","Original source bootstrap and Android native bitmap passed.\n");finish(Activity.RESULT_OK,result);
 }catch(Throwable e){result.putString("stream",android.util.Log.getStackTraceString(e));finish(Activity.RESULT_CANCELED,result);}}
}
