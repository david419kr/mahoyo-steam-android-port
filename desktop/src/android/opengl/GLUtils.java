package android.opengl;

import android.graphics.Bitmap;
import java.awt.image.*;
import java.nio.*;
import org.lwjgl.opengl.GL33C;
import org.lwjgl.system.MemoryUtil;

/**
 * Windows Java2D pixels are already native-endian BGRA; upload them in bulk without channel loops.
 */
public final class GLUtils {
  private static final class Upload {
    ByteBuffer bytes;
    int[] pixels;
  }

  private static final ThreadLocal<Upload> UPLOAD = ThreadLocal.withInitial(Upload::new);

  private static ByteBuffer pixels(Bitmap bitmap, int x, int y, int width, int height) {
    Upload state = UPLOAD.get();
    int count = Math.multiplyExact(width, height), size = Math.multiplyExact(count, 4);
    if (state.bytes == null || state.bytes.capacity() < size) {
      if (state.bytes != null) MemoryUtil.memFree(state.bytes);
      state.bytes = MemoryUtil.memAlloc(size).order(ByteOrder.nativeOrder());
    }
    state.bytes.clear();
    state.bytes.limit(size);
    IntBuffer target = state.bytes.asIntBuffer();
    int type = bitmap.image.getType();
    if ((type == BufferedImage.TYPE_INT_ARGB || type == BufferedImage.TYPE_INT_ARGB_PRE)
        && bitmap.image.getRaster().getDataBuffer() instanceof DataBufferInt) {
      int[] source = ((DataBufferInt) bitmap.image.getRaster().getDataBuffer()).getData();
      for (int row = y; row < y + height; row++)
        target.put(source, row * bitmap.getWidth() + x, width);
    } else {
      if (state.pixels == null || state.pixels.length < count) state.pixels = new int[count];
      bitmap.getPixels(state.pixels, 0, width, x, y, width, height);
      if (bitmap.isPremultiplied())
        for (int i = 0; i < count; i++) {
          int c = state.pixels[i], a = c >>> 24;
          state.pixels[i] =
              (a << 24)
                  | ((((c >>> 16) & 255) * a + 127) / 255 << 16)
                  | ((((c >>> 8) & 255) * a + 127) / 255 << 8)
                  | (((c & 255) * a + 127) / 255);
        }
      target.put(state.pixels, 0, count);
    }
    return state.bytes;
  }

  public static void texImage2D(int target, int level, Bitmap bitmap, int border) {
    GL33C.glTexImage2D(
        target,
        level,
        GL33C.GL_RGBA8,
        bitmap.getWidth(),
        bitmap.getHeight(),
        border,
        GL33C.GL_BGRA,
        GL33C.GL_UNSIGNED_BYTE,
        pixels(bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight()));
  }

  public static void texSubImage2D(
      int target, int level, int x, int y, int width, int height, Bitmap bitmap) {
    GL33C.glTexSubImage2D(
        target,
        level,
        x,
        y,
        width,
        height,
        GL33C.GL_BGRA,
        GL33C.GL_UNSIGNED_BYTE,
        pixels(bitmap, x, y, width, height));
  }

  public static void releaseBuffer() {
    Upload state = UPLOAD.get();
    if (state.bytes != null) MemoryUtil.memFree(state.bytes);
    UPLOAD.remove();
  }

  private GLUtils() {}
}
