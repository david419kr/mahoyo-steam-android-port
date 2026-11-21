package org.mahoyo.port;

import android.content.Context;
import java.awt.image.BufferedImage;
import java.io.*;
import java.nio.ByteBuffer;
import java.util.*;
import javax.imageio.ImageIO;
import org.lwjgl.glfw.*;
import org.lwjgl.system.MemoryUtil;

/** Uses every original size so Windows can choose its small and large window icons. */
final class DesktopIcon {
  static void apply(long window) throws IOException {
    File[] files =
        new File(Context.assets, "icons")
            .listFiles((dir, name) -> name.startsWith("launcher-") && name.endsWith(".png"));
    if (files == null || files.length == 0)
      throw new IOException("The launcher icon is missing. Extract the complete Windows package.");
    Arrays.sort(files, Comparator.comparing(File::getName));
    List<ByteBuffer> pixels = new ArrayList<>();
    try (GLFWImage.Buffer icons = GLFWImage.malloc(files.length)) {
      for (int i = 0; i < files.length; i++) {
        BufferedImage image = ImageIO.read(files[i]);
        if (image == null)
          throw new IOException("Cannot read launcher icon: " + files[i].getName());
        int w = image.getWidth(), h = image.getHeight();
        ByteBuffer rgba = MemoryUtil.memAlloc(w * h * 4);
        pixels.add(rgba);
        for (int color : image.getRGB(0, 0, w, h, null, 0, w))
          rgba.put((byte) (color >>> 16))
              .put((byte) (color >>> 8))
              .put((byte) color)
              .put((byte) (color >>> 24));
        rgba.flip();
        icons.get(i).width(w).height(h).pixels(rgba);
      }
      GLFW.glfwSetWindowIcon(window, icons);
    } finally {
      for (ByteBuffer buffer : pixels) MemoryUtil.memFree(buffer);
    }
  }

  private DesktopIcon() {}
}
