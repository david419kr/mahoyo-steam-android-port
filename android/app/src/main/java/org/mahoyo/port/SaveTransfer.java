package org.mahoyo.port;

import android.app.*;
import android.content.*;
import android.net.Uri;
import android.widget.*;
import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.*;
import org.json.*;

/** SAF file selection plus serialized engine/storage transactions. */
final class SaveTransfer {
    private static final int EXPORT=8201,IMPORT=8202;
    private final Activity activity;private final Context context;private final SlotThumbnailStore store;private final ExecutorService engine;private final LongSupplier handle;private final Consumer<String> apply;
    private final File workspace,journal;private SlotGridDialog picker;private AlertDialog progress,confirmation;private volatile boolean destroyed;private boolean busy,deleting;private SaveArchive.Archive staged;
    SaveTransfer(Activity activity,SlotThumbnailStore store,ExecutorService engine,LongSupplier handle,Consumer<String> apply){this.activity=activity;context=activity.getApplicationContext();this.store=store;this.engine=engine;this.handle=handle;this.apply=apply;workspace=new File(context.getCacheDir(),"save-transfer");journal=new File(context.getFilesDir(),"save-transfer");}
    void choose(SlotGridDialog owner,boolean exporting){
        if(busy||destroyed)return;busy=true;picker=owner;owner.transferring(true);
        Intent intent=new Intent(exporting?Intent.ACTION_CREATE_DOCUMENT:Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/zip");
        if(exporting)intent.putExtra(Intent.EXTRA_TITLE,"mahoyo-saves-"+new SimpleDateFormat("yyyyMMdd-HHmmss",Locale.ROOT).format(new Date())+".zip");
        try{activity.startActivityForResult(intent,exporting?EXPORT:IMPORT);}catch(Exception error){failed(error);}
    }
    void delete(SlotGridDialog owner,SlotThumbnailStore.Entry entry){
        if(busy||destroyed||!entry.exists)return;busy=deleting=true;picker=owner;owner.transferring(true);confirmDelete(entry,false);
    }
    private void confirmDelete(SlotThumbnailStore.Entry entry,boolean finalWarning){
        if(!alive()||picker==null||!picker.isShowing()){finish(false,0);return;}
        confirmation=new AlertDialog.Builder(activity).setTitle(UiText.key(activity,finalWarning?"save_slot_delete_final_title":"save_slot_delete_title"))
            .setMessage(UiText.format(activity,finalWarning?"save_slot_delete_final_warning":"save_slot_delete_warning",entry.slot))
            .setNegativeButton(UiText.key(activity,"cancel"),(d,w)->{confirmation=null;finish(false,0);})
            .setPositiveButton(UiText.key(activity,finalWarning?"save_slot_delete_permanent":"save_slot_delete_continue"),(d,w)->{
                confirmation=null;if(!finalWarning){confirmDelete(entry,true);return;}
                if(!alive()||picker==null||!picker.isShowing()){finish(false,0);return;}
                showProgress("save_slot_deleting");try{store.transfer(()->{try{
                    if(destroyed)return;String raw=onEngine(()->{SaveArchive.Plan plan=SaveArchive.deletion(store.saves,store.sidecars,entry.slot,entry.modified,entry.size,entry.sha256);String[] state={null};
                        SaveArchive.apply(plan,journal,false,()->{state[0]=NativeBridge.command(handle.getAsLong(),"slotDeleted","");if(!new JSONObject(state[0]).optString("notice").equals("Save slot deleted."))throw new IOException("Cannot refresh remaining slots");});return state[0];});
                    post(()->{try{apply.accept(new JSONObject(raw).put("notice","").toString());finish(true,0,"save_slot_deleted");}catch(JSONException error){failed(error);}});
                }catch(Exception error){post(()->failed(error));}});}catch(RejectedExecutionException error){failed(error);}
            }).setOnCancelListener(d->{confirmation=null;finish(false,0);}).create();confirmation.show();
        if(finalWarning)confirmation.getButton(AlertDialog.BUTTON_POSITIVE).setTextColor(0xffe05b64);
    }
    boolean result(int request,int result,Intent data){
        if(request!=EXPORT&&request!=IMPORT)return false;if(!busy||destroyed)return true;
        if(result!=Activity.RESULT_OK||data==null||data.getData()==null){finish(false,0);return true;}Uri uri=data.getData();showProgress(request==EXPORT?"save_zip_exporting":"save_zip_importing");
        try{store.transfer(()->{try{if(request==EXPORT)export(uri);else prepareImport(uri);}catch(Exception error){post(()->failed(error));}});}catch(RejectedExecutionException error){failed(error);}return true;
    }
    private void export(Uri uri)throws Exception {
        try(SaveArchive.Archive archive=onEngine(()->{prepare();return SaveArchive.snapshot(store.saves,store.sidecars,workspace);});OutputStream output=context.getContentResolver().openOutputStream(uri,"wt")){
            if(output==null)throw new IOException("Cannot open destination");SaveArchive.write(archive,output,AppUpdates.version(context));
        }
        post(()->finish(true,-1));
    }
    private void prepareImport(Uri uri)throws Exception {
        try(InputStream input=context.getContentResolver().openInputStream(uri)){if(input==null)throw new IOException("Cannot open backup");staged=SaveArchive.read(input,workspace);}
        if(destroyed){closeStage();return;}SaveArchive.Plan plan=onEngine(()->{prepare();return SaveArchive.plan(staged,store.saves,store.sidecars);});
        if(plan.overwrites.isEmpty())commit(plan,false);else post(()->{
            hideProgress();String slots=plan.overwrites.toString().replace("[","").replace("]","");confirmation=new AlertDialog.Builder(activity).setTitle(UiText.key(activity,"save_zip_overwrite_title")).setMessage(UiText.format(activity,"save_zip_overwrite_warning",slots))
                .setPositiveButton(UiText.key(activity,"save_zip_overwrite"),(d,w)->{confirmation=null;showProgress("save_zip_importing");store.transfer(()->{try{commit(plan,true);}catch(Exception error){post(()->failed(error));}});})
                .setNegativeButton(UiText.key(activity,"cancel"),(d,w)->{confirmation=null;cleanupStage();finish(false,0);}).setOnCancelListener(d->{confirmation=null;cleanupStage();finish(false,0);}).show();
        });
    }
    private void commit(SaveArchive.Plan plan,boolean overwriteApproved)throws Exception {
        if(destroyed){closeStage();return;}String raw=onEngine(()->{
            String[] snapshot={null};SaveArchive.apply(plan,journal,overwriteApproved,()->{snapshot[0]=NativeBridge.command(handle.getAsLong(),"progressImport","");JSONObject state=new JSONObject(snapshot[0]);if(!state.optString("notice").equals("Imported progress refreshed."))throw new IOException("Cannot refresh imported progress");});return snapshot[0];
        });int count=plan.slots.size();closeStage();post(()->{try{apply.accept(new JSONObject(raw).put("notice","").toString());finish(true,count);}catch(JSONException error){failed(error);}});
    }
    private void prepare()throws Exception {long id=handle.getAsLong();if(id==0)throw new IOException("Game not ready");JSONObject state=new JSONObject(NativeBridge.command(id,"backupPrepare",""));if(!state.optString("notice").equals("Save backup ready."))throw new IOException("Cannot snapshot current save data");}
    private <T>T onEngine(Callable<T> work)throws Exception {try{return engine.submit(work).get();}catch(ExecutionException error){Throwable cause=error.getCause();if(cause instanceof Exception)throw (Exception)cause;throw error;}}
    private void showProgress(String key){hideProgress();if(!alive())return;LinearLayout content=new LinearLayout(activity);content.setOrientation(1);content.setGravity(android.view.Gravity.CENTER);content.setPadding(dp(24),dp(24),dp(24),dp(24));ProgressBar spinner=new ProgressBar(activity);content.addView(spinner,new LinearLayout.LayoutParams(dp(48),dp(48)));TextView text=new TextView(activity);text.setText(UiText.key(activity,key));text.setTextSize(18);text.setGravity(android.view.Gravity.CENTER);text.setPadding(0,dp(16),0,0);content.addView(text,new LinearLayout.LayoutParams(-1,-2));progress=new AlertDialog.Builder(activity).setView(content).setCancelable(false).create();progress.show();}
    private void hideProgress(){if(progress!=null){progress.dismiss();progress=null;}}
    private void failed(Exception error){android.util.Log.w("WoH.SaveZip","Save transfer failed",error);cleanupStage();hideProgress();if(alive())new AlertDialog.Builder(activity).setTitle(UiText.key(activity,deleting?"save_slot_delete_failed":"save_zip_failed")).setMessage(UiText.key(activity,deleting?"save_slot_delete_failed_info":"save_zip_failed_info")).setPositiveButton(UiText.key(activity,"close"),null).show();if(deleting&&picker!=null)picker.reload();finish(false,0);}
    private void finish(boolean success,int count){finish(success,count,null);}
    private void finish(boolean success,int count,String message){hideProgress();busy=deleting=false;if(picker!=null){picker.transferring(false);if(success)picker.reload();picker=null;}if(success&&alive())Toast.makeText(activity,message!=null?UiText.key(activity,message):count<0?UiText.key(activity,"save_zip_exported"):UiText.format(activity,"save_zip_imported",count),Toast.LENGTH_LONG).show();}
    private void cleanupStage(){try{store.transfer(()->{try{closeStage();}catch(IOException error){android.util.Log.w("WoH.SaveZip","Temporary backup cleanup",error);}});}catch(RejectedExecutionException ignored){}}
    private synchronized void closeStage()throws IOException {if(staged!=null){staged.close();staged=null;}}
    private boolean alive(){return !destroyed&&!activity.isFinishing()&&!activity.isDestroyed();}
    private void post(Runnable work){activity.runOnUiThread(()->{if(alive())work.run();});}
    void close(){destroyed=true;hideProgress();if(confirmation!=null)confirmation.dismiss();cleanupStage();}
    private int dp(int value){return Math.round(value*activity.getResources().getDisplayMetrics().density);}
}
