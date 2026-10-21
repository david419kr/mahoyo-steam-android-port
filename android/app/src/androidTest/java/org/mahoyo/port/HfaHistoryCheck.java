package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;
import org.json.JSONObject;

/** Synthetic dialogue with a real owned HFA voice; isolated metadata and no player slots. */
final class HfaHistoryCheck {
    static String run(Context context)throws Exception{
        Installation install=new Installation(context);JSONObject active=install.read();
        if(!install.ready()||!install.dataAvailable()||!active.getString("backend").equals("steam"))throw new IOException("Real owned HFA installation required");
        File fixture=new File(context.getExternalFilesDir(null),"offline-save-validation-"+UUID.randomUUID()),game=new File(fixture,"game");
        if(!game.mkdirs())throw new IOException("Cannot create history fixture");
        Files.write(new File(fixture,"fixture-owner.txt").toPath(),"HfaHistoryCheck: synthetic script and metadata only".getBytes(StandardCharsets.UTF_8));
        long handle=0;
        try{
            OfflineSaveCheck.copy(install.dataRoot(active).toPath(),game.toPath());Files.write(new File(game,"scripts/1_0.chs").toPath(),script());
            String edition=active.getString("edition");handle=NativeBridge.createConfigured(game.getPath(),edition,edition.equals("korean")?"ko":"ja");
            state(handle,"textSpeed","0");JSONObject ready=state(handle,"action","start");long clock=0;
            for(int i=0;i<1000&&(!ready.optBoolean("canSave")||!ready.optString("waiting").equals("input"));i++){
                if(ready.optString("waiting").equals("transitionReady")){String ack=ready.isNull("alphaRulePrepareAck")?ready.getJSONObject("transition").get("id").toString():ready.getString("alphaRulePrepareAck");ready=state(handle,"transitionReady",ack);}
                else ready=state(handle,"tick",Long.toString(clock+=100));
            }
            JSONObject history=state(handle,"history","");
            if(history.getJSONArray("history").length()==0)throw new IOException("Fixture did not reach dialogue: "+history.optString("scene")+"/"+history.optString("waiting")+"/"+history.optString("module")+" notice="+history.optString("notice"));
            JSONObject voice=history.getJSONArray("history").getJSONObject(0).getJSONArray("voices").getJSONObject(0);
            if(!voice.getBoolean("available"))throw new IOException("Real HFA voice is disabled in history: "+voice+" marker="+new File(game,".steam-ready").isFile()+" map="+new String(Files.readAllBytes(new File(game,"meta/audio-loop-map.tsv").toPath()),StandardCharsets.UTF_8).contains("a40_4_1_0026.hw"));
            JSONObject replay=state(handle,"historyReplay",history.getString("historyRevision")+" 0 1").getJSONObject("historyReplay");
            String path=replay.getJSONArray("clips").getJSONObject(0).getString("path");
            GameResources resources=new GameResources(context,game,null,edition);OriginalMediaCheck.decode(resources.file(path),"audio/",true);
            return "Owned HFA history voice is available, creates a replay request, and resolves/decodes through Android MediaCodec. No player save slots modified. Recyclable fixture: "+fixture+"\n";
        }finally{if(handle!=0)NativeBridge.destroy(handle);OfflineSaveCheck.recyclePermissions(fixture);}
    }
    private static JSONObject state(long handle,String command,String payload)throws Exception{
        JSONObject value=new JSONObject(NativeBridge.command(handle,command,payload));if(!value.optString("error").isEmpty())throw new IOException(value.getString("error"));return value;
    }
    private static byte[] script()throws Exception{
        ArrayList<Integer> words=new ArrayList<>();ArrayList<Integer> positions=new ArrayList<>();ArrayList<String> strings=new ArrayList<>();
        push(words,0);words.add(0x153);push(words,0);string(words,positions,strings,"a40_4_1_0026");words.add(0x1a0);
        for(int n:new int[]{1,1,0,0})push(words,n);string(words,positions,strings,"HFA history test");words.add(0x140);words.add(0xf4);
        ByteArrayOutputStream suffix=new ByteArrayOutputStream();for(int i=0;i<strings.size();i++){words.set(positions.get(i),words.size()*4+suffix.size());suffix.write(strings.get(i).getBytes(StandardCharsets.UTF_8));suffix.write(0);}
        ByteBuffer output=ByteBuffer.allocate(48+words.size()*4+suffix.size()).order(ByteOrder.LITTLE_ENDIAN);output.put("HunexCompiledScriptVer1.00".getBytes(StandardCharsets.US_ASCII));output.put(27,(byte)21);output.position(48);for(int word:words)output.putInt(word);output.put(suffix.toByteArray());return output.array();
    }
    private static void push(ArrayList<Integer> words,int n){words.add(0);words.add(n);}
    private static void string(ArrayList<Integer> words,ArrayList<Integer> positions,ArrayList<String> strings,String text){words.add(3);positions.add(words.size());strings.add(text);words.add(0);}
}
