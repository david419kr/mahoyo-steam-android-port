package android.graphics;

import java.awt.*;
import java.awt.font.*;
import java.awt.geom.*;
import java.util.Locale;

public class Paint {
  public static final int ANTI_ALIAS_FLAG = 1, FILTER_BITMAP_FLAG = 2, SUBPIXEL_TEXT_FLAG = 128;

  public enum Style {
    FILL,
    STROKE
  }

  public enum Join {
    ROUND,
    MITER,
    BEVEL
  }

  public enum Align {
    LEFT,
    CENTER,
    RIGHT
  }

  public static class FontMetrics {
    public float top, ascent, descent, bottom, leading;
  }

  private int color = 0xff000000;
  private float size = 16, stroke = 1;
  private Typeface face = Typeface.SERIF;
  private Style style = Style.FILL;
  private Locale locale = Locale.getDefault();
  private Shader shader;

  public Paint() {}

  public Paint(Paint other) {
    color = other.color;
    size = other.size;
    stroke = other.stroke;
    face = other.face;
    style = other.style;
    locale = other.locale;
    shader = other.shader;
  }

  public Paint(int flags) {}

  public void setColor(int c) {
    color = c;
  }

  public int getColor() {
    return color;
  }

  public void setAlpha(int a) {
    color = (color & 0xffffff) | (a << 24);
  }

  public void setTextSize(float s) {
    size = s;
  }

  public float getTextSize() {
    return size;
  }

  public void setTypeface(Typeface t) {
    face = t;
  }

  public Typeface getTypeface() {
    return face;
  }

  public void setStyle(Style s) {
    style = s;
  }

  public Style getStyle() {
    return style;
  }

  public void setStrokeWidth(float v) {
    stroke = v;
  }

  public float getStrokeWidth() {
    return stroke;
  }

  public void setStrokeJoin(Join j) {}

  public void setAntiAlias(boolean v) {}

  public void setSubpixelText(boolean v) {}

  public void setFilterBitmap(boolean v) {}

  public void setTextLocale(Locale v) {
    locale = v;
  }

  public Locale getTextLocale() {
    return locale;
  }

  public void setTextScaleX(float v) {}

  public void setLetterSpacing(float v) {}

  public void setTextAlign(Align v) {}

  public void setShadowLayer(float r, float x, float y, int c) {}

  public void clearShadowLayer() {}

  public Shader setShader(Shader s) {
    Shader old = shader;
    shader = s;
    return old;
  }

  public Shader getShader() {
    return shader;
  }

  public java.awt.Font font() {
    return (face == null ? Typeface.SERIF : face).font(size);
  }

  private FontRenderContext frc() {
    return new FontRenderContext(null, true, true);
  }

  public float measureText(String s) {
    return (float) font().getStringBounds(s, frc()).getWidth();
  }

  public float measureText(String s, int start, int end) {
    return measureText(s.substring(start, end));
  }

  public float ascent() {
    return -font().getLineMetrics("Ag", frc()).getAscent();
  }

  public float descent() {
    return font().getLineMetrics("Ag", frc()).getDescent();
  }

  public FontMetrics getFontMetrics() {
    FontMetrics m = new FontMetrics();
    m.top = m.ascent = ascent();
    m.bottom = m.descent = descent();
    return m;
  }

  public float getFontMetrics(FontMetrics out) {
    FontMetrics m = getFontMetrics();
    out.top = m.top;
    out.bottom = m.bottom;
    out.ascent = m.ascent;
    out.descent = m.descent;
    return m.descent - m.ascent;
  }

  public void getTextBounds(String s, int start, int end, Rect out) {
    Rectangle2D r = font().createGlyphVector(frc(), s.substring(start, end)).getVisualBounds();
    out.left = (int) Math.floor(r.getMinX());
    out.top = (int) Math.floor(r.getMinY());
    out.right = (int) Math.ceil(r.getMaxX());
    out.bottom = (int) Math.ceil(r.getMaxY());
  }
}
