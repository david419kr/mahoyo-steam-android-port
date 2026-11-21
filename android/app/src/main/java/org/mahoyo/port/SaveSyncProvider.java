package org.mahoyo.port;

import android.content.*;
import android.database.Cursor;
import android.net.Uri;
import android.os.*;
import java.io.*;
import java.nio.file.Files;
import java.util.*;

/** Shell-only save transfer. No network listener and no access to installation/account records. */
public final class SaveSyncProvider extends ContentProvider {
    private Session session;
    private static volatile boolean active;
    static boolean active(){return active;}
    private final Handler expiry=new Handler(Looper.getMainLooper());
    private static final long TIMEOUT=5*60*1000;
    private static final class Session {
        final String id=UUID.randomUUID().toString();final File root,saves,thumbs,journal;final MainActivity owner;
        File exported,uploaded;String original;boolean committed;
        Session(Context context,MainActivity owner)throws IOException {
            this.owner=owner;File external=context.getExternalFilesDir(null);if(external==null)throw new IOException("Game storage is unavailable");
            saves=new File(external,"savedata");thumbs=new File(context.getFilesDir(),"slot-thumbnails");journal=new File(context.getFilesDir(),"save-transfer");
            File cache=new File(context.getCacheDir(),"save-sync");cache.mkdirs();root=Files.createTempDirectory(cache.toPath(),"session-").toFile();exported=new File(root,"export.zip");uploaded=new File(root,"import.zip");
        }
    }
    private void shellOnly(){int uid=Binder.getCallingUid();if(uid!=2000&&uid!=0&&uid!=android.os.Process.myUid())throw new SecurityException("Save sync is available only through ADB");}
    @Override public boolean onCreate(){return true;}
    @Override public synchronized Bundle call(String method,String argument,Bundle extras){
        shellOnly();Bundle result=new Bundle();
        try{
            if(method.equals("version")){result.putInt("protocol",1);return result;}
            if(method.equals("begin")){
                if(session!=null)throw new IOException("Another save synchronization is in progress");
                active=true;MainActivity owner=MainActivity.saveSyncOwner();if(owner!=null)owner.beginSaveSync();
                Session current=null;
                try{
                    current=new Session(getContext(),owner);SaveArchive.recover(current.saves,current.thumbs,current.journal);
                    try(SaveArchive.Archive snapshot=SaveSyncMerge.snapshot(current.saves,current.thumbs,current.root)){
                        current.original=SaveSyncMerge.fingerprint(snapshot);SaveArchive.write(snapshot,new FileOutputStream(current.exported),"adb-sync-1");
                    }
                    session=current;String token=current.id;expiry.postDelayed(()->expire(token),TIMEOUT);result.putString("session",token);result.putLong("bytes",current.exported.length());return result;
                }catch(Exception failure){if(owner!=null)owner.endSaveSync();if(current!=null)cleanup(current);throw failure;}
            }
            Session current=required(argument);
            if(method.equals("commit")){
                if(current.committed)throw new IOException("This synchronization was already committed");
                try(SaveArchive.Archive existing=SaveSyncMerge.snapshot(current.saves,current.thumbs,current.root);SaveArchive.Archive incoming=SaveArchive.read(new FileInputStream(current.uploaded),current.root,true)){
                    if(!SaveSyncMerge.fingerprint(existing).equals(current.original))throw new IOException("Save data changed during synchronization; retry");
                    SaveArchive.Plan plan=SaveArchive.exactPlan(incoming,current.saves,current.thumbs);
                    if(!plan.replacements.isEmpty()){
                        File backupDir=new File(getContext().getFilesDir(),"save-sync-backups");backupDir.mkdirs();File backup=new File(backupDir,System.currentTimeMillis()+"-"+current.id+".zip");Files.copy(current.exported.toPath(),backup.toPath());
                        SaveArchive.apply(plan,current.journal,true,()->{if(current.owner!=null)current.owner.importSaveSyncProgress();});
                    }
                    if(!plan.replacements.isEmpty())getContext().getSharedPreferences("save-sync",Context.MODE_PRIVATE).edit().putString("revision",current.id).commit();
                    current.committed=true;result.putInt("changed",plan.replacements.size());return result;
                }
            }
            if(method.equals("verify")){try(SaveArchive.Archive snapshot=SaveSyncMerge.snapshot(current.saves,current.thumbs,current.root)){result.putString("fingerprint",SaveSyncMerge.fingerprint(snapshot));}return result;}
            if(method.equals("end")){finish(current);result.putBoolean("ended",true);return result;}
            throw new IllegalArgumentException("Unknown save synchronization operation");
        }catch(Exception failure){if(session==null)active=false;result.putString("error",failure.getMessage()==null?failure.getClass().getSimpleName():failure.getMessage());return result;}
    }
    private Session required(String id)throws IOException {if(session==null||id==null||!session.id.equals(id))throw new IOException("Save synchronization session has expired");return session;}
    private synchronized void expire(String id){if(session!=null&&session.id.equals(id))finish(session);}
    private void finish(Session current){session=null;active=false;try{if(current.owner!=null)current.owner.endSaveSync();}finally{cleanup(current);}}
    private static void cleanup(Session current){current.exported.delete();current.uploaded.delete();current.root.delete();}
    @Override public synchronized ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException {
        shellOnly();try{
            List<String> path=uri.getPathSegments();if(path.size()!=2)throw new IOException("Invalid save synchronization URI");Session current=required(path.get(0));
            if(path.get(1).equals("export")&&mode.equals("r"))return ParcelFileDescriptor.open(current.exported,ParcelFileDescriptor.MODE_READ_ONLY);
            if(path.get(1).equals("import")&&mode.startsWith("w")&&!current.committed)return ParcelFileDescriptor.open(current.uploaded,ParcelFileDescriptor.MODE_WRITE_ONLY|ParcelFileDescriptor.MODE_CREATE|ParcelFileDescriptor.MODE_TRUNCATE);
            throw new IOException("Unsupported save synchronization stream");
        }catch(IOException failure){throw new FileNotFoundException(failure.getMessage());}
    }
    @Override public String getType(Uri uri){return "application/zip";}
    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] args,String order){throw new UnsupportedOperationException();}
    @Override public Uri insert(Uri uri,ContentValues values){throw new UnsupportedOperationException();}
    @Override public int update(Uri uri,ContentValues values,String selection,String[] args){throw new UnsupportedOperationException();}
    @Override public int delete(Uri uri,String selection,String[] args){throw new UnsupportedOperationException();}
}
