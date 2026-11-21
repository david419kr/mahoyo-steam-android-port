package android.opengl;

import java.nio.*;
import org.lwjgl.opengl.GL33C;

public class GLES30 extends GL33C {
  private GLES30() {}

  public static void glShaderSource(int shader, CharSequence source) {
    String s =
        source
            .toString()
            .replace("#version 300 es", "#version 330 core")
            .replace("#extension GL_OES_EGL_image_external_essl3 : require", "")
            .replace("samplerExternalOES", "sampler2D")
            .replaceAll("precision\\s+(?:highp|mediump|lowp)\\s+\\w+;", "")
            .replaceAll("\\b(highp|mediump|lowp)\\b", "");
    GL33C.glShaderSource(shader, s);
  }

  public static void glGenTextures(int n, int[] v, int o) {
    int[] a = new int[n];
    GL33C.glGenTextures(a);
    System.arraycopy(a, 0, v, o, n);
  }

  public static void glGenFramebuffers(int n, int[] v, int o) {
    int[] a = new int[n];
    GL33C.glGenFramebuffers(a);
    System.arraycopy(a, 0, v, o, n);
  }

  public static void glDeleteTextures(int n, int[] v, int o) {
    GL33C.glDeleteTextures(java.util.Arrays.copyOfRange(v, o, o + n));
  }

  public static void glDeleteFramebuffers(int n, int[] v, int o) {
    GL33C.glDeleteFramebuffers(java.util.Arrays.copyOfRange(v, o, o + n));
  }

  public static void glGenSamplers(int n, int[] v, int o) {
    int[] a = new int[n];
    GL33C.glGenSamplers(a);
    System.arraycopy(a, 0, v, o, n);
  }

  public static void glDeleteSamplers(int n, int[] v, int o) {
    GL33C.glDeleteSamplers(java.util.Arrays.copyOfRange(v, o, o + n));
  }

  public static void glGetIntegerv(int p, int[] v, int o) {
    int[] a = new int[v.length - o];
    GL33C.glGetIntegerv(p, a);
    System.arraycopy(a, 0, v, o, a.length);
  }

  public static void glGetShaderiv(int s, int p, int[] v, int o) {
    v[o] = GL33C.glGetShaderi(s, p);
  }

  public static void glGetProgramiv(int s, int p, int[] v, int o) {
    v[o] = GL33C.glGetProgrami(s, p);
  }

  public static void glGetFramebufferAttachmentParameteriv(int t, int a, int p, int[] v, int o) {
    if (GL33C.glGetInteger(GL33C.GL_READ_FRAMEBUFFER_BINDING) == 0 && a == GL33C.GL_BACK)
      a = GL33C.GL_BACK_LEFT;
    v[o] = GL33C.glGetFramebufferAttachmentParameteri(t, a, p);
  }

  public static void glUniform1fv(int l, int n, float[] v, int o) {
    GL33C.glUniform1fv(l, java.util.Arrays.copyOfRange(v, o, o + n));
  }

  public static void glUniform4fv(int l, int n, float[] v, int o) {
    GL33C.glUniform4fv(l, java.util.Arrays.copyOfRange(v, o, o + 4 * n));
  }

  public static void glUniformMatrix4fv(int l, int n, boolean transpose, float[] v, int o) {
    GL33C.glUniformMatrix4fv(l, transpose, java.util.Arrays.copyOfRange(v, o, o + 16 * n));
  }

  public static void glTexImage2D(
      int t, int level, int format, int w, int h, int border, int f, int type, Buffer data) {
    GL33C.glTexImage2D(t, level, format, w, h, border, f, type, (ByteBuffer) data);
  }
}
