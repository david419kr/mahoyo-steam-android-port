package org.mahoyo.port;

import java.nio.*;
import java.util.*;
import java.util.function.*;
import javax.sound.sampled.*;
import org.json.*;
import org.lwjgl.stb.STBVorbis;
import org.lwjgl.system.*;

final class DesktopAudio implements AutoCloseable {
  private final Map<String, Playback> channels = new HashMap<>();
  private final GameResources resources;
  private Playback retiring, replayPlayer;
  private String replayToken = "";
  private JSONArray replayClips;
  private int replayIndex;
  private boolean modalPaused, pauseMusic;

  private final DisplayPrefs prefs;
  private final BiConsumer<String, String> callback;

  DesktopAudio(GameResources r, DisplayPrefs p, BiConsumer<String, String> callback) {
    resources = r;
    prefs = p;
    this.callback = callback;
  }

  synchronized void update(JSONArray events) {
    if (events == null) return;
    for (int i = 0; i < events.length(); i++) {
      JSONObject e = events.getJSONObject(i);
      String channel = e.optString("channel", "se"), id = e.optString("id", e.optString("path"));
      Playback old = channels.get(channel);
      if (old != null && old.id.equals(id)) {
        old.update(e);
        continue;
      }
      if (old != null) {
        int duration = e.optInt("oldFadeOutMs", 0);
        if (channel.equals("bgm") && duration > 0) {
          if (retiring != null) retiring.close();
          retiring = old;
          old.fadeDuration = duration;
          old.suppressEnded = true;
        } else old.close();
      }
      channels.remove(channel);
      if (e.optBoolean("stop") || e.optString("path").isEmpty()) continue;
      Playback next = new Playback(channel, id, e);
      channels.put(channel, next);
      Thread thread = new Thread(next, "Mahoyo audio " + channel);
      thread.setDaemon(true);
      thread.start();
    }
  }

  synchronized void pause(boolean paused) {
    pause(paused, false);
  }

  synchronized void pause(boolean paused, boolean keepMusic) {
    modalPaused = paused;
    pauseMusic = !keepMusic;
    for (Playback p : channels.values())
      p.paused = paused && (!keepMusic || !p.channel.startsWith("bgm"));
    if (retiring != null) retiring.paused = paused && !keepMusic;
  }

  synchronized void updateHistory(JSONObject request) {
    if (request == null || !request.optString("status").equals("playing")) {
      stopReplay();
      return;
    }
    String token = request.optString("generation") + " " + request.optString("id");
    if (token.equals(replayToken)) return;
    stopReplay();
    replayToken = token;
    replayClips = request.getJSONArray("clips");
    replayIndex = 0;
    nextReplay(token);
  }

  private synchronized void nextReplay(String token) {
    if (!token.equals(replayToken)) return;
    if (replayIndex == replayClips.length()) {
      callback.accept("historyReplayEnded", token);
      replayPlayer = null;
      return;
    }
    JSONObject clip = replayClips.getJSONObject(replayIndex++);
    JSONObject event =
        new JSONObject()
            .put("path", clip.getString("path"))
            .put(
                "volume",
                clip.getInt("rowVolumePercent") * clip.optInt("actorGainPercent", 100) / 10000.)
            .put("pan", clip.optInt("pan", 64));
    Playback p = new Playback("history", token, event);
    p.paused = false;
    p.ended = () -> nextReplay(token);
    p.failure =
        reason ->
            callback.accept("historyReplayFailed", token + " " + reason.replaceAll("\\s+", "-"));
    replayPlayer = p;
    Thread thread = new Thread(p, "Mahoyo history voice");
    thread.setDaemon(true);
    thread.start();
  }

  private synchronized void stopReplay() {
    if (replayPlayer != null) replayPlayer.close();
    replayPlayer = null;
    replayClips = null;
    replayToken = "";
  }

  @Override
  public synchronized void close() {
    for (Playback p : channels.values()) p.close();
    channels.clear();
    if (retiring != null) retiring.close();
    stopReplay();
  }

  private final class Playback implements Runnable {
    final String channel, id, path;
    final boolean loop;
    final JSONObject region;
    volatile boolean stopped, paused;
    volatile float gain, pan;
    volatile SourceDataLine line;
    volatile int fadeDuration;
    long fadeElapsed, lastFade;
    boolean suppressEnded;
    Runnable ended;
    java.util.function.Consumer<String> failure;

    Playback(String channel, String id, JSONObject e) {
      this.channel = channel;
      this.id = id;
      path = e.getString("path");
      loop = e.optBoolean("loop");
      region = e.optJSONObject("loopRegion");
      update(e);
      paused = modalPaused && (pauseMusic || !channel.startsWith("bgm"));
      ended = () -> callback.accept("audioEnded", channel + " " + id);
      failure = reason -> callback.accept("audioError", channel + " " + id + " " + reason);
    }

    void update(JSONObject e) {
      String pref =
          (channel.equals("voice") || channel.equals("history"))
              ? "voice"
              : channel.startsWith("bgm") ? "bgm" : "se";
      gain = (float) e.optDouble("volume", 1) * prefs.volume(pref) / 100f;
      pan = (float) e.optDouble("pan", 64);
    }

    void close() {
      stopped = true;
      SourceDataLine l = line;
      if (l != null) {
        l.stop();
        l.flush();
        l.close();
      }
    }

    public void run() {
      ShortBuffer pcm = null;
      try {
        byte[] encoded = resources.file(path).encoded(64 * 1024 * 1024);
        ByteBuffer data = MemoryUtil.memAlloc(encoded.length);
        IntBuffer count = ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder()).asIntBuffer(),
            rate = ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder()).asIntBuffer();
        try {
          data.put(encoded).flip();
          pcm = STBVorbis.stb_vorbis_decode_memory(data, count, rate);
        } finally {
          MemoryUtil.memFree(data);
        }
        if (pcm == null || count.get(0) < 1 || count.get(0) > 2 || rate.get(0) < 8000)
          throw new IllegalStateException("Cannot decode game audio");
        int components = count.get(0), frames = pcm.remaining() / components;
        int start = region == null ? 0 : region.getInt("startFrame"),
            end = region == null ? frames : Math.min(frames, region.getInt("endFrame"));
        if (end <= 0) end = frames;
        if (start < 0 || start >= end) throw new IllegalStateException("Invalid game audio loop");
        AudioFormat format = new AudioFormat(rate.get(0), 16, components, true, false);
        SourceDataLine output = AudioSystem.getSourceDataLine(format);
        line = output;
        if (stopped) return;
        output.open(format, Math.max(4096, rate.get(0) / 10 * components * 2));
        if (stopped) return;
        output.start();
        int position = 0;
        byte[] block = new byte[1024 * components * 2];
        while (!stopped) {
          if (paused) {
            lastFade = 0;
            output.stop();
            Thread.sleep(10);
            continue;
          }
          if (!output.isRunning()) output.start();
          float fade = 1;
          if (fadeDuration > 0) {
            long now = System.nanoTime();
            if (lastFade != 0) fadeElapsed += now - lastFade;
            lastFade = now;
            fade = Math.max(0, 1 - fadeElapsed / (fadeDuration * 1000000f));
            if (fade <= 0) break;
          }
          int n = Math.min(1024, end - position);
          float left = gain * (pan > 64 ? (128 - Math.min(128, pan)) / 64f : 1),
              right = gain * (pan < 64 ? Math.max(0, pan) / 64f : 1);
          for (int f = 0; f < n; f++)
            for (int c = 0; c < components; c++) {
              int value =
                  Math.round(
                      pcm.get((position + f) * components + c)
                          * fade
                          * (components == 1 ? gain : c == 0 ? left : right));
              value = Math.max(-32768, Math.min(32767, value));
              int at = (f * components + c) * 2;
              block[at] = (byte) value;
              block[at + 1] = (byte) (value >> 8);
            }
          int length = n * components * 2, written = 0;
          while (written < length && !stopped) {
            int used = output.write(block, written, length - written);
            if (used <= 0) break;
            written += used;
          }
          position += n;
          if (position >= end) {
            if (loop) position = start;
            else break;
          }
        }
        if (!stopped) {
          output.drain();
          if (!stopped && !suppressEnded) ended.run();
        }
      } catch (Exception e) {
        if (!stopped) failure.accept(e.getMessage());
      } finally {
        if (line != null) line.close();
        if (pcm != null) org.lwjgl.system.libc.LibCStdlib.free(pcm);
      }
    }
  }
}
