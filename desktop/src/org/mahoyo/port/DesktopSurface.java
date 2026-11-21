package org.mahoyo.port;

import org.lwjgl.opengl.GL33C;

/** A low-resolution scene surface; text and desktop controls remain at window resolution. */
final class DesktopSurface implements AutoCloseable {
  private int framebuffer, texture, width, height;

  void begin(int width, int height, int actualWidth, int actualHeight) {
    if (width == actualWidth && height == actualHeight) {
      GL33C.glBindFramebuffer(GL33C.GL_FRAMEBUFFER, 0);
      return;
    }
    if (framebuffer == 0 || this.width != width || this.height != height) {
      close();
      this.width = width;
      this.height = height;
      texture = GL33C.glGenTextures();
      GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, texture);
      GL33C.glTexImage2D(
          GL33C.GL_TEXTURE_2D,
          0,
          GL33C.GL_RGBA8,
          width,
          height,
          0,
          GL33C.GL_RGBA,
          GL33C.GL_UNSIGNED_BYTE,
          (java.nio.ByteBuffer) null);
      GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MIN_FILTER, GL33C.GL_LINEAR);
      GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MAG_FILTER, GL33C.GL_LINEAR);
      framebuffer = GL33C.glGenFramebuffers();
      GL33C.glBindFramebuffer(GL33C.GL_FRAMEBUFFER, framebuffer);
      GL33C.glFramebufferTexture2D(
          GL33C.GL_FRAMEBUFFER, GL33C.GL_COLOR_ATTACHMENT0, GL33C.GL_TEXTURE_2D, texture, 0);
      if (GL33C.glCheckFramebufferStatus(GL33C.GL_FRAMEBUFFER) != GL33C.GL_FRAMEBUFFER_COMPLETE)
        throw new IllegalStateException("Incomplete desktop scene surface");
    }
    GL33C.glBindFramebuffer(GL33C.GL_FRAMEBUFFER, framebuffer);
  }

  void present(int bufferWidth, int bufferHeight, int windowWidth, int windowHeight) {
    if (bufferWidth != windowWidth || bufferHeight != windowHeight) {
      GL33C.glDisable(GL33C.GL_SCISSOR_TEST);
      GL33C.glBindFramebuffer(GL33C.GL_READ_FRAMEBUFFER, framebuffer);
      GL33C.glBindFramebuffer(GL33C.GL_DRAW_FRAMEBUFFER, 0);
      GL33C.glBlitFramebuffer(
          0,
          0,
          bufferWidth,
          bufferHeight,
          0,
          0,
          windowWidth,
          windowHeight,
          GL33C.GL_COLOR_BUFFER_BIT,
          GL33C.GL_LINEAR);
      GL33C.glBindFramebuffer(GL33C.GL_FRAMEBUFFER, 0);
    }
    GL33C.glViewport(0, 0, windowWidth, windowHeight);
  }

  public void close() {
    if (framebuffer != 0) GL33C.glDeleteFramebuffers(framebuffer);
    if (texture != 0) GL33C.glDeleteTextures(texture);
    framebuffer = texture = 0;
  }
}
