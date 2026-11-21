package android.graphics;

import java.awt.*;
import java.awt.image.*;
import java.io.*;
import java.nio.*;
import javax.imageio.ImageIO;

public class Bitmap {
  public enum Config {
    ARGB_8888,
    ALPHA_8,
    RGB_565
  }

  public enum CompressFormat {
    PNG,
    JPEG,
    WEBP
  }

  public BufferedImage image;
  private boolean premultiplied = true;

  public Bitmap(BufferedImage image) {
    this.image = image;
    premultiplied = image.isAlphaPremultiplied();
  }

  public static Bitmap createBitmap(int w, int h, Config c) {
    return new Bitmap(new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB_PRE));
  }

  public static Bitmap createBitmap(int[] p, int w, int h, Config c) {
    Bitmap b = new Bitmap(new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB));
    b.image.setRGB(0, 0, w, h, p, 0, w);
    return b;
  }

  public static Bitmap createBitmap(Bitmap source, int x, int y, int w, int h) {
    Bitmap b = createBitmap(w, h, Config.ARGB_8888);
    b.image.getGraphics().drawImage(source.image, 0, 0, w, h, x, y, x + w, y + h, null);
    return b;
  }

  public static Bitmap createScaledBitmap(Bitmap src, int w, int h, boolean filter) {
    Bitmap b = createBitmap(w, h, Config.ARGB_8888);
    Graphics2D g = b.image.createGraphics();
    g.setRenderingHint(
        RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
    g.drawImage(src.image, 0, 0, w, h, null);
    g.dispose();
    return b;
  }

  public int getWidth() {
    return image.getWidth();
  }

  public int getHeight() {
    return image.getHeight();
  }

  public int getByteCount() {
    return getWidth() * getHeight() * 4;
  }

  public int getAllocationByteCount() {
    return getByteCount();
  }

  public int getRowBytes() {
    return getWidth() * 4;
  }

  public void recycle() {}

  public boolean isRecycled() {
    return false;
  }

  public void setPremultiplied(boolean v) {
    image.coerceData(v);
    premultiplied = v;
  }

  public boolean isPremultiplied() {
    return premultiplied;
  }

  public boolean hasAlpha() {
    return true;
  }

  public void setHasAlpha(boolean v) {}

  public Config getConfig() {
    return Config.ARGB_8888;
  }

  public void getPixels(int[] dst, int offset, int stride, int x, int y, int w, int h) {
    image.getRGB(x, y, w, h, dst, offset, stride);
  }

  public void setPixels(int[] src, int offset, int stride, int x, int y, int w, int h) {
    image.setRGB(x, y, w, h, src, offset, stride);
  }

  public int getPixel(int x, int y) {
    return image.getRGB(x, y);
  }

  public void eraseColor(int color) {
    Graphics2D g = image.createGraphics();
    g.setComposite(AlphaComposite.Src);
    g.setColor(new java.awt.Color(color, true));
    g.fillRect(0, 0, getWidth(), getHeight());
    g.dispose();
  }

  public boolean compress(CompressFormat format, int quality, OutputStream out) {
    try {
      BufferedImage encoded = image;
      if (format == CompressFormat.JPEG) {
        encoded = new BufferedImage(getWidth(), getHeight(), BufferedImage.TYPE_INT_RGB);
        Graphics2D g = encoded.createGraphics();
        g.drawImage(image, 0, 0, null);
        g.dispose();
      }
      return ImageIO.write(encoded, format == CompressFormat.JPEG ? "jpg" : "png", out);
    } catch (IOException e) {
      return false;
    }
  }
}
