package org.mahoyo.port;

import android.graphics.*;
import java.awt.*;
import java.awt.geom.*;
import java.awt.image.*;
import java.nio.*;
import org.lwjgl.glfw.GLFW;
import org.lwjgl.opengl.*;

/** Checks cached text against Java2D and the bulk upload's actual OpenGL pixels. */
public final class DesktopRasterCheck {
  public static void main(String[] args) {
    int max = 0;
    for (int alpha : new int[] {255, 128, 37})
      for (float phase : new float[] {0, .25f, .75f})
        for (android.graphics.Paint.Style style : android.graphics.Paint.Style.values()) {
          Bitmap actual = Bitmap.createBitmap(400, 130, Bitmap.Config.ARGB_8888);
          android.graphics.Paint p = new android.graphics.Paint();
          p.setColor((alpha << 24) | 0xeac987);
          p.setTextSize(36);
          p.setStyle(style);
          p.setStrokeWidth(2);
          android.graphics.Canvas canvas = new android.graphics.Canvas(actual);
          canvas.clipRect(8, 8, 375, 110);
          canvas.translate(3.25f, 1.5f);
          canvas.drawText("Aa日本語루비", 10 + phase, 65 + phase, p);
          canvas.close();
          BufferedImage reference = new BufferedImage(400, 130, BufferedImage.TYPE_INT_ARGB_PRE);
          Graphics2D g = reference.createGraphics();
          g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
          g.setRenderingHint(
              RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
          g.setRenderingHint(
              RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
          g.clip(new Rectangle(8, 8, 367, 102));
          g.translate(3.25, 1.5);
          g.setColor(new java.awt.Color(p.getColor(), true));
          g.setFont(p.font());
          g.setStroke(new BasicStroke(2, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
          if (style == android.graphics.Paint.Style.STROKE) {
            g.translate(10 + phase, 65 + phase);
            g.draw(p.font().createGlyphVector(g.getFontRenderContext(), "Aa日本語루비").getOutline());
          } else g.drawString("Aa日本語루비", 10 + phase, 65 + phase);
          g.dispose();
          int[] a = ((DataBufferInt) actual.image.getRaster().getDataBuffer()).getData(),
              b = ((DataBufferInt) reference.getRaster().getDataBuffer()).getData();
          for (int i = 0; i < a.length; i++)
            for (int shift : new int[] {0, 8, 16, 24}) {
              int difference = Math.abs(((a[i] >>> shift) & 255) - ((b[i] >>> shift) & 255));
              max = Math.max(max, difference);
              if (difference > 2)
                throw new AssertionError(
                    "Cached text changed pixels: "
                        + difference
                        + " alpha="
                        + alpha
                        + " phase="
                        + phase
                        + " style="
                        + style);
            }
        }
    Bitmap retained = Bitmap.createBitmap(500, 180, Bitmap.Config.ARGB_8888);
    DesktopCanvas.Cache cache = new DesktopCanvas.Cache();
    for (int frame = 0; frame < 8; frame++) {
      Bitmap reference = Bitmap.createBitmap(500, 180, Bitmap.Config.ARGB_8888);
      android.graphics.Canvas expectedCanvas = new android.graphics.Canvas(reference);
      DesktopCanvas actualCanvas = new DesktopCanvas(retained, cache);
      for (android.graphics.Canvas c :
          new android.graphics.Canvas[] {expectedCanvas, actualCanvas}) {
        android.graphics.Paint p = new android.graphics.Paint();
        p.setColor(0x99306090);
        c.drawRect(0, 0, 500, 180, p);
        c.save();
        c.clipRect(23, 10, 470, 164);
        c.translate(frame >= 4 ? -.5f : 0, frame >= 5 ? -22 : 0);
        p.setColor(frame >= 3 ? 0x8055aaff : 0xff55aaff);
        p.setTextSize(30);
        String value = "Aa日本語루비".substring(0, Math.min(7, frame + 1));
        c.drawText(value, 30.25f, 65.75f, p);
        p.setColor(0xff000000);
        p.setStyle(android.graphics.Paint.Style.STROKE);
        p.setStrokeWidth(2);
        c.drawText(value, 30.25f, 65.75f, p);
        c.restore();
      }
      actualCanvas.render();
      actualCanvas.close();
      expectedCanvas.close();
      if (!java.util.Arrays.equals(
          ((DataBufferInt) retained.image.getRaster().getDataBuffer()).getData(),
          ((DataBufferInt) reference.image.getRaster().getDataBuffer()).getData()))
        throw new AssertionError("Retained raster differs at frame " + frame);
    }
    if (!GLFW.glfwInit()) throw new AssertionError("GLFW");
    GLFW.glfwWindowHint(GLFW.GLFW_VISIBLE, GLFW.GLFW_FALSE);
    long window = GLFW.glfwCreateWindow(32, 32, "raster-check", 0, 0);
    GLFW.glfwMakeContextCurrent(window);
    GL.createCapabilities();
    try {
      Bitmap b = Bitmap.createBitmap(3, 2, Bitmap.Config.ARGB_8888);
      int[] pixels = ((DataBufferInt) b.image.getRaster().getDataBuffer()).getData();
      int[] expected = {0xffff0000, 0xff00ff00, 0xff0000ff, 0x80402010, 0, 0xff7a92be};
      System.arraycopy(expected, 0, pixels, 0, 6);
      int texture = GL33C.glGenTextures();
      GL33C.glBindTexture(GL33C.GL_TEXTURE_2D, texture);
      android.opengl.GLUtils.texImage2D(GL33C.GL_TEXTURE_2D, 0, b, 0);
      int[] read = new int[6];
      GL33C.glGetTexImage(GL33C.GL_TEXTURE_2D, 0, GL33C.GL_BGRA, GL33C.GL_UNSIGNED_BYTE, read);
      if (!java.util.Arrays.equals(expected, read))
        throw new AssertionError("BGRA upload changed pixels");
      pixels[4] = 0x90503020;
      expected[4] = pixels[4];
      android.opengl.GLUtils.texSubImage2D(GL33C.GL_TEXTURE_2D, 0, 1, 1, 1, 1, b);
      GL33C.glGetTexImage(GL33C.GL_TEXTURE_2D, 0, GL33C.GL_BGRA, GL33C.GL_UNSIGNED_BYTE, read);
      if (!java.util.Arrays.equals(expected, read))
        throw new AssertionError("Dirty upload changed other pixels");
      GL33C.glDeleteTextures(texture);
      try(DesktopSurface surface=new DesktopSurface()){
        surface.begin(16,16,32,32);GL33C.glDisable(GL33C.GL_SCISSOR_TEST);GL33C.glClearColor(.25f,.5f,.75f,1);GL33C.glClear(GL33C.GL_COLOR_BUFFER_BIT);surface.present(16,16,32,32);
        ByteBuffer pixel=ByteBuffer.allocateDirect(4);GL33C.glReadPixels(31,31,1,1,GL33C.GL_RGBA,GL33C.GL_UNSIGNED_BYTE,pixel);
        if(Math.abs((pixel.get(0)&255)-64)>1||Math.abs((pixel.get(1)&255)-128)>1||Math.abs((pixel.get(2)&255)-191)>1)throw new AssertionError("50% scene surface did not fill native output");
        if(GL33C.glGetError()!=GL33C.GL_NO_ERROR)throw new AssertionError("OpenGL error in the scene/overlay adapters");
      }
    } finally {
      android.opengl.GLUtils.releaseBuffer();
      GLFW.glfwDestroyWindow(window);
      GLFW.glfwTerminate();
    }
    System.out.println(
        "Cached text parity max channel difference=" + max + "/255; BGRA full/dirty upload passed");
  }
}
