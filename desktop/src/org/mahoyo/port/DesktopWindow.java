package org.mahoyo.port;

import android.content.Context;
import android.graphics.*;
import android.opengl.*;
import java.io.*;
import java.nio.*;
import java.util.*;
import org.json.*;
import org.lwjgl.glfw.*;
import org.lwjgl.opengl.*;

final class DesktopWindow {
  private long window, engine;
  private final Context context;
  private final GameResources resources;
  private final DisplayPrefs prefs;
  private final GLSurfaceView surface = new GLSurfaceView();
  private final DesktopOverlay overlay = new DesktopOverlay();
  private final TextOverlay text;
  private Bitmap overlayBitmap;
  private String overlayKey = "";
  private DesktopCanvas.Cursor cursor;
  private final DesktopCanvas.Cache drawingCache = new DesktopCanvas.Cache();
  private final java.util.concurrent.ConcurrentLinkedQueue<Runnable> commands =
      new java.util.concurrent.ConcurrentLinkedQueue<>();
  private DesktopAudio audio;
  private MoviePlayer movies;
  private DesktopDialogs dialogs;
  private volatile boolean dialogOpen;
  private Thread glThread;
  private SceneRenderer renderer;
  private Scene scene;
  private Viewport viewport;
  private int width = Integer.getInteger("mahoyo.width", 1280),
      height = Integer.getInteger("mahoyo.height", 720),
      bufferWidth,
      bufferHeight;
  private final DesktopSurface sceneSurface = new DesktopSurface();
  private int oldX = 100, oldY = 100, oldW = 1280, oldH = 720;
  private boolean fullscreen, menu, hidden;
  private DesktopInput input;
  private DesktopCursor mouseCursor;
  private boolean windowFocused = true;
  private volatile boolean syncBusy;
  private DesktopSaveSync saveSync;
  private final DesktopText translations;
  private DesktopControls controls;
  private final InputRegionSession inputRegion = new InputRegionSession();
  private int appliedTextSpeed = -1, appliedAutoDelay = -1;

  DesktopWindow(Context context, GameResources resources, long engine) {
    this.context = context;
    translations = new DesktopText(resources);
    this.resources = resources;
    this.engine = engine;
    prefs = new DisplayPrefs(context);
    text = new TextOverlay(context);
    text.setPanelScale(prefs.dialogueScale());
  }

  void run() throws Exception {
    glThread = Thread.currentThread();
    GLFWErrorCallback.createPrint(System.err).set();
    if (!GLFW.glfwInit()) throw new IOException("GLFW initialization failed");
    GLFW.glfwWindowHint(GLFW.GLFW_CONTEXT_VERSION_MAJOR, 3);
    GLFW.glfwWindowHint(GLFW.GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFW.glfwWindowHint(GLFW.GLFW_OPENGL_PROFILE, GLFW.GLFW_OPENGL_COMPAT_PROFILE);
    if (Boolean.getBoolean("mahoyo.smoke")) GLFW.glfwWindowHint(GLFW.GLFW_VISIBLE, GLFW.GLFW_FALSE);
    window = GLFW.glfwCreateWindow(width, height, "WITCH ON THE HOLY NIGHT — Unofficial", 0, 0);
    if (window == 0) throw new IOException("Cannot create OpenGL window");
    GLFW.glfwMakeContextCurrent(window);
    GL.createCapabilities();
    GLFW.glfwSwapInterval(1);
    audio =
        new DesktopAudio(
            resources, prefs, (type, payload) -> commands.add(() -> command(type, payload)));
    movies =
        new MoviePlayer(
            resources,
            (type, payload) ->
                commands.add(
                    () -> {
                      if (Boolean.getBoolean("mahoyo.smoke") && type.equals("movieError"))
                        throw new IllegalStateException(payload);
                      command(type, payload);
                    }));
    controls = new DesktopControls(resources, this::command);
    dialogs =
        new DesktopDialogs(
            new DesktopDialogs.Host() {
              public JSONObject call(String type, String payload) {
                return invoke(type, payload);
              }

              public Bitmap capture() {
                return onGl(() -> snapshotBitmap());
              }

              public void language(String target) {
                commands.add(
                    () ->
                        renderer.switchLanguage(
                            target,
                            () -> {
                              resources.setLanguage(target);
                              DialogueArt.load(resources);
                              command("language", target);
                            },
                            () -> {}));
              }

              public void open(boolean open, boolean settings) {
                dialogOpen = open;
                commands.add(
                    () -> {
                      command(open ? "pause" : "resume", "");
                      audio.pause(open, !settings);
                    });
              }
            },
            context,
            prefs,
            resources);
    renderer =
        new SceneRenderer(
            surface,
            new SceneRenderer.Assets() {
              public GameAsset file(String n) {
                return resources.file(n);
              }

              public Collection<String> imageFiles() {
                return resources.imageFiles();
              }
            },
            s -> {
              if (syncBusy) return;
              if (s.transition != null && s.json.optString("waiting").equals("transitionReady")) {
                String ack = s.json.optString("alphaRulePrepareAck");
                command("transitionReady", ack.isEmpty() ? s.transition.id : ack);
              }
              controls.presented(s);
              String inputAck =
                  inputRegion.acknowledge(
                      s.json.optLong("version"),
                      scene == s,
                      !dialogOpen,
                      movies.releasedForInput(),
                      width,
                      height);
              if (!inputAck.isEmpty()) {
                int space = inputAck.indexOf(' ');
                command(inputAck.substring(0, space), inputAck.substring(space + 1));
              }
              String particleAck = s.particles.readyAck(s.json.optLong("version", -1));
              if (!particleAck.isEmpty()) command("particlesReady", particleAck);
            },
            e -> {
              throw new IllegalStateException(e);
            },
            movies);
    DialogueArt.load(resources);
    renderer.onSurfaceCreated(null, null);
    saveSync =
        new DesktopSaveSync(
            Context.home,
            new DesktopSaveSync.Host() {
              public void begin() throws Exception {
                try {
                  onGl(
                      () -> {
                        if (dialogOpen || syncBusy)
                          throw new IOException("Close the game menu before synchronizing");
                        input.focus(false);
                        inputRegion.cancel();
                        syncBusy = true;
                        NativeBridge.command(engine, "skip", "0");
                        NativeBridge.command(engine, "pause", "");
                        JSONObject prepared =
                            new JSONObject(NativeBridge.command(engine, "backupPrepare", ""));
                        if (!prepared.optString("notice").equals("Save backup ready."))
                          throw new IOException(
                              "Could not flush game progress for synchronization");
                        audio.pause(true);
                        return null;
                      });
                  dialogs.awaitStorage();
                } catch (Exception failure) {
                  end();
                  throw failure;
                }
              }

              public void imported() throws Exception {
                onGl(
                    () -> {
                      JSONObject value =
                          new JSONObject(NativeBridge.command(engine, "progressImport", ""));
                      if (!value.optString("notice").equals("Imported progress refreshed."))
                        throw new IOException("Could not refresh synchronized game progress");
                      return null;
                    });
              }

              public void end() {
                onGl(
                    () -> {
                      syncBusy = false;
                      NativeBridge.command(engine, "resume", "");
                      audio.pause(false);
                      return null;
                    });
              }
            });
    GLFW.glfwSetWindowCloseCallback(
        window,
        w -> {
          if (syncBusy) GLFW.glfwSetWindowShouldClose(window, false);
        });

    bufferWidth = width;
    bufferHeight = height;
    renderer.onSurfaceChanged(null, width, height);
    GLFW.glfwSetFramebufferSizeCallback(
        window,
        (w, x, y) -> {
          if (x > 0 && y > 0) {
            width = x;
            height = y;
          }
        });
    mouseCursor =
        new DesktopCursor(
            System.nanoTime(),
            visible ->
                GLFW.glfwSetInputMode(
                    window,
                    GLFW.GLFW_CURSOR,
                    visible ? GLFW.GLFW_CURSOR_NORMAL : GLFW.GLFW_CURSOR_HIDDEN));
    input =
        new DesktopInput(
            () -> syncBusy || dialogOpen || inputRegion.restricted() || controls.active(),
            () -> {
              mouseCursor.advance(cursorMayHide());
              advance();
            },
            () -> hidden = !hidden,
            () -> dialogs.open("history"),
            this::fullscreen,
            () -> menu = !menu,
            () -> prefs.values.edit().putBoolean("dynamic", !prefs.dynamic()).apply(),
            value -> command("skip", value ? "1" : "0"));
    GLFW.glfwSetKeyCallback(window, (w, key, scan, action, mods) -> input.key(key, action, mods));
    GLFW.glfwSetCursorPosCallback(window, (w, x, y) -> mouseCursor.mouse(System.nanoTime()));
    GLFW.glfwSetCursorEnterCallback(
        window,
        (w, entered) -> {
          if (entered) mouseCursor.mouse(System.nanoTime());
        });
    GLFW.glfwSetScrollCallback(
        window,
        (w, x, y) -> {
          mouseCursor.mouse(System.nanoTime());
          input.wheel(y);
        });
    GLFW.glfwSetWindowFocusCallback(
        window,
        (w, focused) -> {
          windowFocused = focused;
          input.focus(focused);
          if (!focused) mouseCursor.mouse(System.nanoTime());
        });
    GLFW.glfwSetMouseButtonCallback(
        window,
        (w, button, action, mods) -> {
          mouseCursor.mouse(System.nanoTime());
          if (syncBusy || dialogOpen) return;
          if (inputRegion.captures()) {
            if (button == GLFW.GLFW_MOUSE_BUTTON_LEFT
                && (action == GLFW.GLFW_PRESS || action == GLFW.GLFW_RELEASE)) {
              double[] px = {0}, py = {0};
              GLFW.glfwGetCursorPos(window, px, py);
              int[] ww = {0}, wh = {0};
              GLFW.glfwGetWindowSize(window, ww, wh);
              String payload =
                  inputRegion.pointer(
                      action == GLFW.GLFW_PRESS,
                      viewport != null
                          && viewport.content.contains(
                              (float) px[0] * width / Math.max(1, ww[0]),
                              (float) py[0] * height / Math.max(1, wh[0])));
              if (!payload.isEmpty()) command("inputPointer", payload);
            }
            return;
          }
          if (action != GLFW.GLFW_RELEASE) return;
          if (button == GLFW.GLFW_MOUSE_BUTTON_RIGHT) {
            menu = !menu;
            return;
          }
          if (button == GLFW.GLFW_MOUSE_BUTTON_LEFT) {
            double[] x = {0}, y = {0};
            GLFW.glfwGetCursorPos(window, x, y);
            int[] ww = {0}, wh = {0};
            GLFW.glfwGetWindowSize(window, ww, wh);
            click(
                (float) x[0] * width / Math.max(1, ww[0]),
                (float) y[0] * height / Math.max(1, wh[0]));
          }
        });
    if (Boolean.getBoolean("mahoyo.smoke.play")) command("action", "start");
    int frames = 0, movieFrames = 0;
    long sceneNs = 0, overlayNs = 0, totalNs = 0, maxOverlayNs = 0;
    int samples = 0;
    java.util.ArrayList<Long> frameTimes = new java.util.ArrayList<>();
    try {
      while (!GLFW.glfwWindowShouldClose(window)) {
        long frameStarted = System.nanoTime();
        GLFW.glfwPollEvents();
        Runnable pending;
        while ((pending = commands.poll()) != null) pending.run();
        surface.drain();
        if (appliedTextSpeed != prefs.textSpeed()) {
          appliedTextSpeed = prefs.textSpeed();
          command("textSpeed", Integer.toString(appliedTextSpeed));
        }
        if (appliedAutoDelay != prefs.autoDelay()) {
          appliedAutoDelay = prefs.autoDelay();
          command("autoDelay", Integer.toString(appliedAutoDelay));
        }
        String stateJson =
            NativeBridge.command(
                engine, "tick", Long.toString(android.os.SystemClock.uptimeMillis()));
        if (Boolean.getBoolean("mahoyo.smoke.movie")) {
          JSONObject movieState = new JSONObject(stateJson);
          JSONObject testMovie =
              new JSONObject()
                  .put("id", "99999999")
                  .put("path", "video/opening_en.mp4")
                  .put("paused", movieFrames >= 20 && movieFrames < 40)
                  .put("volume", .2);
          if (movieFrames >= 60) testMovie.put("seekSerial", 1).put("seekToMs", 20000);
          movieState.put("movie", testMovie);
          stateJson = movieState.toString();
        }
        if (Boolean.getBoolean("mahoyo.smoke.synthetic")) {
          String synthetic = "<漢字|かんじ>と文字の表示を確認します。".repeat(26);
          int count = 0;
          for (TextOverlay.NativeTextToken token : TextOverlay.NativeTextToken.parse(synthetic))
            count += token.base.codePointCount(0, token.base.length());
          int active = Math.max(0, count - 80),
              visible = active + (int) ((android.os.SystemClock.uptimeMillis() / 25) % 81);
          JSONObject frame = new JSONObject(stateJson);
          frame
              .put("scene", "play")
              .put("module", "synthetic.chs")
              .put("windowAlphaFixed16", 65536)
              .put("language", "ja")
              .put(
                  "text",
                  new JSONArray()
                      .put(
                          new JSONObject()
                              .put("text", synthetic)
                              .put("fontSize", 40)
                              .put("x", 150)
                              .put("y", 100)
                              .put("w", 1620)
                              .put("h", 850)
                              .put("color", -1)
                              .put("activeFrom", active)
                              .put("visibleCharacters", visible)
                              .put("cursor", visible >= count)));
          stateJson = frame.toString();
        }
        scene = new Scene(stateJson);
        controls.state(scene);
        String saveRequest = controls.takeSave();
        if (saveRequest != null)
          dialogs.open("save", () -> commands.add(() -> command("modalAction", saveRequest)));
        JSONObject region = scene.json.optJSONObject("inputRegion");
        inputRegion.update(
            scene.json.optLong("version"),
            region != null && region.optBoolean("active"),
            region != null && region.optBoolean("enabled"),
            region != null && region.optBoolean("readyPending"),
            region == null ? "0" : region.optString("generation", "0"),
            region == null ? "0" : region.optString("request", "0"),
            region == null ? "0" : region.optString("trigger", "0"),
            region == null ? "0" : region.optString("epoch", "0"),
            region == null ? "0" : region.optString("jumpRelease", "0"));
        movies.update(scene.movie, dialogOpen || syncBusy);
        audio.update(scene.json.optJSONArray("audio"));
        audio.updateHistory(scene.json.optJSONObject("historyReplay"));
        if (scene.json.optString("waiting").equals("exit")) break;
        if (!scene.json.optString("error").isEmpty())
          throw new IOException(scene.json.optString("error"));
        int bar = menuBar();
        boolean visibleMenu = showMenu();
        boolean rails = width > height * 16f / 9f;
        int panel =
            text.panelHeight(
                scene,
                prefs,
                width - (visibleMenu && rails ? 2 * bar : 0),
                height - (visibleMenu && !rails ? bar : 0),
                1,
                1);
        viewport =
            new Viewport(
                width, height, bar, visibleMenu, prefs.dynamic(), scene.width, scene.height, panel);
        TextOverlay.RenderState state = text.prepare(scene, viewport, hidden);
        int bw = prefs.renderDimension(width), bh = prefs.renderDimension(height);
        sceneSurface.begin(bw, bh, width, height);
        if (bw != bufferWidth || bh != bufferHeight) {
          bufferWidth = bw;
          bufferHeight = bh;
          renderer.onSurfaceChanged(null, bw, bh);
        }
        renderer.update(scene, viewport, prefs, hidden, state);
        renderer.onDrawFrame(null);
        sceneSurface.present(bw, bh, width, height);
        long sceneFinished = System.nanoTime();
        text.present(scene, viewport, hidden);
        boolean overlayNeeded =
            (!hidden
                    && (!scene.texts.isEmpty()
                        || TextOverlay.fullscreenBackdropAlpha(scene) > 0
                        || viewport.panel.height() > 0))
                || showMenu()
                || controls.visible()
                || syncBusy;
        if (overlayNeeded) {
          String key =
              text.visualKey()
                  + "/"
                  + controls.visualKey()
                  + "/"
                  + showMenu()
                  + "/"
                  + hidden
                  + "/"
                  + width
                  + "x"
                  + height
                  + "/"
                  + syncBusy;
          boolean changed = !key.equals(overlayKey), rasterChanged = false;
          if (changed) {
            if (overlayBitmap == null
                || overlayBitmap.getWidth() != width
                || overlayBitmap.getHeight() != height)
              overlayBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            DesktopCanvas canvas = new DesktopCanvas(overlayBitmap, drawingCache);
            text.draw(canvas);
            cursor = canvas.cursor;
            controls.draw(canvas, viewport);
            if (showMenu()) drawMenu(canvas);
            if (syncBusy) {
              Paint busy = new Paint();
              busy.setColor(0xdd080f1d);
              canvas.drawRect(0, 0, width, height, busy);
              busy.setColor(0xffeff5ff);
              busy.setTextSize(26);
              String label = translations.text("save_sync_busy");
              canvas.drawText(label, (width - busy.measureText(label)) / 2, height / 2f, busy);
            }
            rasterChanged = canvas.render();
            canvas.close();
            overlayKey = key;
          }
          overlay.draw(overlayBitmap, rasterChanged);
          if (!syncBusy) overlay.cursor(cursor, width, height);
        } else {
          overlayKey = "";
          cursor = null;
        }
        long overlayFinished = System.nanoTime();
        mouseCursor.update(cursorMayHide(), System.nanoTime());
        GLFW.glfwSwapBuffers(window);
        if (Boolean.getBoolean("mahoyo.profile")
            && frames >= Integer.getInteger("mahoyo.profile.warmup", 20)) {
          samples++;
          frameTimes.add(System.nanoTime() - frameStarted);
          sceneNs += sceneFinished - frameStarted;
          overlayNs += overlayFinished - sceneFinished;
          totalNs += System.nanoTime() - frameStarted;
          maxOverlayNs = Math.max(maxOverlayNs, overlayFinished - sceneFinished);
        }

        if (movies.ready()) movieFrames++;
        if (Boolean.getBoolean("mahoyo.smoke.movie") && frames > 1800)
          throw new IOException("Movie smoke timed out: " + movies.debug());
        if (Boolean.getBoolean("mahoyo.smoke")
            && ++frames >= Integer.getInteger("mahoyo.smoke.frames", 180)
            && (!Boolean.getBoolean("mahoyo.smoke.movie")
                || movies.ready() && movies.serial() == 1 && movies.position() > 25500)) {
          readback();
          if (samples > 0) {
            java.util.Collections.sort(frameTimes);
            System.out.printf(
                java.util.Locale.ROOT,
                "Frame p95_ms=%.2f p99_ms=%.2f max_ms=%.2f%n",
                frameTimes.get(Math.min(frameTimes.size() - 1, (int) (frameTimes.size() * .95)))
                    / 1e6,
                frameTimes.get(Math.min(frameTimes.size() - 1, (int) (frameTimes.size() * .99)))
                    / 1e6,
                frameTimes.get(frameTimes.size() - 1) / 1e6);
          }
          if (samples > 0)
            System.out.printf(
                java.util.Locale.ROOT,
                "Profile %dx%d samples=%d scene_ms=%.2f overlay_ms=%.2f max_overlay_ms=%.2f"
                    + " fps=%.1f%n",
                width,
                height,
                samples,
                sceneNs / 1e6 / samples,
                overlayNs / 1e6 / samples,
                maxOverlayNs / 1e6,
                samples * 1e9 / totalNs);
          if (Boolean.getBoolean("mahoyo.smoke.movie"))
            System.out.println("Movie smoke " + movies.debug());
          System.out.println(
              "Smoke scene="
                  + scene.json.optString("scene")
                  + " waiting="
                  + scene.json.optString("waiting")
                  + " layers="
                  + scene.layers.size()
                  + " text blocks="
                  + scene.texts.size());
          break;
        }
      }
    } finally {
      if (saveSync != null) saveSync.close();
      dialogs.close();
      audio.close();
      movies.close();
      renderer.shutdown();
      surface.drain();
      overlay.close();
      sceneSurface.close();
      GLFW.glfwDestroyWindow(window);
      GLFW.glfwTerminate();
    }
  }

  private void readback() throws Exception {
    java.nio.ByteBuffer b = java.nio.ByteBuffer.allocateDirect(width * height * 4);
    GL33C.glReadBuffer(GL33C.GL_FRONT);
    GL33C.glReadPixels(0, 0, width, height, GL33C.GL_RGBA, GL33C.GL_UNSIGNED_BYTE, b);
    java.awt.image.BufferedImage image =
        new java.awt.image.BufferedImage(width, height, java.awt.image.BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++) {
        int i = (y * width + x) * 4;
        image.setRGB(
            x,
            height - 1 - y,
            0xff000000
                | ((b.get(i) & 255) << 16)
                | ((b.get(i + 1) & 255) << 8)
                | (b.get(i + 2) & 255));
      }
    javax.imageio.ImageIO.write(image, "png", new java.io.File(".local/windows/title-render.png"));
    System.out.println("Windows shared scene renderer completed a bounded render check");
  }

  private void command(String type, String payload) {
    if (syncBusy && !type.equals("pause")) return;
    NativeBridge.command(engine, type, payload);
  }

  private <T> T onGl(java.util.concurrent.Callable<T> task) {
    try {
      if (Thread.currentThread() == glThread) return task.call();
      java.util.concurrent.CompletableFuture<T> result =
          new java.util.concurrent.CompletableFuture<>();
      commands.add(
          () -> {
            try {
              result.complete(task.call());
            } catch (Throwable e) {
              result.completeExceptionally(e);
            }
          });
      return result.get(30, java.util.concurrent.TimeUnit.SECONDS);
    } catch (Exception e) {
      throw new IllegalStateException(e);
    }
  }

  private JSONObject invoke(String type, String payload) {
    return onGl(() -> new JSONObject(NativeBridge.command(engine, type, payload)));
  }

  private Bitmap snapshotBitmap() {
    ByteBuffer b = ByteBuffer.allocateDirect(width * height * 4);
    GL33C.glReadBuffer(GL33C.GL_FRONT);
    GL33C.glReadPixels(0, 0, width, height, GL33C.GL_RGBA, GL33C.GL_UNSIGNED_BYTE, b);
    Bitmap image = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    int[] pixels = new int[width * height];
    for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++) {
        int i = (y * width + x) * 4;
        pixels[(height - 1 - y) * width + x] =
            0xff000000
                | ((b.get(i) & 255) << 16)
                | ((b.get(i + 1) & 255) << 8)
                | (b.get(i + 2) & 255);
      }
    image.setPixels(pixels, 0, width, 0, 0, width, height);
    GL33C.glReadBuffer(GL33C.GL_BACK);
    return image;
  }

  private void advance() {
    if (hidden) hidden = false;
    else command("advance", "");
  }

  private boolean cursorMayHide() {
    return fullscreen
        && windowFocused
        && !dialogOpen
        && !syncBusy
        && !showMenu()
        && !controls.active()
        && !inputRegion.restricted()
        && scene != null
        && scene.json.optString("scene").equals("play");
  }

  private void click(float x, float y) {
    if (scene == null || viewport == null) return;
    if (controls.click(x, y)) return;
    int item = menuItemAt(x, y);
    if (item >= 0) {
      switch (item) {
        case 0:
          dialogs.open("save");
          break;
        case 1:
          dialogs.open("load");
          break;
        case 2:
          dialogs.open("history");
          break;
        case 3:
          command("auto", "");
          break;
        case 4:
          command("skip", "");
          break;
        case 5:
          hidden = !hidden;
          break;
        case 6:
          command("title", "");
          break;
        case 7:
          dialogs.open("settings");
          break;
      }
      return;
    }
    float dx = (x - viewport.x) / viewport.scale, dy = (y - viewport.y) / viewport.scale;
    for (JSONObject a : scene.actions) {
      if (dx >= a.optDouble("x")
          && dx <= a.optDouble("x") + a.optDouble("w")
          && dy >= a.optDouble("y")
          && dy <= a.optDouble("y") + a.optDouble("h")) {
        String id = a.optString("id");
        if (id.equals("config") || id.equals("settings")) dialogs.open("settings");
        else if (id.equals("load")) dialogs.open("load");
        else if (id.equals("extra")) dialogs.open("extra");
        else command("action", id);
        return;
      }
    }
    advance();
  }

  private int menuBar() {
    return width > height * 16f / 9f ? 76 : 58;
  }

  private boolean showMenu() {
    return menu && scene != null && !scene.wholeFrame;
  }

  private int menuItemAt(float x, float y) {
    if (!showMenu()) return -1;
    int bar = menuBar();
    if (width > height * 16f / 9f) {
      if (x < bar) return Math.min(3, (int) (y * 4 / height));
      if (x >= width - bar) return 4 + Math.min(3, (int) (y * 4 / height));
      return -1;
    }
    return y >= height - bar ? Math.min(7, (int) (x * 8 / width)) : -1;
  }

  private void drawMenu(Canvas canvas) {
    Paint paint = new Paint();
    paint.setColor(0xff17263a);
    int bar = menuBar();
    boolean rails = width > height * 16f / 9f;
    if (rails) {
      canvas.drawRect(0, 0, bar, height, paint);
      canvas.drawRect(width - bar, 0, width, height, paint);
    } else canvas.drawRect(0, height - bar, width, height, paint);
    String[] names = {"Save", "Load", "History", "Auto", "Skip", "Hide", "Title", "Settings"};
    paint.setColor(0xffe5eefb);
    paint.setTextSize(16);
    for (int i = 0; i < 8; i++) {
      float x = rails ? (i < 4 ? bar / 2f : width - bar / 2f) : (i + .5f) * width / 8,
          y = rails ? ((i % 4) + .5f) * height / 4 : height - bar / 2f;
      String label = translations.text(names[i]);
      canvas.drawText(label, x - paint.measureText(label) / 2, y + 6, paint);
    }
  }

  private void fullscreen() {
    if (!fullscreen) {
      int[] x = {0}, y = {0}, w = {0}, h = {0};
      GLFW.glfwGetWindowPos(window, x, y);
      GLFW.glfwGetWindowSize(window, w, h);
      oldX = x[0];
      oldY = y[0];
      oldW = w[0];
      oldH = h[0];
      long monitor = GLFW.glfwGetPrimaryMonitor();
      org.lwjgl.PointerBuffer monitors = GLFW.glfwGetMonitors();
      long largest = -1;
      if (monitors != null)
        for (int i = 0; i < monitors.remaining(); i++) {
          long candidate = monitors.get(i);
          int[] cx = {0}, cy = {0};
          GLFW.glfwGetMonitorPos(candidate, cx, cy);
          GLFWVidMode size = GLFW.glfwGetVideoMode(candidate);
          long overlap =
              (long)
                      Math.max(
                          0, Math.min(oldX + oldW, cx[0] + size.width()) - Math.max(oldX, cx[0]))
                  * Math.max(
                      0, Math.min(oldY + oldH, cy[0] + size.height()) - Math.max(oldY, cy[0]));
          if (overlap > largest) {
            largest = overlap;
            monitor = candidate;
          }
        }

      GLFWVidMode mode = GLFW.glfwGetVideoMode(monitor);
      int[] mx = {0}, my = {0};
      GLFW.glfwGetMonitorPos(monitor, mx, my);
      GLFW.glfwSetWindowAttrib(window, GLFW.GLFW_DECORATED, GLFW.GLFW_FALSE);
      GLFW.glfwSetWindowPos(window, mx[0], my[0]);
      GLFW.glfwSetWindowSize(window, mode.width(), mode.height());
    } else {
      GLFW.glfwSetWindowAttrib(window, GLFW.GLFW_DECORATED, GLFW.GLFW_TRUE);
      GLFW.glfwSetWindowPos(window, oldX, oldY);
      GLFW.glfwSetWindowSize(window, oldW, oldH);
    }
    fullscreen = !fullscreen;
    System.err.println("Window fullscreen=" + fullscreen);
  }
}
