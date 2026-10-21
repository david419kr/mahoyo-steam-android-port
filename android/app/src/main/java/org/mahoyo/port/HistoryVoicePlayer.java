package org.mahoyo.port;

import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.os.Handler;
import android.os.Looper;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;

/** Transient replay; the Core owns history validation and normal-voice stopping. */
final class HistoryVoicePlayer {
    private final DisplayPrefs prefs;
    private final BiConsumer<String,String> events;
    private final Handler ui=new Handler(Looper.getMainLooper());
    private MediaPlayer player;
    private Runnable prepareTimeout;
    private List<Clip> clips=new ArrayList<>();
    private int position;
    private boolean allowed,active,closed,haveToken,preparing,reported;
    private long generation,id;
    HistoryVoicePlayer(DisplayPrefs prefs,BiConsumer<String,String> events){this.prefs=prefs;this.events=events;}

    void update(JSONObject request,SceneRenderer.Assets assets,boolean playbackAllowed){
        if(closed)return;
        setAllowed(playbackAllowed);
        if(request==null){cancel();return;}
        long nextGeneration,nextId;
        try{nextGeneration=token(request,"generation");nextId=token(request,"id");}
        catch(RuntimeException invalid){cancel();return;}
        if(haveToken){
            int order=Long.compareUnsigned(nextGeneration,generation);
            if(order<0||order==0&&Long.compareUnsigned(nextId,id)<0)return;
            if(order==0&&nextId==id){refreshVolume();return;}
        }
        cancel();generation=nextGeneration;id=nextId;haveToken=true;reported=false;
        if(!"playing".equals(request.optString("status","")))return;
        try{
            JSONArray rows=request.optJSONArray("clips");
            if(rows==null||rows.length()==0||rows.length()>512)throw new IllegalArgumentException("history-clips");
            ArrayList<Clip> next=new ArrayList<>();int previous=-1;
            for(int i=0;i<rows.length();i++){
                Clip clip=Clip.from(rows.optJSONObject(i));
                if(clip.row<=previous)throw new IllegalArgumentException("history-row-order");
                previous=clip.row;next.add(clip);
            }
            clips=next;position=0;active=true;
            if(!allowed){finish(false,"");return;}
            prepare(assets);
        }catch(Exception error){finish(true,PartialBgmPlayer.reason(error));}
    }
    void setAllowed(boolean value){allowed=value;if(!value&&active)finish(false,"");}
    void cancel(){active=false;clips.clear();position=0;releaseCurrent();}
    void close(){closed=true;cancel();}
    void refreshVolume(){
        if(player!=null&&active)try{applyVolume(player,clips.get(position));}
        catch(RuntimeException error){finish(true,PartialBgmPlayer.reason(error));}
    }
    private void prepare(SceneRenderer.Assets assets){
        if(!active||!allowed||closed)return;
        if(position==clips.size()){finish(false,"");return;}
        Clip clip=clips.get(position);long requestGeneration=generation,requestId=id;
        try{
            GameAsset file=assets.file(clip.path);if(file==null||!file.isFile())throw new IllegalArgumentException("history-asset-missing");
            MediaPlayer next=new MediaPlayer();player=next;preparing=true;
            next.setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build());
            file.source(next);next.setLooping(false);applyVolume(next,clip);
            next.setOnPreparedListener(p->{
                if(!current(p,requestGeneration,requestId)||!preparing)return;
                try{clearTimeout();preparing=false;applyVolume(p,clip);p.start();}
                catch(RuntimeException error){finish(true,PartialBgmPlayer.reason(error));}
            });
            next.setOnCompletionListener(p->{
                if(!current(p,requestGeneration,requestId))return;
                releaseCurrent();position++;prepare(assets);
            });
            next.setOnErrorListener((p,what,extra)->{
                if(current(p,requestGeneration,requestId))finish(true,"media-player-"+what+"-"+extra);
                return true;
            });
            prepareTimeout=()->{if(current(next,requestGeneration,requestId)&&preparing)finish(true,"prepare-timeout");};
            ui.postDelayed(prepareTimeout,15000);next.prepareAsync();
        }catch(Exception error){finish(true,PartialBgmPlayer.reason(error));}
    }
    private boolean current(MediaPlayer value,long expectedGeneration,long expectedId){return !closed&&allowed&&active&&player==value&&generation==expectedGeneration&&id==expectedId;}
    private void applyVolume(MediaPlayer value,Clip clip){
        float gain=clip.rowVolume/100f*clip.actorGain/100f*Math.max(0,Math.min(100,prefs.volume("voice")))/100f;
        value.setVolume(gain*AudioBus.AudioState.panGain(clip.pan,true),gain*AudioBus.AudioState.panGain(clip.pan,false));
    }
    private void clearTimeout(){if(prepareTimeout!=null){ui.removeCallbacks(prepareTimeout);prepareTimeout=null;}}
    private void releaseCurrent(){clearTimeout();MediaPlayer old=player;player=null;preparing=false;if(old!=null)try{old.release();}catch(RuntimeException ignored){}}
    private void finish(boolean failed,String reason){
        if(closed||reported)return;reported=true;
        String payload=Long.toUnsignedString(generation)+" "+Long.toUnsignedString(id);cancel();
        if(failed){String shortReason=reason.replaceAll("\\s+","-");payload+=" "+shortReason.substring(0,Math.min(96,shortReason.length()));}
        events.accept(failed?"historyReplayFailed":"historyReplayEnded",payload);
    }
    String debugState(){return "active="+active+" generation="+Long.toUnsignedString(generation)+" id="+Long.toUnsignedString(id)+" clip="+position+" queued="+clips.size()+" preparing="+preparing+" row="+(active&&position<clips.size()?clips.get(position).row:-1);}
    private static long token(JSONObject value,String key){
        Object raw=value.opt(key);if(!(raw instanceof String))throw new IllegalArgumentException("history-token");
        String text=(String)raw;long number=Long.parseUnsignedLong(text);
        if(!text.equals(Long.toUnsignedString(number)))throw new IllegalArgumentException("history-token");return number;
    }
    private static int integer(JSONObject value,String key,int minimum,int maximum){
        Object raw=value.opt(key);if(!(raw instanceof Number))throw new IllegalArgumentException("history-"+key);
        double number=((Number)raw).doubleValue();if(!Double.isFinite(number)||number!=Math.rint(number)||number<minimum||number>maximum)throw new IllegalArgumentException("history-"+key);return (int)number;
    }
    private static final class Clip {
        final int row,index,actor,rowVolume,actorGain,pan,variant;final String path;
        Clip(int row,int index,String path,int actor,int rowVolume,int actorGain,int pan,int variant){this.row=row;this.index=index;this.path=path;this.actor=actor;this.rowVolume=rowVolume;this.actorGain=actorGain;this.pan=pan;this.variant=variant;}
        static Clip from(JSONObject value){
            if(value==null)throw new IllegalArgumentException("history-clip");
            Object raw=value.opt("path");if(!(raw instanceof String))throw new IllegalArgumentException("history-path");String path=(String)raw;
            if(path.length()>512||!path.matches("audio/[a-z0-9_-]+\\.hw\\.ogg"))throw new IllegalArgumentException("history-path");
            int actor=integer(value,"actorId",0,14),gain=value.has("actorGainPercent")?integer(value,"actorGainPercent",0,100):HistoryActorGain.defaultPercent(actor);
            return new Clip(integer(value,"row",0,511),integer(value,"index",0,65535),path,actor,integer(value,"rowVolumePercent",0,100),gain,integer(value,"pan",0,128),integer(value,"variant",0,1));
        }
    }
}
