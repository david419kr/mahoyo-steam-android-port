package org.mahoyo.port;

import android.graphics.*;
import java.nio.ByteBuffer;
import org.lwjgl.opengl.GL33C;
import org.lwjgl.system.MemoryUtil;

/**
 * Called before swapping the completed back buffer; never reads an occluded/undefined front buffer.
 */
final class DesktopThumbnail {
  static Bitmap capture(int width, int height, RectF content) {
    int oldRead = GL33C.glGetInteger(GL33C.GL_READ_FRAMEBUFFER_BINDING),
        oldDraw = GL33C.glGetInteger(GL33C.GL_DRAW_FRAMEBUFFER_BINDING),
        oldReadBuffer = GL33C.glGetInteger(GL33C.GL_READ_BUFFER),
        oldTexture = GL33C.glGetInteger(GL33C.GL_TEXTURE_BINDING_2D),
        oldPack = GL33C.glGetInteger(GL33C.GL_PIXEL_PACK_BUFFER_BINDING),
        oldUnpack = GL33C.glGetInteger(GL33C.GL_PIXEL_UNPACK_BUFFER_BINDING);
    float[] clear = new float[4];
    GL33C.glGetFloatv(GL33C.GL_COLOR_CLEAR_VALUE, clear);
    boolean scissor = GL33C.glIsEnabled(GL33C.GL_SCISSOR_TEST);
    int framebuffer = GL33C.glGenFramebuffers(), texture = GL33C.glGenTextures();
    ByteBuffer rgba = MemoryUtil.memAlloc(320 * 180 * 4);
    try {
      GL33C.glBindBuffer(GL33C.GL_PIXEL_PACK_BUFFER, 0);
      GL33C.glBindBuffer(GL33C.GL_PIXEL_UNPACK_BUFFER, 0);
      GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, texture);
      GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MIN_FILTER, GL33C.GL_LINEAR);
      GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MAG_FILTER, GL33C.GL_LINEAR);
      GL33C.glTexImage2D(
          GL33C.GL_TEXTURE_2D,
          0,
          GL33C.GL_RGBA8,
          320,
          180,
          0,
          GL33C.GL_RGBA,
          GL33C.GL_UNSIGNED_BYTE,
          (ByteBuffer) null);
      GL33C.glBindFramebuffer(GL33C.GL_DRAW_FRAMEBUFFER, framebuffer);
      GL33C.glFramebufferTexture2D(
          GL33C.GL_DRAW_FRAMEBUFFER, GL33C.GL_COLOR_ATTACHMENT0, GL33C.GL_TEXTURE_2D, texture, 0);
      GL33C.glDrawBuffer(GL33C.GL_COLOR_ATTACHMENT0);
      if (GL33C.glCheckFramebufferStatus(GL33C.GL_DRAW_FRAMEBUFFER)
          != GL33C.GL_FRAMEBUFFER_COMPLETE)
        throw new IllegalStateException("Cannot create save thumbnail surface");
      GL33C.glDisable(GL33C.GL_SCISSOR_TEST);
      GL33C.glClearColor(0, 0, 0, 1);
      GL33C.glClear(GL33C.GL_COLOR_BUFFER_BIT);
      int l = Math.max(0, Math.round(content.left)),
          r = Math.min(width, Math.round(content.right)),
          top = Math.max(0, Math.round(content.top)),
          bottom = Math.min(height, Math.round(content.bottom));
      if (r <= l || bottom <= top) throw new IllegalArgumentException("Empty save viewport");
      float scale = Math.min(320f / (r - l), 180f / (bottom - top));
      int w = Math.round((r - l) * scale),
          h = Math.round((bottom - top) * scale),
          x = (320 - w) / 2,
          y = (180 - h) / 2;
      GL33C.glBindFramebuffer(GL33C.GL_READ_FRAMEBUFFER, 0);
      GL33C.glReadBuffer(GL33C.GL_BACK);
      GL33C.glBlitFramebuffer(
          l,
          height - bottom,
          r,
          height - top,
          x,
          y,
          x + w,
          y + h,
          GL33C.GL_COLOR_BUFFER_BIT,
          GL33C.GL_LINEAR);
      GL33C.glBindFramebuffer(GL33C.GL_READ_FRAMEBUFFER, framebuffer);
      GL33C.glReadBuffer(GL33C.GL_COLOR_ATTACHMENT0);
      GL33C.glReadPixels(0, 0, 320, 180, GL33C.GL_RGBA, GL33C.GL_UNSIGNED_BYTE, rgba);
      int[] pixels = new int[320 * 180];
      for (int row = 0; row < 180; row++)
        for (int col = 0; col < 320; col++) {
          int i = (row * 320 + col) * 4;
          pixels[(179 - row) * 320 + col] =
              0xff000000
                  | ((rgba.get(i) & 255) << 16)
                  | ((rgba.get(i + 1) & 255) << 8)
                  | (rgba.get(i + 2) & 255);
        }
      return Bitmap.createBitmap(pixels, 320, 180, Bitmap.Config.ARGB_8888);
    } finally {
      GL33C.glBindFramebuffer(GL33C.GL_READ_FRAMEBUFFER, oldRead);
      GL33C.glReadBuffer(oldReadBuffer);
      GL33C.glBindFramebuffer(GL33C.GL_DRAW_FRAMEBUFFER, oldDraw);
      if (scissor) GL33C.glEnable(GL33C.GL_SCISSOR_TEST);
      GL33C.glDeleteFramebuffers(framebuffer);
      GL33C.glDeleteTextures(texture);
      GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, oldTexture);
      GL33C.glBindBuffer(GL33C.GL_PIXEL_PACK_BUFFER, oldPack);
      GL33C.glBindBuffer(GL33C.GL_PIXEL_UNPACK_BUFFER, oldUnpack);
      GL33C.glClearColor(clear[0], clear[1], clear[2], clear[3]);
      MemoryUtil.memFree(rgba);
    }
  }

  private DesktopThumbnail() {}
}
