package org.mahoyo.port;

import android.content.Context;
import android.net.ConnectivityManager;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import org.json.JSONObject;

/** Real owned engine metadata, isolated save directory; never touches player slots. */
final class OfflineSaveCheck {
    static String run(Context context)throws Exception{
        if(context.getSystemService(ConnectivityManager.class).getActiveNetwork()!=null)throw new IOException("Disconnect networking before the offline save check");
        Installation install=new Installation(context);
        if(!install.ready()||!install.dataAvailable())throw new IOException("A real owned installation is required");
        byte[] record=Files.readAllBytes(new File(context.getFilesDir(),"installation.json").toPath());
        File playerSaves=new File(context.getExternalFilesDir(null),"savedata");String before=fingerprint(playerSaves.toPath());
        JSONObject active=install.read();String edition=active.getString("edition"),language=GameLanguage.effective(context);
        File fixture=new File(context.getExternalFilesDir(null),"offline-save-validation-"+UUID.randomUUID()),game=new File(fixture,"game");
        if(!game.mkdirs())throw new IOException("Cannot create isolated save fixture");
        Files.write(new File(fixture,"fixture-owner.txt").toPath(),"OfflineSaveCheck; metadata copy and isolated slots only".getBytes(java.nio.charset.StandardCharsets.UTF_8));
        long handle=0;
        try{
            copy(install.dataRoot(active).toPath(),game.toPath());
            handle=NativeBridge.createConfigured(game.getPath(),edition,language);
            command(handle,"textSpeed","0");JSONObject state=command(handle,"action","start");
            long clock=0;
            for(int i=0;i<5000;i++){
                if(state.optBoolean("canSave")&&state.getString("waiting").equals("input")&&state.getJSONArray("text").length()>0)break;
                if(state.getString("waiting").equals("transitionReady")){
                    String ack=state.isNull("alphaRulePrepareAck")?state.getJSONObject("transition").get("id").toString():state.getString("alphaRulePrepareAck");
                    state=command(handle,"transitionReady",ack);
                }else state=command(handle,"tick",Long.toString(clock+=100));
                if(i==4999)throw new IOException("First input wait did not become saveable: "+state.optString("waiting"));
            }
            String expected=position(state);command(handle,"save","1");
            if(!new File(fixture,"savedata/slot1.whs").isFile())throw new IOException("Isolated save was not written");
            NativeBridge.destroy(handle);handle=0;
            handle=NativeBridge.createConfigured(game.getPath(),edition,language);
            if(!expected.equals(position(command(handle,"load","1"))))throw new IOException("Offline load changed script position or displayed text");
            NativeBridge.destroy(handle);handle=0;
            handle=NativeBridge.createConfigured(game.getPath(),edition,language);
            if(!expected.equals(position(command(handle,"action","continue"))))throw new IOException("Offline continue changed script position or displayed text");
            return "Offline Android JNI save, fresh Runtime load and fresh Runtime continue passed with owned metadata. Player slots unchanged. Recyclable fixture: "+fixture.getPath()+"\n";
        }finally{
            if(handle!=0)NativeBridge.destroy(handle);
            recyclePermissions(fixture);
            if(!Arrays.equals(record,Files.readAllBytes(new File(context.getFilesDir(),"installation.json").toPath()))||!before.equals(fingerprint(playerSaves.toPath())))throw new IOException("Offline check changed player installation or saves");
        }
    }
    static void recyclePermissions(File fixture)throws IOException{
        // Native atomic saves use mode 0600; let the host move only these test files to trash.
        File[] files=new File(fixture,"savedata").listFiles();
        if(files!=null)for(File file:files)if(!file.isFile()||!file.setReadable(true,false))throw new IOException("Cannot prepare isolated fixture for recycling");
    }
    private static JSONObject command(long handle,String type,String payload)throws Exception{
        JSONObject state=new JSONObject(NativeBridge.command(handle,type,payload));
        if(!state.optString("error").isEmpty())throw new IOException(state.getString("error"));return state;
    }
    private static String position(JSONObject state)throws Exception{
        if(!state.getString("scene").equals("play"))throw new IOException("Expected gameplay state");
        return state.getString("module")+":"+state.getLong("modulePc")+":"+state.getLong("textLine")+":"+state.getString("language")+":"+state.getJSONArray("text").getJSONObject(0).getString("text");
    }
    private static void copy(Path source,Path target)throws Exception{
        long bytes=0;int count=0;
        try(java.util.stream.Stream<Path> paths=Files.walk(source)){
            for(Path path:(Iterable<Path>)paths::iterator){
                if(Files.isSymbolicLink(path)||++count>10000)throw new IOException("Unexpected metadata tree");
                Path destination=target.resolve(source.relativize(path));
                if(Files.isDirectory(path))Files.createDirectories(destination);
                else{bytes+=Files.size(path);if(bytes>256L*1024*1024)throw new IOException("Metadata copy budget exceeded");Files.copy(path,destination);}
            }
        }
    }
    private static String fingerprint(Path root)throws Exception{
        MessageDigest hash=MessageDigest.getInstance("SHA-256");long total=0;
        if(Files.exists(root))try(java.util.stream.Stream<Path> paths=Files.walk(root)){
            for(Path path:(Iterable<Path>)paths.sorted()::iterator)if(Files.isRegularFile(path)){
                total+=Files.size(path);if(total>64L*1024*1024)throw new IOException("Player save fingerprint budget");
                hash.update(root.relativize(path).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
                try(InputStream input=Files.newInputStream(path)){byte[] bytes=new byte[8192];int n;while((n=input.read(bytes))!=-1)hash.update(bytes,0,n);}
            }
        }
        return Base64.getEncoder().encodeToString(hash.digest());
    }
}
