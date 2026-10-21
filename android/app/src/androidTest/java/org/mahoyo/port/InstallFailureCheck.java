package org.mahoyo.port;

import android.content.*;
import android.net.Uri;
import android.provider.MediaStore;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;
import org.json.JSONObject;

/** Real installer failure paths against an existing owned game. No entitlement is manufactured. */
final class InstallFailureCheck {
    interface Attempt {GameInstaller.Result run()throws Exception;}
    private final Context context;private final Installation installation;
    private final List<Uri> fixtures=new ArrayList<>();
    private final byte[] record;private final Set<String> roots;private final String saves;
    private int checks;
    private InstallFailureCheck(Context context)throws Exception{
        this.context=context;installation=new Installation(context);
        if(!installation.ready()||!installation.dataAvailable()||!installation.read().getString("backend").equals("steam"))throw new IOException("A real owned Steam installation is required");
        record=Files.readAllBytes(new File(context.getFilesDir(),"installation.json").toPath());roots=roots();saves=saves();
    }
    static String run(Context context)throws Exception{
        InstallFailureCheck check=new InstallFailureCheck(context);
        try{
            check.rejectUnverifiedSession();
            JSONObject active=check.installation.read();File root=check.installation.dataRoot(active);
            JSONObject marker=new JSONObject(new String(Files.readAllBytes(new File(root,SteamResources.MARKER).toPath()),StandardCharsets.UTF_8));
            byte[] dll=Files.readAllBytes(new File(marker.getString("patch"),"mahoyo_kr.dll").toPath());
            Uri badObb=check.zip("obb",null);
            check.reject(()->GameInstaller.obb(context,badObb,(s,d,t)->{}),false,"invalid OBB format");
            Uri missing=check.zip("missing",dll);
            check.reject(()->GameInstaller.patch(context,missing,(s,d,t)->{}),false,"missing patch files");
            Uri wrong=check.zip("wrong",dll);
            check.reject(()->GameInstaller.patch(context,wrong,(s,d,t)->{}),false,"unsupported patch version");
            Uri duplicate=check.zip("duplicate",dll);
            check.reject(()->GameInstaller.patch(context,duplicate,(s,d,t)->{}),false,"conflicting duplicate patch leaf");
            Thread.currentThread().interrupt();
            check.reject(()->GameInstaller.patch(context,missing,(s,d,t)->{}),true,"interrupted patch input");
            return "Android installer safety: fresh online session gate and "+check.checks+" failed imports passed; installation record, active files and saves preserved. Source ZIPs retained in system trash.\n";
        }finally{Thread.interrupted();check.trash();}
    }
    private void reject(Attempt attempt,boolean interrupted,String name)throws Exception{
        GameInstaller.Result result=null;
        try{result=attempt.run();throw new AssertionError("Installer accepted "+name);}
        catch(IOException expected){if(interrupted&&!(expected instanceof InterruptedIOException)&&!(expected instanceof java.nio.channels.ClosedByInterruptException))throw new AssertionError("Cancellation was not observed",expected);}
        finally{Thread.interrupted();if(result!=null)GameInstaller.discard(context,result);}
        if(!Arrays.equals(record,Files.readAllBytes(new File(context.getFilesDir(),"installation.json").toPath()))||!installation.ready()||!installation.dataAvailable()||!roots.equals(roots())||!saves.equals(saves()))throw new AssertionError("Failed import changed the installed game: "+name);
        ++checks;
    }
    private void rejectUnverifiedSession()throws Exception{
        java.util.concurrent.CountDownLatch done=new java.util.concurrent.CountDownLatch(1);java.util.concurrent.atomic.AtomicReference<String> result=new java.util.concurrent.atomic.AtomicReference<>();
        java.util.function.Consumer<String> unexpected=value->{result.set("unexpected "+value);done.countDown();};
        SteamSession.Listener listener=new SteamSession.Listener(){
            public void status(String value){unexpected.accept("connection attempt");}public void guard(String email,boolean incorrect,java.util.concurrent.CompletableFuture<String> answer){answer.cancel(true);unexpected.accept("guard");}
            public void qr(String value){unexpected.accept("QR");}public void mobileApproval(){unexpected.accept("approval");}public void owned(String value){unexpected.accept("ownership");}
            public void estimate(long network,long installed){unexpected.accept("estimate");}public void progress(float fraction,long network,long installed){unexpected.accept("progress");}public void downloaded(){unexpected.accept("download");}
            public void failed(String error){result.set(error);done.countDown();}
        };
        JSONObject profile=SteamResources.assetJson(context,"steam-profile/source-profile.json",1024*1024).getJSONObject("steam");
        SteamSession session=new SteamSession(listener,Long.parseUnsignedLong(profile.getString("manifestId")),profile.getLong("installedBytes"));File target=new File(context.getExternalFilesDir(null),"probe-unverified-"+UUID.randomUUID());
        try{if(session.connected())throw new AssertionError("Fresh session inherited offline ownership");session.download(target);if(!done.await(3,java.util.concurrent.TimeUnit.SECONDS)||!"Sign in to Steam again to download".equals(result.get())||target.exists())throw new AssertionError("Download bypassed online login: "+result.get());}
        finally{session.close();}
    }
    private Set<String> roots(){String[] names=context.getExternalFilesDir(null).list();if(names==null)throw new AssertionError("Storage disappeared");return new TreeSet<>(Arrays.asList(names));}
    private String saves()throws Exception{
        File folder=new File(context.getExternalFilesDir(null),"savedata");MessageDigest hash=MessageDigest.getInstance("SHA-256");long total=0;
        if(folder.exists())try(java.util.stream.Stream<java.nio.file.Path> paths=Files.walk(folder.toPath())){
            for(java.nio.file.Path path:(Iterable<java.nio.file.Path>)paths.sorted()::iterator)if(Files.isRegularFile(path)){
                total+=Files.size(path);if(total>64L*1024*1024)throw new IOException("Save fixture fingerprint budget");
                hash.update(folder.toPath().relativize(path).toString().getBytes(StandardCharsets.UTF_8));
                try(InputStream input=Files.newInputStream(path)){byte[] bytes=new byte[8192];int n;while((n=input.read(bytes))!=-1)hash.update(bytes,0,n);}
            }
        }
        return Base64.getEncoder().encodeToString(hash.digest());
    }
    private Uri zip(String kind,byte[] dll)throws Exception{
        ContentValues values=new ContentValues();values.put(MediaStore.MediaColumns.DISPLAY_NAME,"mahoyo-validation-"+kind+"-"+UUID.randomUUID()+".zip");values.put(MediaStore.MediaColumns.MIME_TYPE,"application/zip");values.put(MediaStore.MediaColumns.RELATIVE_PATH,"Download/");values.put(MediaStore.MediaColumns.IS_PENDING,1);
        Uri uri=context.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI,values);if(uri==null)throw new IOException("Cannot create owned fixture");fixtures.add(uri);
        try(OutputStream out=context.getContentResolver().openOutputStream(uri);ZipOutputStream zip=new ZipOutputStream(out)){
            if(kind.equals("obb")){entry(zip,"_obb/index.tsv",new byte[0]);entry(zip,"_obb/info.json","{\"format\":\"invalid\"}".getBytes(StandardCharsets.UTF_8));}
            else {entry(zip,"NESTED/PATCH_KR/MAHOYO_KR.DLL",kind.equals("wrong")?new byte[]{1,2}:dll);if(kind.equals("duplicate"))entry(zip,"other/mahoyo_kr.dll",new byte[]{7});}
        }
        ContentValues published=new ContentValues();published.put(MediaStore.MediaColumns.IS_PENDING,0);context.getContentResolver().update(uri,published,null,null);return uri;
    }
    private static void entry(ZipOutputStream zip,String name,byte[] bytes)throws IOException{zip.putNextEntry(new ZipEntry(name));zip.write(bytes);zip.closeEntry();}
    private void trash()throws IOException{
        ContentValues values=new ContentValues();values.put(MediaStore.MediaColumns.IS_PENDING,0);values.put(MediaStore.MediaColumns.IS_TRASHED,1);
        android.os.Bundle query=new android.os.Bundle();query.putInt(MediaStore.QUERY_ARG_MATCH_TRASHED,MediaStore.MATCH_INCLUDE);
        for(Uri uri:fixtures){if(context.getContentResolver().update(uri,values,null,null)!=1)throw new IOException("Cannot trash owned fixture");try(android.database.Cursor cursor=context.getContentResolver().query(uri,new String[]{MediaStore.MediaColumns.IS_TRASHED},query,null)){if(cursor==null||!cursor.moveToFirst()||cursor.getInt(0)!=1)throw new IOException("Owned fixture was not retained in trash");}}
    }
}
