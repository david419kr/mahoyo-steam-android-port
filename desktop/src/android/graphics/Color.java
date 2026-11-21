package android.graphics;

public class Color {
  public static final int WHITE = -1, BLACK = 0xff000000, TRANSPARENT = 0;

  public static int red(int c) {
    return (c >>> 16) & 255;
  }

  public static int green(int c) {
    return (c >>> 8) & 255;
  }

  public static int blue(int c) {
    return c & 255;
  }

  public static int alpha(int c) {
    return c >>> 24;
  }

  public static int argb(int a, int r, int g, int b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
  }

  public static int rgb(int r, int g, int b) {
    return argb(255, r, g, b);
  }

  public static int parseColor(String s) {
    long n = Long.parseLong(s.substring(1), 16);
    return (int) (s.length() == 7 ? n | 0xff000000L : n);
  }
}
