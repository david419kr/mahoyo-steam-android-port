package org.mahoyo.port;

import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.graphics.Color;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.lang.ref.WeakReference;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import okhttp3.*;

/** One public GitHub check per launch; never gates offline game access. */
final class AppUpdates {
    private static final ExecutorService IO=Executors.newSingleThreadExecutor();
    private static final Handler UI=new Handler(Looper.getMainLooper());
    private static final AtomicBoolean launchChecked=new AtomicBoolean(),checking=new AtomicBoolean(),downloading=new AtomicBoolean();
    private static final OkHttpClient HTTP=new OkHttpClient.Builder().connectTimeout(6,TimeUnit.SECONDS).readTimeout(20,TimeUnit.SECONDS).callTimeout(180,TimeUnit.SECONDS).build();
    private static WeakReference<Activity> foreground=new WeakReference<>(null);
    private static WeakReference<Activity> manualWaiting=new WeakReference<>(null);
    private static WeakReference<Dialog> shown=new WeakReference<>(null);
    private static ReleaseUpdate pending;
    static SharedPreferences prefs(Context context){return context.getSharedPreferences("updates",Context.MODE_PRIVATE);}
    static boolean enabled(Context context){return prefs(context).getBoolean("enabled",true);}
    static void beginLaunch(){launchChecked.set(false);pending=null;}
    static void reset(Context context){prefs(context).edit().putBoolean("enabled",true).remove("skipTag").apply();}
    static String version(Context context)throws PackageManager.NameNotFoundException {return context.getPackageManager().getPackageInfo(context.getPackageName(),0).versionName;}
    static void launch(Activity activity){
        foreground=new WeakReference<>(activity);resumeInstall(activity);
        if(pending!=null)offerWhenReady(0);
        if(enabled(activity)&&launchChecked.compareAndSet(false,true))check(activity,false);
    }
    static void manual(Activity activity){foreground=new WeakReference<>(activity);check(activity,true);}
    static void focused(Activity activity){foreground=new WeakReference<>(activity);if(pending!=null)UI.post(()->{if(alive(activity)&&activity.hasWindowFocus())offerWhenReady(0);});}
    static void detach(Activity activity){if(foreground.get()==activity)foreground.clear();Dialog dialog=shown.get();if(dialog!=null){Context owner=dialog.getContext();while(owner instanceof ContextWrapper&&owner!=activity){Context next=((ContextWrapper)owner).getBaseContext();if(next==owner)break;owner=next;}if(owner==activity){dialog.dismiss();shown.clear();}}}
    private static boolean alive(Activity activity){return activity!=null&&!activity.isFinishing()&&!activity.isDestroyed();}
    private static void check(Activity activity,boolean manual){
        if(!checking.compareAndSet(false,true)){if(manual){manualWaiting=new WeakReference<>(activity);toast(activity,"update_checking");}return;}
        if(manual)toast(activity,"update_checking");Context context=activity.getApplicationContext();WeakReference<Activity> requester=new WeakReference<>(activity);
        IO.execute(()->{try{
            Request request=new Request.Builder().url(ReleaseUpdate.API).header("Accept","application/vnd.github+json").header("User-Agent","Mahoyo-Android/"+version(context)).build();ReleaseUpdate release;
            try(Response response=HTTP.newCall(request).execute()){if(!response.isSuccessful()||response.body()==null)throw new IOException("Release request failed");byte[] bytes=bounded(response.body().byteStream(),256*1024);release=ReleaseUpdate.parse(new String(bytes,java.nio.charset.StandardCharsets.UTF_8));}
            boolean newer=release.newerThan(version(context));UI.post(()->{checking.set(false);Activity waiting=manualWaiting.get();manualWaiting.clear();boolean explicit=manual||alive(waiting);Activity current=alive(waiting)?waiting:manual?requester.get():foreground.get();if(!alive(current))return;if(!newer){if(explicit)toast(current,"update_current");return;}
                if(explicit){show(current,release);return;}if(enabled(context)&&!prefs(context).getString("skipTag","").equals(release.tag)){pending=release;offerWhenReady(0);}
            });
        }catch(Exception error){UI.post(()->{checking.set(false);Activity waiting=manualWaiting.get();manualWaiting.clear();Activity current=alive(waiting)?waiting:requester.get();if((manual||alive(waiting))&&alive(current))toast(current,"update_check_failed");});}});
    }
    private static void offerWhenReady(int attempt){
        if(pending==null)return;Activity activity=foreground.get();if(!alive(activity))return;
        if(!enabled(activity)||prefs(activity).getString("skipTag","").equals(pending.tag)){pending=null;return;}
        Dialog dialog=shown.get();if(!activity.hasWindowFocus()||downloading.get()||dialog!=null&&dialog.isShowing()){if(attempt<60)UI.postDelayed(()->offerWhenReady(attempt+1),500);return;}
        ReleaseUpdate release=pending;pending=null;show(activity,release);
    }
    private static void show(Activity activity,ReleaseUpdate release){
        if(!alive(activity)||downloading.get())return;Dialog previous=shown.get();if(previous!=null&&previous.isShowing())return;
        LinearLayout body=new LinearLayout(activity);body.setOrientation(LinearLayout.VERTICAL);body.setPadding(dp(activity,24),dp(activity,8),dp(activity,24),0);
        TextView info=new TextView(activity);try{info.setText(UiText.format(activity,"update_available_info",release.version,version(activity),release.size/(1024.*1024.)));}catch(Exception impossible){return;}info.setTextSize(16);body.addView(info);
        TextView changes=new TextView(activity);changes.setText(UiText.key(activity,"update_release_notes"));changes.setTextColor(0xff89b7e0);changes.setPadding(0,dp(activity,12),0,dp(activity,10));changes.setOnClickListener(v->activity.startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(release.page))));body.addView(changes);
        CheckBox never=new CheckBox(activity);never.setText(UiText.key(activity,"update_never"));never.setChecked(!enabled(activity));never.setOnCheckedChangeListener((button,value)->prefs(activity).edit().putBoolean("enabled",!value).apply());body.addView(never);
        Runnable remember=()->{if(never.isChecked())prefs(activity).edit().putBoolean("enabled",false).apply();};
        AlertDialog dialog=new AlertDialog.Builder(activity).setTitle(UiText.key(activity,"update_available"))
            .setView(body).setPositiveButton(UiText.key(activity,"update_install"),(d,w)->{remember.run();download(activity,release);})
            .setNegativeButton(UiText.key(activity,"update_later"),(d,w)->remember.run())
            .setNeutralButton(UiText.key(activity,"update_skip"),(d,w)->{remember.run();prefs(activity).edit().putString("skipTag",release.tag).apply();}).create();shown=new WeakReference<>(dialog);dialog.show();
    }
    private static void download(Activity activity,ReleaseUpdate release){
        if(!downloading.compareAndSet(false,true))return;Context context=activity.getApplicationContext();WeakReference<Activity> owner=new WeakReference<>(activity);AtomicBoolean cancelled=new AtomicBoolean();
        LinearLayout view=new LinearLayout(activity);view.setOrientation(1);view.setPadding(dp(activity,24),dp(activity,16),dp(activity,24),dp(activity,8));ProgressBar progress=new ProgressBar(activity,null,android.R.attr.progressBarStyleHorizontal);progress.setMax(100);view.addView(progress,new LinearLayout.LayoutParams(-1,dp(activity,12)));TextView caption=new TextView(activity);caption.setText(UiText.key(activity,"update_downloading"));caption.setPadding(0,dp(activity,10),0,0);view.addView(caption);
        AlertDialog dialog=new AlertDialog.Builder(activity).setTitle(UiText.key(activity,"update_downloading")).setView(view).setNegativeButton(UiText.key(activity,"cancel"),(d,w)->cancelled.set(true)).create();dialog.setCanceledOnTouchOutside(false);dialog.setOnCancelListener(d->cancelled.set(true));dialog.show();shown=new WeakReference<>(dialog);
        IO.execute(()->{File target=UpdateApkProvider.file(context),temp=new File(target.getParentFile(),"download.partial");try{
            target.getParentFile().mkdirs();MessageDigest digest=MessageDigest.getInstance("SHA-256");Request request=new Request.Builder().url(release.url).header("User-Agent","Mahoyo-Android").build();
            try(Response response=HTTP.newCall(request).execute()){
                if(!response.isSuccessful()||response.body()==null||!response.request().url().isHttps())throw new IOException("APK download failed");long received=0,last=0;
                try(InputStream input=response.body().byteStream();FileOutputStream output=new FileOutputStream(temp)){byte[] bytes=new byte[65536];int n;while((n=input.read(bytes))!=-1){if(cancelled.get())throw new InterruptedIOException("Cancelled");if(n>release.size-received)throw new IOException("APK exceeds release size");output.write(bytes,0,n);digest.update(bytes,0,n);received+=n;long now=SystemClock.uptimeMillis();if(now-last>=200){last=now;int percent=(int)(received*100/release.size);UI.post(()->{if(alive(owner.get())){progress.setProgress(percent);caption.setText(percent+"%");}});}}output.getFD().sync();}
                if(received!=release.size)throw new IOException("APK truncated");
            }
            String actual="sha256:"+SlotSaveMetadata.hex(digest.digest());if(!release.digest.isEmpty()&&!release.digest.equals(actual))throw new IOException("APK checksum mismatch");validateApk(context,temp);
            Files.move(temp.toPath(),target.toPath(),StandardCopyOption.REPLACE_EXISTING);UI.post(()->{downloading.set(false);if(alive(owner.get())){dialog.dismiss();if(!cancelled.get())install(owner.get());}});
        }catch(Exception error){temp.delete();UI.post(()->{downloading.set(false);if(alive(owner.get())){dialog.dismiss();if(!cancelled.get())toast(owner.get(),"update_download_failed");}});}});
    }
    static void validateApk(Context context,File file)throws Exception {
        PackageManager pm=context.getPackageManager();PackageInfo candidate=pm.getPackageArchiveInfo(file.getPath(),PackageManager.GET_SIGNING_CERTIFICATES),installed=pm.getPackageInfo(context.getPackageName(),PackageManager.GET_SIGNING_CERTIFICATES);
        if(candidate==null||!context.getPackageName().equals(candidate.packageName)||candidate.getLongVersionCode()<=installed.getLongVersionCode()||candidate.signingInfo==null||installed.signingInfo==null)throw new IOException("Invalid update APK identity");
        Set<String> a=signers(candidate.signingInfo),b=signers(installed.signingInfo);if(a.isEmpty()||!a.equals(b))throw new IOException("Update signature differs");
    }
    private static Set<String> signers(SigningInfo info)throws Exception {Set<String> result=new HashSet<>();for(Signature signature:info.getApkContentsSigners())result.add(SlotSaveMetadata.hex(MessageDigest.getInstance("SHA-256").digest(signature.toByteArray())));return result;}
    private static void install(Activity activity){
        try{validateApk(activity,UpdateApkProvider.file(activity));if(!activity.getPackageManager().canRequestPackageInstalls()){
                AlertDialog permission=new AlertDialog.Builder(activity).setMessage(UiText.key(activity,"update_install_permission")).setPositiveButton(UiText.key(activity,"settings"),(d,w)->{prefs(activity).edit().putBoolean("pendingInstall",true).apply();activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+activity.getPackageName())));}).setNegativeButton(UiText.key(activity,"cancel"),null).create();shown=new WeakReference<>(permission);permission.show();return;}
            Uri uri=Uri.parse("content://"+activity.getPackageName()+".updates/release.apk");Intent intent=new Intent(Intent.ACTION_VIEW).setDataAndType(uri,"application/vnd.android.package-archive").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);intent.setClipData(ClipData.newRawUri("APK",uri));activity.startActivity(intent);
        }catch(Exception error){toast(activity,"update_download_failed");}
    }
    static void resumeInstall(Activity activity){if(prefs(activity).getBoolean("pendingInstall",false)){prefs(activity).edit().remove("pendingInstall").apply();if(activity.getPackageManager().canRequestPackageInstalls())install(activity);}}
    private static byte[] bounded(InputStream input,int maximum)throws IOException {try(InputStream source=input;ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] b=new byte[8192];int n;while((n=source.read(b))!=-1){if(n>maximum-out.size())throw new IOException("Metadata exceeds limit");out.write(b,0,n);}return out.toByteArray();}}
    private static void toast(Context context,String key){Toast.makeText(context,UiText.key(context,key),Toast.LENGTH_LONG).show();}
    private static int dp(Context context,int value){return Math.round(value*context.getResources().getDisplayMetrics().density);}
    private AppUpdates(){}
}
