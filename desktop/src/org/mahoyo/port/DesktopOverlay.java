package org.mahoyo.port;

import android.graphics.*;
import android.opengl.GLUtils;
import java.awt.image.DataBufferInt;
import java.nio.*;
import java.util.Arrays;
import org.lwjgl.opengl.GL33C;

/**
 * Retained UI texture: only changed pixels cross to the GPU; the animated cursor is a tiny quad.
 */
final class DesktopOverlay implements AutoCloseable {
  private int texture, cursorTexture, program, position, uv, sampler, opacity, width, height;
  private Bitmap cursorImage;
  private int[] previous;
  private final FloatBuffer quad =
      ByteBuffer.allocateDirect(16 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();

  private void ensure() {
    if (program != 0) return;
    int
        v =
            shader(
                GL33C.GL_VERTEX_SHADER,
                "#version 330 core\n"
                    + "in vec2 p;in vec2 uv;out vec2 t;void main(){t=uv;gl_Position=vec4(p,0,1);}"),
        f =
            shader(
                GL33C.GL_FRAGMENT_SHADER,
                "#version 330 core\n"
                    + "uniform sampler2D image;uniform float alpha;in vec2 t;out vec4 color;void"
                    + " main(){color=texture(image,t)*alpha;}");
    program = GL33C.glCreateProgram();
    GL33C.glAttachShader(program, v);
    GL33C.glAttachShader(program, f);
    GL33C.glLinkProgram(program);
    if (GL33C.glGetProgrami(program, GL33C.GL_LINK_STATUS) == 0)
      throw new IllegalStateException(GL33C.glGetProgramInfoLog(program));
    GL33C.glDeleteShader(v);
    GL33C.glDeleteShader(f);
    position = GL33C.glGetAttribLocation(program, "p");
    uv = GL33C.glGetAttribLocation(program, "uv");
    sampler = GL33C.glGetUniformLocation(program, "image");
    opacity = GL33C.glGetUniformLocation(program, "alpha");
    texture = GL33C.glGenTextures();
  }

  void draw(Bitmap image, boolean changed) {
    ensure();
    GL33C.glActiveTexture(GL33C.GL_TEXTURE0);
    GL33C.glDisable(GL33C.GL_SCISSOR_TEST);
    GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, texture);
    if (changed || width != image.getWidth() || height != image.getHeight()) update(image);
    drawQuad(texture, 0, 0, width, height, 0, 0, 1, 1, width, height, 1);
  }

  private void update(Bitmap image) {
    int w = image.getWidth(), h = image.getHeight();
    int[] pixels = ((DataBufferInt) image.image.getRaster().getDataBuffer()).getData();
    if (width != w || height != h || previous == null) {
      width = w;
      height = h;
      previous = pixels.clone();
      linear();
      GLUtils.texImage2D(GL33C.GL_TEXTURE_2D, 0, image, 0);
      return;
    }
    int left = w, top = h, right = 0, bottom = 0;
    for (int y = 0; y < h; y++) {
      int offset = y * w,
          first = Arrays.mismatch(pixels, offset, offset + w, previous, offset, offset + w);
      if (first < 0) continue;
      int last = w - 1;
      while (last > first && pixels[offset + last] == previous[offset + last]) last--;
      left = Math.min(left, first);
      right = Math.max(right, last + 1);
      top = Math.min(top, y);
      bottom = y + 1;
    }
    if (left >= right || top >= bottom) return;
    GLUtils.texSubImage2D(GL33C.GL_TEXTURE_2D, 0, left, top, right - left, bottom - top, image);
    for (int y = top; y < bottom; y++)
      System.arraycopy(pixels, y * w + left, previous, y * w + left, right - left);
  }

  void cursor(DesktopCanvas.Cursor cursor, int width, int height) {
    if (cursor == null || cursor.alpha <= 0) return;
    ensure();
    if (cursorTexture == 0) cursorTexture = GL33C.glGenTextures();
    GL33C.glActiveTexture(GL33C.GL_TEXTURE0);
    GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, cursorTexture);
    if (cursorImage != cursor.image) {
      cursorImage = cursor.image;
      linear();
      GLUtils.texImage2D(GL33C.GL_TEXTURE_2D, 0, cursor.image, 0);
    }
    Rect source = DialogueArt.cursorFrame(true);
    RectF dst = cursor.bounds;
    if (cursor.clip != null) {
      RectF c = cursor.clip;
      int l = Math.max(0, Math.round(c.left)),
          r = Math.min(width, Math.round(c.right)),
          t = Math.max(0, Math.round(c.top)),
          b = Math.min(height, Math.round(c.bottom));
      if (r <= l || b <= t) return;
      GL33C.glEnable(GL33C.GL_SCISSOR_TEST);
      GL33C.glScissor(l, height - b, r - l, b - t);
    }
    drawQuad(
        cursorTexture,
        dst.left,
        dst.top,
        dst.right,
        dst.bottom,
        (source.left + .5f) / cursor.image.getWidth(),
        (source.top + .5f) / cursor.image.getHeight(),
        (source.right - .5f) / cursor.image.getWidth(),
        (source.bottom - .5f) / cursor.image.getHeight(),
        width,
        height,
        cursor.alpha);
    GL33C.glDisable(GL33C.GL_SCISSOR_TEST);
  }

  private void drawQuad(
      int texture,
      float left,
      float top,
      float right,
      float bottom,
      float u0,
      float v0,
      float u1,
      float v1,
      int w,
      int h,
      float alpha) {
    GL33C.glUseProgram(program);
    GL33C.glEnable(GL33C.GL_BLEND);
    GL33C.glBlendFunc(GL33C.GL_ONE, GL33C.GL_ONE_MINUS_SRC_ALPHA);
    GL33C.glActiveTexture(GL33C.GL_TEXTURE0);
    GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, texture);
    GL33C.glUniform1i(sampler, 0);
    GL33C.glUniform1f(opacity, alpha);
    float l = left / w * 2 - 1, r = right / w * 2 - 1, t = 1 - top / h * 2, b = 1 - bottom / h * 2;
    quad.clear();
    quad.put(new float[] {l, t, u0, v0, l, b, u0, v1, r, t, u1, v0, r, b, u1, v1}).flip();
    GL33C.glVertexAttribPointer(position, 2, GL33C.GL_FLOAT, false, 16, quad);
    GL33C.glEnableVertexAttribArray(position);
    quad.position(2);
    GL33C.glVertexAttribPointer(uv, 2, GL33C.GL_FLOAT, false, 16, quad);
    GL33C.glEnableVertexAttribArray(uv);
    GL33C.glDrawArrays(GL33C.GL_TRIANGLE_STRIP, 0, 4);
  }

  private static void linear() {
    GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MIN_FILTER, GL33C.GL_LINEAR);
    GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_MAG_FILTER, GL33C.GL_LINEAR);
    GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_WRAP_S, GL33C.GL_CLAMP_TO_EDGE);
    GL33C.glTexParameteri(GL33C.GL_TEXTURE_2D, GL33C.GL_TEXTURE_WRAP_T, GL33C.GL_CLAMP_TO_EDGE);
  }

  public void close() {
    if (texture != 0) GL33C.glDeleteTextures(texture);
    if (cursorTexture != 0) GL33C.glDeleteTextures(cursorTexture);
    if (program != 0) GL33C.glDeleteProgram(program);
    GLUtils.releaseBuffer();
  }

  private static int shader(int type, String source) {
    int shader = GL33C.glCreateShader(type);
    GL33C.glShaderSource(shader, source);
    GL33C.glCompileShader(shader);
    if (GL33C.glGetShaderi(shader, GL33C.GL_COMPILE_STATUS) == 0)
      throw new IllegalStateException(GL33C.glGetShaderInfoLog(shader));
    return shader;
  }
}
