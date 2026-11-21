package org.mahoyo.port;

import java.io.*;
import java.nio.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.*;
import javax.sound.sampled.*;
import org.json.*;
import org.lwjgl.opengl.GL33C;

/**
 * Windows movie decoder. Original packed MP4s are accessed through FFmpeg's bounded subfile
 * protocol.
 */
final class MoviePlayer implements AutoCloseable {
  static final class Frame {
    final int textureId, width, height;
    final String id;
    final float[] transform;
    final boolean newFrame, hasFrame;

    Frame(int texture, String id, int w, int h, boolean fresh) {
      textureId = texture;
      this.id = id;
      width = w;
      height = h;
      newFrame = fresh;
      hasFrame = texture != 0;
      transform = new float[16];
      transform[0] = transform[10] = transform[15] = 1;
      transform[5] = -1;
      transform[13] = 1;
    }
  }

  private final GameResources resources;
  private final BiConsumer<String, String> events;
  private Playback current;
  private int texture;

  MoviePlayer(GameResources r, BiConsumer<String, String> events) {
    resources = r;
    this.events = events;
  }

  static String input(GameAsset asset) {
    return asset.ranged
        ? "subfile,,start,"
            + asset.offset
            + ",end,"
            + (asset.offset + asset.length)
            + ",,:"
            + asset.container.getAbsolutePath()
        : asset.container.getAbsolutePath();
  }

  static String tool(String name) {
    String ff = System.getProperty("mahoyo.ffmpeg", "ffmpeg");
    return name.equals("ffmpeg")
        ? ff
        : new File(new File(ff).getAbsoluteFile().getParentFile(), "ffprobe.exe").getPath();
  }

  void update(JSONObject movie, boolean paused) {
    String id = movie == null ? "" : movie.optString("id"),
        path = movie == null ? "" : movie.optString("path");
    if (id.isEmpty()) {
      if (current != null) current.close();
      current = null;
      return;
    }
    long serial = movie.optLong("seekSerial", 0);
    if (current == null
        || !current.id.equals(id)
        || !current.path.equals(path)
        || movie.has("seekToMs") && serial > current.serial) {
      if (current != null) current.close();
      current =
          new Playback(
              id, path, serial, movie.has("seekToMs") ? movie.optLong("seekToMs") : 0, serial > 0);
      current.start();
    }
    current.paused = paused || movie.optBoolean("paused");
    current.volume = (float) movie.optDouble("volume", .4);
  }

  boolean releasedForInput() {
    return current == null;
  }

  boolean ready() {
    return current != null && current.started && !current.failed;
  }

  long serial() {
    return current == null ? 0 : current.serial;
  }

  long position() {
    return current == null ? 0 : current.offset + (long) current.elapsed;
  }

  String debug() {
    Playback p = current;
    return p == null
        ? "no movie"
        : p.width
            + "x"
            + p.height
            + " ready="
            + p.started
            + " failed="
            + p.failed
            + " eof="
            + p.eof
            + " queued="
            + p.frames.size()
            + " shown="
            + p.shown
            + " fps="
            + p.fps
            + " videoLagMs="
            + (p.offset + (long) p.elapsed - p.presentedPosition)
            + " serial="
            + p.serial
            + " position="
            + position();
  }

  void onSurfaceCreated() {
    texture = 0;
  }

  void frameFailed(String id, String message) {
    if (current != null && current.id.equals(id)) events.accept("movieError", id + " " + message);
  }

  Frame frameOnGl() {
    Playback p = current;
    if (p == null || p.failed || p.width == 0) return new Frame(0, "", 0, 0, false);
    long now = System.nanoTime();
    if (p.lastClock != 0 && !p.paused && p.started) p.elapsed += (now - p.lastClock) / 1000000.;
    p.lastClock = now;
    boolean fresh = false;
    Decoded decoded = null;
    long target = p.offset + (long) p.elapsed;
    while (!p.frames.isEmpty() && (!p.started || p.frames.peek().position <= target)) {
      Decoded next = p.frames.poll();
      if (next == null) break;
      if (decoded != null) p.pool.offer(decoded.pixels);
      decoded = next;
      p.shown++;
      fresh = true;
      if (!p.started) break;
    }
    if (decoded != null) {
      if (texture == 0) texture = GL33C.glGenTextures();
      GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, texture);
      GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MIN_FILTER, GL33C.GL_LINEAR);
      GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MAG_FILTER, GL33C.GL_LINEAR);
      decoded.pixels.clear();
      GL33C.glTexImage2D(
          GL33C.GL_TEXTURE_2D,
          0,
          GL33C.GL_RGBA8,
          p.width,
          p.height,
          0,
          GL33C.GL_RGBA,
          GL33C.GL_UNSIGNED_BYTE,
          decoded.pixels);
      p.presentedPosition = decoded.position;
      p.pool.offer(decoded.pixels);
      if (!p.started) {
        p.started = true;
        events.accept(
            p.seeking ? "movieSeeked" : "moviePrepared",
            p.seeking ? p.payload() : p.id + " " + p.duration);
      }
    }
    if (p.started && !p.paused) {
      long position = p.offset + (long) p.elapsed;
      if (position - p.lastReported >= 33) {
        p.lastReported = position;
        events.accept("moviePosition", p.payload());
      }
      if (p.eof && p.frames.isEmpty() && p.offset + p.elapsed >= p.duration && !p.ended) {
        p.ended = true;
        events.accept("movieEnded", p.payload());
      }
    }
    return new Frame(p.started ? texture : 0, p.id, p.width, p.height, fresh);
  }

  public void close() {
    if (current != null) current.close();
    current = null;
    if (texture != 0) GL33C.glDeleteTextures(texture);
    texture = 0;
  }

  private static final class Decoded {
    final ByteBuffer pixels;
    final long position;

    Decoded(ByteBuffer pixels, long position) {
      this.pixels = pixels;
      this.position = position;
    }
  }

  private final class Playback {
    final String id, path;
    final long serial, offset;
    final boolean seeking;
    final BlockingQueue<Decoded> frames = new ArrayBlockingQueue<>(2);
    final BlockingQueue<ByteBuffer> pool = new ArrayBlockingQueue<>(3);
    volatile boolean closed, paused = true, started, failed, eof;
    boolean ended;
    volatile int width, height;
    volatile long duration;
    volatile double fps = 30, elapsed;
    long lastClock, lastReported, presentedPosition;
    int shown = -1;
    volatile float volume = .4f;
    Process video, audio;
    volatile SourceDataLine sound;

    Playback(String id, String path, long serial, long offset, boolean seeking) {
      this.id = id;
      this.path = path;
      this.serial = serial;
      this.offset = offset;
      this.seeking = seeking;
    }

    String payload() {
      return id + " " + (offset + (long) elapsed) + " " + duration + " " + serial;
    }

    void start() {
      Thread t = new Thread(this::decode, "Mahoyo movie decode");
      t.setDaemon(true);
      t.start();
    }

    void close() {
      closed = true;
      if (video != null) video.destroyForcibly();
      if (audio != null) audio.destroyForcibly();
      if (sound != null) {
        sound.stop();
        sound.flush();
        sound.close();
      }
      frames.clear();
      pool.clear();
    }

    Process process(String name, String... options) throws IOException {
      ArrayList<String> command = new ArrayList<>();
      command.add(tool(name));
      Collections.addAll(command, options);
      return new ProcessBuilder(command).redirectError(ProcessBuilder.Redirect.DISCARD).start();
    }

    void decode() {
      long decoder = 0;
      try {
        String input = input(resources.file(path));
        decoder = NativeVideo.open(input, offset);
        long[] info = NativeVideo.info(decoder);
        width = (int) info[0];
        height = (int) info[1];
        duration = info[2];
        fps = info[3] / (double) info[4];
        for (int i = 0; i < 3; i++) pool.add(ByteBuffer.allocateDirect(width * height * 4));
        if (info[5] != 0) {
          Thread a = new Thread(() -> audio(input), "Mahoyo movie audio");
          a.setDaemon(true);
          a.start();
        }
        while (!closed) {
          ByteBuffer buffer = pool.poll(100, TimeUnit.MILLISECONDS);
          if (buffer == null) continue;
          long pts = NativeVideo.next(decoder, buffer);
          if (pts < 0) {
            eof = true;
            break;
          }
          Decoded frame = new Decoded(buffer, pts);
          while (!closed && !frames.offer(frame, 100, TimeUnit.MILLISECONDS)) {}
        }
      } catch (Exception e) {
        if (!closed) {
          failed = true;
          events.accept("movieError", id + " " + e.getMessage());
        }
        close();
      } finally {
        if (decoder != 0) NativeVideo.close(decoder);
      }
    }

    void audio(String input) {
      try {
        audio =
            process(
                "ffmpeg",
                "-v",
                "error",
                "-nostdin",
                "-ss",
                Double.toString(offset / 1000.),
                "-i",
                input,
                "-vn",
                "-ac",
                "2",
                "-ar",
                "48000",
                "-f",
                "s16le",
                "pipe:1");
        AudioFormat f = new AudioFormat(48000, 16, 2, true, false);
        SourceDataLine line = AudioSystem.getSourceDataLine(f);
        sound = line;
        line.open(f, 19200);
        try (InputStream in = audio.getInputStream()) {
          byte[] raw = new byte[4096], scaled = new byte[4096];
          while (!closed) {
            if (paused || !started) {
              line.stop();
              Thread.sleep(5);
              continue;
            }
            if (!line.isRunning()) line.start();
            int n = in.readNBytes(raw, 0, raw.length);
            if (n <= 0) break;
            for (int i = 0; i + 1 < n; i += 2) {
              int sample = (short) ((raw[i] & 255) | (raw[i + 1] << 8)),
                  v = Math.round(sample * volume);
              v = Math.max(-32768, Math.min(32767, v));
              scaled[i] = (byte) v;
              scaled[i + 1] = (byte) (v >> 8);
            }
            int at = 0;
            while (at < n && !closed) {
              int used = line.write(scaled, at, n - at);
              if (used <= 0) break;
              at += used;
            }
          }
          if (!closed) line.drain();
        }
      } catch (Exception e) {
        if (!closed) events.accept("movieError", id + " " + e.getMessage());
      } finally {
        if (sound != null) sound.close();
        if (audio != null) audio.destroyForcibly();
      }
    }
  }
}
