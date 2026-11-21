package org.mahoyo.port;

import android.graphics.*;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.concurrent.*;
import java.util.zip.CRC32;
import javax.swing.*;
import org.lwjgl.glfw.*;
import org.lwjgl.opengl.*;
import org.lwjgl.system.windows.User32;

/** Own hidden windows and authored saves only; never touches the user's game progress. */
public final class DesktopWindowUxCheck {
  public static void main(String[] args) throws Exception {
    DesktopMenuFocus focus = new DesktopMenuFocus();
    check(focus.selected() == -1, "menu opens neutral");
    focus.move(0, -1);
    check(focus.selected() == -1, "neutral up is not a history shortcut");
    focus.move(-1, 0);
    check(focus.selected() == 0, "left starts at Save");
    focus.move(0, 1);
    check(focus.selected() == 1, "left down selects Load");
    focus.move(1, 0);
    check(focus.selected() == -1, "inward right returns to neutral");
    focus.move(1, 0);
    check(focus.selected() == 4, "second right selects the right rail");
    focus.move(0, 1);
    check(focus.selected() == 5, "right down selects Hide");
    focus.move(-1, 0);
    check(focus.selected() == -1, "inward left returns to neutral");
    focus.clear();
    Object menu = new Object();
    try (DesktopGamepad pad =
        new DesktopGamepad(
            a -> {
              if (a == DesktopGamepad.Action.LEFT) focus.move(-1, 0);
              if (a == DesktopGamepad.Action.RIGHT) focus.move(1, 0);
            },
            v -> {})) {
      pad.update(0, 0, true, menu, DesktopGamepad.VERTICAL, false, 0);
      int left = 1 << GLFW.GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
          right = 1 << GLFW.GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
      pad.update(0, left, true, menu, DesktopGamepad.VERTICAL, false, 1);
      pad.update(0, 0, true, menu, DesktopGamepad.VERTICAL, false, 2);
      pad.update(0, right, true, menu, DesktopGamepad.VERTICAL, false, 3);
      pad.update(0, right, true, menu, DesktopGamepad.VERTICAL, false, 1_000_000_000L);
      check(focus.selected() == -1, "holding inward direction cannot cross neutral");
      pad.update(0, 0, true, menu, DesktopGamepad.VERTICAL, false, 1_100_000_000L);
      pad.update(0, right, true, menu, DesktopGamepad.VERTICAL, false, 1_200_000_000L);
      check(focus.selected() == 4, "a fresh press crosses neutral");
    }
    if (!GLFW.glfwInit()) throw new AssertionError("GLFW init");
    GLFW.glfwWindowHint(GLFW.GLFW_VISIBLE, GLFW.GLFW_FALSE);
    long window = GLFW.glfwCreateWindow(640, 360, "window-ux-check", 0, 0);
    if (window == 0) throw new AssertionError("Hidden window");
    GLFW.glfwMakeContextCurrent(window);
    GL.createCapabilities();
    try {
      int[] width = {0}, height = {0};
      GLFW.glfwGetFramebufferSize(window, width, height);
      GL33C.glBindFramebuffer(GL33C.GL_FRAMEBUFFER, 0);
      GL33C.glDrawBuffer(GL33C.GL_BACK);
      GL33C.glDisable(GL33C.GL_SCISSOR_TEST);
      GL33C.glClearColor(0, 0, 1, 1);
      GL33C.glClear(GL33C.GL_COLOR_BUFFER_BIT);
      GL33C.glEnable(GL33C.GL_SCISSOR_TEST);
      GL33C.glScissor(0, height[0] / 2, width[0], height[0] / 2);
      GL33C.glClearColor(1, 0, 0, 1);
      GL33C.glClear(GL33C.GL_COLOR_BUFFER_BIT);
      Bitmap image =
          DesktopThumbnail.capture(
              width[0], height[0], new RectF(width[0] / 8f, 0, width[0] * 7 / 8f, height[0]));
      check(
          image.getWidth() == 320 && image.getHeight() == 180, "thumbnail has bounded dimensions");
      check(
          (image.getPixel(160, 35) & 0xffffff) == 0xff0000,
          "top rows are captured from the completed back buffer");
      check((image.getPixel(160, 145) & 0xffffff) == 0x0000ff, "capture preserves orientation");
      check((image.getPixel(0, 90) & 0xffffff) == 0, "thumbnail preserves aspect ratio");
      check(GL33C.glIsEnabled(GL33C.GL_SCISSOR_TEST), "capture restores scissor state");
      check(GL33C.glGetError() == GL33C.GL_NO_ERROR, "capture has no GL errors");
      thumbnailRoundTrip(image);
      try (DesktopOverlays overlays = new DesktopOverlays(window)) {
        long game = GLFWNativeWin32.glfwGetWin32Window(window);
        for (int i = 0; i < 12; i++) {
          GLFW.glfwSetWindowAttrib(
              window, GLFW.GLFW_DECORATED, i % 2 == 0 ? GLFW.GLFW_FALSE : GLFW.GLFW_TRUE);
          SwingUtilities.invokeAndWait(
              () -> {
                JDialog root = new JDialog((java.awt.Frame) null, "overlay-check"),
                    child = new JDialog(root, "confirm-check");
                root.setSize(300, 200);
                child.setSize(200, 100);
                try {
                  overlays.prepare(root, false);
                  overlays.prepare(child, false);
                  check(
                      User32.GetWindowLongPtr(null, overlays.handle(root), User32.GWL_HWNDPARENT)
                          == game,
                      "root overlay owns the actual game HWND");
                  check(
                      User32.GetWindowLongPtr(null, overlays.handle(child), User32.GWL_HWNDPARENT)
                          == overlays.handle(root),
                      "confirmation stays above its dialog");
                  check(
                      !root.isAlwaysOnTop() && !child.isAlwaysOnTop(),
                      "overlays do not become global topmost");
                } finally {
                  child.dispose();
                  root.dispose();
                }
              });
        }
      }
    } finally {
      GLFW.glfwDestroyWindow(window);
      GLFW.glfwTerminate();
    }
    System.out.println(
        "Neutral/rail controller focus, separate horizontal presses, GPU thumbnail/save ZIP and"
            + " repeated native dialog ownership passed");
  }

  private static void thumbnailRoundTrip(Bitmap image) throws Exception {
    File root = Files.createTempDirectory(Path.of(".local/windows"), "thumbnail-check-").toFile(),
        saves = new File(root, "saves"),
        thumbs = new File(root, "thumbs");
    saves.mkdirs();
    Files.write(new File(saves, "slot1.whs").toPath(), save());
    SlotThumbnailStore store = new SlotThumbnailStore(saves, thumbs);
    try {
      store.saved(1, image);
      CountDownLatch written = new CountDownLatch(1);
      store.transfer(written::countDown);
      check(written.await(10, TimeUnit.SECONDS), "thumbnail write completes");
      check(new File(thumbs, "slot1.thumb").isFile(), "thumbnail sidecar exists");
      assertThumbnail(store);
      ByteArrayOutputStream zip = new ByteArrayOutputStream();
      try (SaveArchive.Archive snapshot = SaveArchive.snapshot(saves, thumbs, root)) {
        SaveArchive.write(snapshot, zip, "window-ux-test");
      }
      File restored = new File(root, "restored"),
          restoredThumbs = new File(root, "restored-thumbs");
      try (SaveArchive.Archive imported =
          SaveArchive.read(new ByteArrayInputStream(zip.toByteArray()), root)) {
        SaveArchive.apply(
            SaveArchive.exactPlan(imported, restored, restoredThumbs),
            new File(root, "journal"),
            true,
            () -> {});
      }
      SlotThumbnailStore reopened = new SlotThumbnailStore(restored, restoredThumbs);
      try {
        assertThumbnail(reopened);
      } finally {
        reopened.close();
      }
    } finally {
      store.close();
    }
  }

  private static void assertThumbnail(SlotThumbnailStore store) throws Exception {
    CountDownLatch listed = new CountDownLatch(1);
    boolean[] valid = {false};
    store.list(
        entries -> {
          SlotThumbnailStore.Entry slot = entries.getFirst();
          valid[0] =
              slot.thumbnail != null
                  && slot.thumbnail.getWidth() == 320
                  && slot.thumbnail.getHeight() == 180
                  && ((slot.thumbnail.getPixel(160, 35) >>> 16) & 255) > 220;
          listed.countDown();
        });
    check(
        listed.await(10, TimeUnit.SECONDS) && valid[0],
        "saved/restored thumbnail decodes and retains captured image");
  }

  private static byte[] save() {
    ByteBuffer b = ByteBuffer.allocate(52).order(ByteOrder.LITTLE_ENDIAN);
    b.put("WOHSAVE2".getBytes(java.nio.charset.StandardCharsets.US_ASCII))
        .putInt(36)
        .putInt(0)
        .putInt(4)
        .putInt(17);
    for (int i = 0; i < 7; i++) b.putInt(0);
    CRC32 crc = new CRC32();
    crc.update(b.array(), 16, 36);
    b.putInt(12, (int) crc.getValue());
    return b.array();
  }

  private static void check(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }
}
