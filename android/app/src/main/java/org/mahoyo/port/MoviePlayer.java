package org.mahoyo.port;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.opengl.GLES11Ext;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import org.json.JSONObject;
import java.io.File;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/** MediaPlayer stays on the main Looper; SurfaceTexture acquisition stays on GL. */
final class MoviePlayer {
    interface Assets { GameAsset file(String name); }
    static final class Event {
        final String type,id,message;
        final long positionMs,durationMs,seekSerial;
        Event(String type,Playback playback,long position,String message){this.type=type;id=playback.id;positionMs=position;durationMs=playback.durationMs;seekSerial=playback.seeks.completedSerial();this.message=message;}
    }
    static final class Frame {
        static final Frame EMPTY=new Frame(0,"",new float[16],0,0,false,false);
        final int textureId,width,height;
        final String id;
        final float[] transform;
        final boolean newFrame,hasFrame;
        Frame(int texture,String id,float[] transform,int width,int height,boolean fresh,boolean ready){textureId=texture;this.id=id;this.transform=transform;this.width=width;this.height=height;newFrame=fresh;hasFrame=ready;}
    }
    private static final class Playback {
        final String id,path;
        final long token;
        final SeekQueue seeks=new SeekQueue();
        MediaPlayer player;
        volatile int width,height;
        long durationMs;
        float volume=.4f;
        boolean prepared,preparedSent,ended,failed,moviePaused;
        Playback(String id,String path,long token){this.id=id;this.path=path;this.token=token;}
    }
    private static final class TextureSurface {
        final int textureId,context;
        final long token;
        final String id;
        final SurfaceTexture texture;
        final Surface surface;
        final AtomicBoolean pending=new AtomicBoolean(),released=new AtomicBoolean();
        final float[] transform=new float[16];
        boolean hasFrame;
        TextureSurface(int textureId,int context,Playback playback){this.textureId=textureId;this.context=context;token=playback.token;id=playback.id;texture=new SurfaceTexture(textureId);surface=new Surface(texture);Matrix.setIdentityM(transform,0);}
    }
    private final GLSurfaceView view;
    private final Assets assets;
    private final Consumer<Event> events;
    private final Handler ui=new Handler(Looper.getMainLooper());
    private volatile Playback playback;
    private volatile TextureSurface glSurface;
    private TextureSurface attachedSurface;
    private volatile boolean closed,glReady;
    private boolean paused=true;
    private boolean positionScheduled;
    private int contextGeneration;
    private long nextToken;
    private final Runnable positionPoll=this::pollPosition;

    MoviePlayer(Context context,GLSurfaceView view,Assets assets,Consumer<Event> events){this.view=view;this.assets=assets;this.events=events;}

    // Callers may use these from lifecycle callbacks; all player operations are serialized on main.
    void update(JSONObject movie,boolean paused){onUi(()->updateOnUi(movie,paused));}
    boolean releasedForInput(){return playback==null;} //Called on main after update(null) released MediaPlayer.
    void setPaused(boolean paused){onUi(()->{this.paused=paused;syncPlayback(playback);});}
    void frameFailed(String id,String message){onUi(()->{Playback current=playback;if(current!=null&&current.id.equals(id))fail(current,current.player,message);});}
    void close(){onUi(()->{if(closed)return;closed=true;stopPlayer();playback=null;TextureSurface old=glSurface;glSurface=null;attachedSurface=null;releaseSurface(old);});}

    // GL thread only. Reconnecting a Surface does not recreate or rewind MediaPlayer.
    void onSurfaceCreated(){contextGeneration++;glReady=true;ensureSurfaceOnGl();}
    void onSurfaceDestroyed(){
        glReady=false;TextureSurface old=glSurface;glSurface=null;
        ui.post(()->{if(attachedSurface==old){Playback current=playback;pausePlayer(current);if(current!=null&&current.player!=null)try{current.player.setSurface(null);}catch(IllegalStateException ignored){}attachedSurface=null;}releaseSurface(old);});
    }
    Frame frameOnGl(){
        TextureSurface surface=glSurface;Playback current=playback;
        if(closed||surface==null||current==null||surface.token!=current.token||surface.released.get())return Frame.EMPTY;
        boolean fresh=surface.pending.getAndSet(false);
        if(fresh){
            try{surface.texture.updateTexImage();surface.texture.getTransformMatrix(surface.transform);surface.hasFrame=true;}
            catch(RuntimeException error){ui.post(()->fail(current,current.player,"영상 프레임 갱신 실패: "+error.getMessage()));return Frame.EMPTY;}
        }
        return new Frame(surface.textureId,current.id,surface.transform.clone(),current.width,current.height,fresh,surface.hasFrame);
    }
    private void onUi(Runnable work){if(Looper.myLooper()==Looper.getMainLooper())work.run();else ui.post(work);}
    private void updateOnUi(JSONObject movie,boolean paused){
        if(closed)return;this.paused=paused;
        String id=movie==null?"":movie.optString("id",""),path=movie==null?"":movie.optString("path","");
        if(id.isEmpty()||path.isEmpty()){stopPlayer();playback=null;view.queueEvent(this::ensureSurfaceOnGl);view.requestRender();return;}
        Playback current=playback;
        if(current==null||!id.equals(current.id)||!path.equals(current.path)){
            stopPlayer();current=new Playback(id,path,++nextToken);playback=current;
            view.queueEvent(this::ensureSurfaceOnGl);view.requestRender();
        }
        current.moviePaused=movie.optBoolean("paused",false);
        float volume=(float)movie.optDouble("volume",.4);volume=Float.isFinite(volume)?Math.max(0,Math.min(1,volume)):.4f;
        if(current.volume!=volume){current.volume=volume;if(current.player!=null)current.player.setVolume(volume,volume);}
        if(movie.has("seekToMs"))current.seeks.request(movie.optLong("seekSerial",0),Math.max(0,movie.optLong("seekToMs",0)));
        syncPlayback(current);
    }
    private void ensureSurfaceOnGl(){
        Playback current=playback;TextureSurface old=glSurface;
        if(closed||!glReady||current==null){glSurface=null;if(old!=null)ui.post(()->releaseSurface(old));return;}
        if(old!=null&&old.context==contextGeneration&&old.token==current.token){ui.post(()->attachSurface(old));return;}
        int[] ids=new int[1];GLES30.glGenTextures(1,ids,0);GLES30.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,ids[0]);
        GLES30.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,GLES30.GL_TEXTURE_MIN_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,GLES30.GL_TEXTURE_MAG_FILTER,GLES30.GL_LINEAR);GLES30.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,GLES30.GL_TEXTURE_WRAP_S,GLES30.GL_CLAMP_TO_EDGE);GLES30.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,GLES30.GL_TEXTURE_WRAP_T,GLES30.GL_CLAMP_TO_EDGE);
        TextureSurface surface=new TextureSurface(ids[0],contextGeneration,current);glSurface=surface;
        surface.texture.setOnFrameAvailableListener(available->{Playback active=playback;if(!closed&&glSurface==surface&&active!=null&&surface.token==active.token&&!surface.released.get()){surface.pending.set(true);view.requestRender();}},ui);
        ui.post(()->{attachSurface(surface);releaseSurface(old);});
    }
    private void attachSurface(TextureSurface surface){
        Playback current=playback;
        if(closed||glSurface!=surface||current==null||current.token!=surface.token){releaseSurface(surface);return;}
        TextureSurface old=attachedSurface;attachedSurface=surface;
        if(current.player!=null)try{current.player.setSurface(surface.surface);}catch(IllegalStateException error){fail(current,current.player,"영상 출력 연결 실패: "+error.getMessage());}
        else prepare(current,surface);
        if(old!=surface)releaseSurface(old);syncPlayback(current);
    }
    private void prepare(Playback current,TextureSurface surface){
        if(current.failed||current.player!=null)return;
        MediaPlayer player=new MediaPlayer();current.player=player;
        player.setVolume(current.volume,current.volume);
        player.setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MOVIE).build());
        player.setOnPreparedListener(ready->{if(!active(current,ready))return;current.prepared=true;current.durationMs=Math.max(0,ready.getDuration());current.width=ready.getVideoWidth();current.height=ready.getVideoHeight();syncPlayback(current);});
        player.setOnVideoSizeChangedListener((ready,width,height)->{if(active(current,ready)){current.width=width;current.height=height;view.requestRender();}});
        player.setOnSeekCompleteListener(ready->{
            if(!active(current,ready)||!current.seeks.inFlight())return;
            current.seeks.complete();emit("seeked",current,safePosition(current),"");syncPlayback(current);
        });
        player.setOnCompletionListener(ready->{if(!active(current,ready)||current.seeks.pending()||current.ended)return;current.ended=true;cancelPosition();emit("ended",current,safePosition(current),"");});
        player.setOnErrorListener((ready,what,extra)->{fail(current,ready,"MediaPlayer "+what+"/"+extra);return true;});
        try{player.setSurface(surface.surface);assets.file(current.path).source(player);player.prepareAsync();}
        catch(Exception error){fail(current,player,"영상을 열 수 없습니다: "+error.getMessage());}
    }
    private boolean active(Playback current,MediaPlayer player){return !closed&&playback==current&&current.player==player&&!current.failed;}
    private void syncPlayback(Playback current){
        if(current==null||current.failed||!current.prepared||current.player==null)return;
        MediaPlayer player=current.player;
        if(attachedSurface==null||attachedSurface.token!=current.token||paused||current.moviePaused){pausePlayer(current);return;}
        if(current.seeks.pending()){
            pausePlayer(current);if(!current.seeks.inFlight()){long target=current.seeks.begin();current.ended=false;try{player.seekTo(target,MediaPlayer.SEEK_CLOSEST);}catch(IllegalStateException error){fail(current,player,"영상 탐색 실패: "+error.getMessage());}}return;
        }
        if(current.ended)return;
        try{if(!player.isPlaying())player.start();if(!current.preparedSent){current.preparedSent=true;emit("prepared",current,safePosition(current),"");}schedulePosition();}
        catch(IllegalStateException error){fail(current,player,"영상 시작 실패: "+error.getMessage());}
    }
    private void pausePlayer(Playback current){if(current!=null&&current.player!=null&&current.prepared)try{if(current.player.isPlaying())current.player.pause();}catch(IllegalStateException ignored){}cancelPosition();}
    private long safePosition(Playback current){try{return current.player==null?0:Math.max(0,current.player.getCurrentPosition());}catch(IllegalStateException ignored){return 0;}}
    private void schedulePosition(){if(!positionScheduled){positionScheduled=true;ui.postDelayed(positionPoll,33);}}
    private void cancelPosition(){ui.removeCallbacks(positionPoll);positionScheduled=false;}
    private void pollPosition(){positionScheduled=false;Playback current=playback;if(current==null||current.failed||current.ended||paused||current.moviePaused||current.seeks.pending()||attachedSurface==null||attachedSurface.token!=current.token)return;emit("position",current,safePosition(current),"");if(playback==current&&!closed)schedulePosition();}
    private void emit(String type,Playback current,long position,String message){if(!closed&&playback==current)events.accept(new Event(type,current,position,message));}
    private void fail(Playback current,MediaPlayer player,String message){if(!active(current,player))return;current.failed=true;cancelPosition();emit("error",current,safePosition(current),message);if(current.player!=null){current.player.release();current.player=null;}}
    private void stopPlayer(){cancelPosition();Playback current=playback;if(current!=null&&current.player!=null){current.player.release();current.player=null;}}
    private void releaseSurface(TextureSurface surface){
        if(surface==null||surface==glSurface||!surface.released.compareAndSet(false,true))return;
        surface.surface.release();surface.texture.release();
        view.queueEvent(()->{if(glReady&&surface.context==contextGeneration)GLES30.glDeleteTextures(1,new int[]{surface.textureId},0);});
    }
    // Serializing seeks gives tokenless MediaPlayer callbacks an unambiguous request identity.
    static final class SeekQueue {
        private long requestedSerial,completedSerial,target,inFlightSerial=-1;
        private boolean initialPosition,requested;
        boolean request(long serial,long position){if(serial<0||requested&&serial<=requestedSerial)return false;requested=true;requestedSerial=serial;target=Math.max(0,position);initialPosition=serial>completedSerial||target>0;return initialPosition;}
        boolean pending(){return initialPosition||requestedSerial>completedSerial||inFlight();}
        boolean inFlight(){return inFlightSerial>=0;}
        long begin(){if(inFlight()||!pending())return -1;inFlightSerial=requestedSerial;initialPosition=false;return target;}
        long complete(){if(!inFlight())return -1;completedSerial=inFlightSerial;inFlightSerial=-1;return completedSerial;}
        long completedSerial(){return completedSerial;}
    }
}
