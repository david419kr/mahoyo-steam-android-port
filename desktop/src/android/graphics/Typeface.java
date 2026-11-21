package android.graphics;

import java.awt.Font;

public class Typeface {
  public static final int NORMAL = 0, BOLD = 1, ITALIC = 2;
  public static final Typeface SERIF = new Typeface("Serif", 0),
      DEFAULT = new Typeface("Dialog", 0);
  final String name;
  final int style;
  private final java.util.Map<Float, Font> fonts = new java.util.concurrent.ConcurrentHashMap<>();

  Typeface(String n, int s) {
    name = n;
    style = s;
  }

  public static Typeface create(Typeface base, int style) {
    return new Typeface(base == null ? "Serif" : base.name, style);
  }

  public static Typeface create(String name, int style) {
    return new Typeface(name, style);
  }

  Font font(float size) {
    return fonts.computeIfAbsent(
        size, value -> new Font(name, style, Math.max(1, Math.round(value))).deriveFont(value));
  }
}
