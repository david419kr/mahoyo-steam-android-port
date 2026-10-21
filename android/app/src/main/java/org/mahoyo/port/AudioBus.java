package org.mahoyo.port;

import android.content.Context;
import android.media.*;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import org.json.*;
import java.io.File;
import java.util.*;
import java.util.function.Consumer;
import java.util.function.BiConsumer;

final class AudioBus {
    private final Map<String,MediaPlayer> players=new HashMap<>();
    private final Set<MediaPlayer> preparedPlayers=new HashSet<>();
    private final Map<MediaPlayer,float[]> appliedVolumes=new IdentityHashMap<>();
    private final AudioState state=new AudioState();
    private final AudioManager manager;
    private final AudioFocusRequest focus;
    private final DisplayPrefs prefs;
    private final Map<String,PartialBgmPlayer> partials=new HashMap<>();
    private final Consumer<String> onError;
    private final Consumer<String> onEnded;
    private final HistoryVoicePlayer historyReplay;
    private BiConsumer<String,String> onHistoryReplay=(type,payload)->{};
    private final Handler ui=new Handler(Looper.getMainLooper());
    private Retiring retiring;
    private boolean retirementTickPosted;
    private final Runnable retirementTick=()->{retirementTickPosted=false;updateRetiring();};
    private boolean paused=true,focused,closed;
    AudioBus(Context context,DisplayPrefs prefs){this(context,prefs,message->{});}
    AudioBus(Context context,DisplayPrefs prefs,Consumer<String> onError){this(context,prefs,onError,message->{});}
    AudioBus(Context context,DisplayPrefs prefs,Consumer<String> onError,Consumer<String> onEnded){this.prefs=prefs;this.onError=Objects.requireNonNull(onError);this.onEnded=Objects.requireNonNull(onEnded);historyReplay=new HistoryVoicePlayer(prefs,(type,payload)->onHistoryReplay.accept(type,payload));manager=(AudioManager)context.getSystemService(Context.AUDIO_SERVICE);focus=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setOnAudioFocusChangeListener(n->{if(closed)return;focused=n>0;if(!focused)pausePlayers();else if(!paused)resumePlayers();}).build();}
    void update(JSONArray events,SceneRenderer.Assets assets){
        if(closed)return;
        if(events!=null)for(int i=0;i<events.length();i++){
            JSONObject item=events.optJSONObject(i);if(item==null)continue;
            String channel=item.optString("channel","se"),id=item.optString("id",item.optString("path"));
            float oldGain=state.gain(channel),oldPan=state.pan(channel);String oldId=state.id(channel);
            if(!state.update(channel,id,(float)item.optDouble("volume",1),(float)item.optDouble("pan",64)))continue;
            String path=item.optString("path","");MediaPlayer player=null;boolean displaced=!channel.equals("bgm");
            try{
                if(channel.equals("bgm")){retireBgm(oldFadeOutMs(item),oldId,oldGain,oldPan);displaced=true;}
                else{PartialBgmPlayer previous=partials.get(channel);if(previous!=null)previous.stop();releasePlayer(players.remove(channel));}
                if(path.isEmpty()||item.optBoolean("stop",false))continue;
                GameAsset file=assets.file(path);JSONObject region=item.optJSONObject("loopRegion");
                if(region!=null){if(!item.optBoolean("loop",false))throw new IllegalArgumentException("nonloop-region");partial(channel).start(id,file,PartialBgmPlayer.LoopSpec.fromJson(region));continue;}
                player=new MediaPlayer();player.setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(channel.equals("voice")?AudioAttributes.CONTENT_TYPE_SPEECH:AudioAttributes.CONTENT_TYPE_MUSIC).build());file.source(player);player.setLooping(item.optBoolean("loop",false));applyVolume(player,channel);players.put(channel,player);
                player.setOnPreparedListener(p->{if(players.get(channel)!=p||closed)return;try{preparedPlayers.add(p);applyVolume(p,channel);if(!paused&&focused)p.start();}catch(RuntimeException e){players.remove(channel);releasePlayer(p);report(channel,id,PartialBgmPlayer.reason(e));}});
                player.setOnCompletionListener(p->{if(players.get(channel)==p&&!p.isLooping()){players.remove(channel);releasePlayer(p);if(!closed&&state.current(channel,id))onEnded.accept(channel+" "+id);}else retiringEnded(p);});
                player.setOnErrorListener((p,a,b)->{if(players.get(channel)==p){players.remove(channel);releasePlayer(p);report(channel,id,"media-player-"+a+"-"+b);}else retiringEnded(p);return true;});player.prepareAsync();
            }catch(Exception e){if(!displaced)retireBgm(0,oldId,oldGain,oldPan);if(player!=null){players.remove(channel,player);releasePlayer(player);}android.util.Log.w("WoH.Audio","Cannot open "+path,e);report(channel,id,PartialBgmPlayer.reason(e));}
        }
        for(Map.Entry<String,MediaPlayer> entry:players.entrySet())try{applyVolume(entry.getValue(),entry.getKey());}catch(IllegalStateException ignored){}
        for(Map.Entry<String,PartialBgmPlayer> entry:partials.entrySet()){String channel=entry.getKey();float gain=AudioState.mix(state.gain(channel),prefs.volume(AudioState.preferenceChannel(channel))),pan=state.pan(channel);entry.getValue().setVolume(gain*AudioState.panGain(pan,true),gain*AudioState.panGain(pan,false));entry.getValue().setPlaybackAllowed(!paused&&focused);}
        updateRetiring();historyReplay.refreshVolume();
    }
    void setHistoryReplayListener(BiConsumer<String,String> listener){onHistoryReplay=Objects.requireNonNull(listener);}
    void updateHistoryReplay(JSONObject request,SceneRenderer.Assets assets){historyReplay.update(request,assets,!closed&&!paused&&focused);}
    private void applyVolume(MediaPlayer player,String channel){float gain=AudioState.mix(state.gain(channel),prefs.volume(AudioState.preferenceChannel(channel))),pan=state.pan(channel),left=gain*AudioState.panGain(pan,true),right=gain*AudioState.panGain(pan,false);float[] old=appliedVolumes.get(player);if(old==null||old[0]!=left||old[1]!=right){player.setVolume(left,right);appliedVolumes.put(player,new float[]{left,right});}}
    private void report(String channel,String id,String reason){if(!closed&&state.current(channel,id))onError.accept(channel+" "+id+" "+reason);}
    void resume(){if(closed)return;paused=false;focused=manager.requestAudioFocus(focus)==AudioManager.AUDIOFOCUS_REQUEST_GRANTED;if(focused)resumePlayers();else{allowPartials(false);historyReplay.setAllowed(false);updateRetiring();}}
    void pause(){paused=true;pausePlayers();manager.abandonAudioFocusRequest(focus);}
    private void allowPartials(boolean value){for(PartialBgmPlayer player:partials.values())player.setPlaybackAllowed(value);}
    private PartialBgmPlayer partial(String channel){PartialBgmPlayer player=partials.get(channel);if(player==null){if(partials.size()>=21)throw new IllegalStateException("partial-audio-channel-cap");player=new PartialBgmPlayer((id,reason)->report(channel,id,reason));partials.put(channel,player);}return player;}
    private void pausePlayers(){historyReplay.setAllowed(false);for(MediaPlayer p:players.values())try{if(p.isPlaying())p.pause();}catch(IllegalStateException ignored){}allowPartials(false);updateRetiring();}
    private void resumePlayers(){for(MediaPlayer p:players.values())try{p.start();}catch(IllegalStateException ignored){}allowPartials(!paused&&focused);historyReplay.setAllowed(!paused&&focused);updateRetiring();}
    void close(){if(closed)return;historyReplay.close();pause();closed=true;clearRetiring();for(MediaPlayer p:players.values())releasePlayer(p);players.clear();for(PartialBgmPlayer player:partials.values())player.close();}
    String debugState(){StringBuilder detail=new StringBuilder();for(Map.Entry<String,PartialBgmPlayer> entry:partials.entrySet())detail.append(" partialChannel=").append(entry.getKey()).append(' ').append(entry.getValue().debugState());return "audio paused="+paused+" focused="+focused+" mediaPlayers="+players.size()+" historyReplay="+historyReplay.debugState()+detail+(retiring==null?" retiringBgm=none":" retiringBgm="+retiring.id+" elapsedMs="+retiring.clock.elapsed+" durationMs="+retiring.clock.duration);}
    private void releasePlayer(MediaPlayer player){if(player!=null){preparedPlayers.remove(player);appliedVolumes.remove(player);player.release();}}
    private static int oldFadeOutMs(JSONObject item){double value=item.optDouble("oldFadeOutMs",0);if(!Double.isFinite(value)||value<0||value>Integer.MAX_VALUE||value!=Math.floor(value))throw new IllegalArgumentException("invalid-old-fade-ms");return (int)value;}
    private void retireBgm(int duration,String id,float gain,float pan){
        //A bounded two-slot policy: one incoming clip plus one retiring clip.
        clearRetiring();MediaPlayer player=players.remove("bgm");PartialBgmPlayer bgm=partials.get("bgm");PartialBgmPlayer.Playback track=bgm==null?null:bgm.detachPlayback();
        if(duration<=0){releasePlayer(player);if(track!=null)track.stop();return;}
        if(player!=null&&!preparedPlayers.contains(player)){releasePlayer(player);player=null;}
        if(track!=null&&player!=null){releasePlayer(player);player=null;}
        if(player!=null||track!=null){retiring=new Retiring(id,player,track,gain,pan,duration);updateRetiring();}
    }
    private void clearRetiring(){ui.removeCallbacks(retirementTick);retirementTickPosted=false;Retiring old=retiring;retiring=null;if(old!=null){releasePlayer(old.player);if(old.track!=null)old.track.stop();}}
    private void retiringEnded(MediaPlayer player){if(retiring!=null&&retiring.player==player)clearRetiring();}
    private void updateRetiring(){
        Retiring old=retiring;if(old==null)return;boolean running=!closed&&!paused&&focused;old.clock.sample(SystemClock.elapsedRealtime(),running);
        if(old.clock.done()||(old.track!=null&&!old.track.active())){clearRetiring();return;}
        try{
            float gain=AudioState.mix(old.gain,prefs.volume("bgm"))*old.clock.factor();float left=gain*AudioState.panGain(old.pan,true),right=gain*AudioState.panGain(old.pan,false);
            if(left!=old.left||right!=old.right){if(old.player!=null)old.player.setVolume(left,right);else old.track.setVolume(left,right);old.left=left;old.right=right;}
            if(old.player!=null){if(running&&!old.player.isPlaying())old.player.start();else if(!running&&old.player.isPlaying())old.player.pause();}
            else old.track.setPlaybackAllowed(running);
        }catch(RuntimeException error){android.util.Log.w("WoH.Audio","Retiring BGM "+old.id,error);clearRetiring();return;}
        if(!running){ui.removeCallbacks(retirementTick);retirementTickPosted=false;}
        else if(!retirementTickPosted){retirementTickPosted=ui.postDelayed(retirementTick,Math.min(33,old.clock.duration-old.clock.elapsed));}
    }
    static final class FadeClock {
        final long duration;
        long elapsed,last=-1;
        FadeClock(long duration){if(duration<=0)throw new IllegalArgumentException("invalid-retirement-duration");this.duration=duration;}
        void sample(long now,boolean running){if(last>=0)elapsed=Math.min(duration,elapsed+Math.max(0,now-last));last=running&&elapsed<duration?now:-1;}
        float factor(){return (float)Math.max(0,1-elapsed/(double)duration);}
        boolean done(){return elapsed>=duration;}
    }
    private static final class Retiring {
        final String id;final MediaPlayer player;final PartialBgmPlayer.Playback track;final float gain,pan;final FadeClock clock;
        float left=Float.NaN,right=Float.NaN;
        Retiring(String id,MediaPlayer player,PartialBgmPlayer.Playback track,float gain,float pan,int duration){this.id=id;this.player=player;this.track=track;this.gain=gain;this.pan=pan;clock=new FadeClock(duration);}
    }
    static final class AudioState {
        private final Map<String,String> ids=new HashMap<>();
        private final Map<String,Float> gains=new HashMap<>();
        private final Map<String,Float> pans=new HashMap<>();
        boolean update(String channel,String id,float gain){return update(channel,id,gain,pan(channel));}
        boolean update(String channel,String id,float gain,float pan){gains.put(channel,gain);pans.put(channel,clampPan(pan));return !Objects.equals(ids.put(channel,id),id);}
        boolean current(String channel,String id){return Objects.equals(ids.get(channel),id);}
        String id(String channel){return ids.getOrDefault(channel,"");}
        float gain(String channel){return gains.getOrDefault(channel,1f);}
        float pan(String channel){return pans.getOrDefault(channel,64f);}
        private static float clampPan(float pan){return Float.isFinite(pan)?Math.max(0f,Math.min(128f,pan)):64f;}
        static int panCentibels(float pan){
            // Native17d300 truncates animated pan before17c870/1ba630's cubic.
            int value=(int)clampPan(pan);double q=(value-64)/64.;return (int)(10000*q*q*q);
        }
        static float panGain(float pan,boolean left){int value=panCentibels(pan);int attenuation=left?-Math.max(value,0):Math.min(value,0);return attenuation==0?1f:(float)Math.pow(10,attenuation/2000.);}
        static float mix(float gain,int percent){return gain*percent/100f;}
        static String preferenceChannel(String channel){
            if(!channel.startsWith("se"))return channel;
            for(int i=2;i<channel.length();i++)if(channel.charAt(i)<'0'||channel.charAt(i)>'9')return channel;
            return "se";
        }
    }
}
