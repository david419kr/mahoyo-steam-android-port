package android.graphics;

import java.awt.*;
import java.awt.geom.*;
import java.util.*;

public class Canvas {
  public void close() {
    while (!stack.isEmpty()) restore();
    g.dispose();
  }

  private record Outline(java.awt.Font font, String text) {}

  private static final java.util.Map<Outline, Shape> OUTLINES =
      java.util.Collections.synchronizedMap(
          new java.util.LinkedHashMap<Outline, Shape>(256, .75f, true) {
            protected boolean removeEldestEntry(java.util.Map.Entry<Outline, Shape> e) {
              return size() > 4096;
            }
          });

  private record GlyphKey(
      java.awt.Font font,
      String text,
      int color,
      Paint.Style style,
      float stroke,
      double x,
      double y) {}

  private record Glyph(java.awt.image.BufferedImage pixels, int x, int y) {}

  private static final LinkedHashMap<GlyphKey, Glyph> GLYPHS = new LinkedHashMap<>(256, .75f, true);
  private static long glyphPixels;
  private Graphics2D g;

  protected final Graphics2D graphics() {
    return g;
  }

  protected final void graphics(Graphics2D next) {
    g = next;
  }

  protected final Shape textOutline(String text, Paint paint) {
    return OUTLINES.computeIfAbsent(
        new Outline(paint.font(), text),
        key -> key.font().createGlyphVector(g.getFontRenderContext(), key.text()).getOutline());
  }

  private final Deque<Graphics2D> stack = new ArrayDeque<>();

  public Canvas(Bitmap bitmap) {
    g = bitmap.image.createGraphics();
    hints();
  }

  private void hints() {
    g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    g.setRenderingHint(
        RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
    g.setRenderingHint(
        RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
    g.setRenderingHint(
        RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
  }

  public int save() {
    stack.push(g);
    g = (Graphics2D) g.create();
    return stack.size();
  }

  public void restore() {
    g.dispose();
    g = stack.pop();
  }

  public void restoreToCount(int n) {
    while (stack.size() >= n) restore();
  }

  public void translate(float x, float y) {
    g.translate(x, y);
  }

  public void scale(float x, float y) {
    g.scale(x, y);
  }

  public boolean clipRect(RectF r) {
    return clipRect(r.left, r.top, r.right, r.bottom);
  }

  public boolean clipRect(float l, float t, float r, float b) {
    g.clip(new Rectangle2D.Float(l, t, Math.max(0, r - l), Math.max(0, b - t)));
    return true;
  }

  private void use(Paint p) {
    g.setColor(new java.awt.Color(p.getColor(), true));
    g.setPaint(
        p.getShader() == null ? new java.awt.Color(p.getColor(), true) : p.getShader().paint);
    g.setStroke(
        new BasicStroke(
            Math.max(.01f, p.getStrokeWidth()), BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
    g.setFont(p.font());
  }

  private void shape(Shape s, Paint p) {
    use(p);
    if (p.getStyle() == Paint.Style.STROKE) g.draw(s);
    else g.fill(s);
  }

  public void drawRect(RectF r, Paint p) {
    drawRect(r.left, r.top, r.right, r.bottom, p);
  }

  public void drawRect(float l, float t, float r, float b, Paint p) {
    shape(new Rectangle2D.Float(l, t, r - l, b - t), p);
  }

  public void drawRoundRect(RectF r, float rx, float ry, Paint p) {
    drawRoundRect(r.left, r.top, r.right, r.bottom, rx, ry, p);
  }

  public void drawRoundRect(float l, float t, float r, float b, float rx, float ry, Paint p) {
    shape(new RoundRectangle2D.Float(l, t, r - l, b - t, rx * 2, ry * 2), p);
  }

  public void drawLine(float x, float y, float x2, float y2, Paint p) {
    use(p);
    g.draw(new Line2D.Float(x, y, x2, y2));
  }

  public void drawText(String s, float x, float y, Paint p) {
    use(p);
    AffineTransform transform = g.getTransform();
    if (s.length() <= 8
        && p.getShader() == null
        && transform.getScaleX() == 1
        && transform.getScaleY() == 1
        && transform.getShearX() == 0
        && transform.getShearY() == 0
        && g.getComposite().equals(AlphaComposite.SrcOver)) {
      double dx = x + transform.getTranslateX(), dy = y + transform.getTranslateY();
      GlyphKey key =
          new GlyphKey(
              p.font(),
              s,
              p.getColor(),
              p.getStyle(),
              p.getStyle() == Paint.Style.STROKE ? p.getStrokeWidth() : 0,
              dx - Math.floor(dx),
              dy - Math.floor(dy));
      Glyph glyph;
      synchronized (GLYPHS) {
        glyph = GLYPHS.get(key);
      }
      if (glyph == null) {
        Rectangle2D bounds =
            textOutline(s, p)
                .getBounds2D()
                .createUnion(p.font().getStringBounds(s, g.getFontRenderContext()));
        bounds =
            new Rectangle2D.Double(
                bounds.getX() + x, bounds.getY() + y, bounds.getWidth(), bounds.getHeight());
        Rectangle device = transform.createTransformedShape(bounds).getBounds();
        device.grow(2 + (int) Math.ceil(key.stroke()), 2 + (int) Math.ceil(key.stroke()));
        java.awt.image.BufferedImage image =
            new java.awt.image.BufferedImage(
                Math.max(1, device.width),
                Math.max(1, device.height),
                java.awt.image.BufferedImage.TYPE_INT_ARGB_PRE);
        Graphics2D paint = image.createGraphics();
        paint.setRenderingHints(g.getRenderingHints());
        paint.setFont(p.font());
        paint.setColor(new java.awt.Color(p.getColor(), true));
        paint.setStroke(g.getStroke());
        paint.translate(-device.x, -device.y);
        paint.transform(transform);
        if (p.getStyle() == Paint.Style.STROKE) {
          paint.translate(x, y);
          paint.draw(textOutline(s, p));
        } else paint.drawString(s, x, y);
        paint.dispose();
        glyph = new Glyph(image, device.x - (int) Math.floor(dx), device.y - (int) Math.floor(dy));
        synchronized (GLYPHS) {
          Glyph old = GLYPHS.put(key, glyph);
          if (old != null) glyphPixels -= (long) old.pixels().getWidth() * old.pixels().getHeight();
          glyphPixels += (long) image.getWidth() * image.getHeight();
          while (GLYPHS.size() > 4096 || glyphPixels > 12 * 1024 * 1024) {
            var iterator = GLYPHS.entrySet().iterator();
            Glyph removed = iterator.next().getValue();
            glyphPixels -= (long) removed.pixels().getWidth() * removed.pixels().getHeight();
            iterator.remove();
          }
        }
      }
      g.setTransform(new AffineTransform());
      g.drawImage(
          glyph.pixels(), (int) Math.floor(dx) + glyph.x(), (int) Math.floor(dy) + glyph.y(), null);
      g.setTransform(transform);
      return;
    }
    if (p.getStyle() == Paint.Style.STROKE) {
      Shape outline = textOutline(s, p);
      AffineTransform old = g.getTransform();
      g.translate(x, y);
      g.draw(outline);
      g.setTransform(old);
    } else g.drawString(s, x, y);
  }

  public void drawBitmap(Bitmap bitmap, Rect src, RectF dst, Paint p) {
    Graphics2D old = g;
    g = (Graphics2D) g.create();
    if (p != null) g.setComposite(AlphaComposite.SrcOver.derive(Color.alpha(p.getColor()) / 255f));
    Rect s = src == null ? new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight()) : src;
    g.drawImage(
        bitmap.image,
        Math.round(dst.left),
        Math.round(dst.top),
        Math.round(dst.right),
        Math.round(dst.bottom),
        s.left,
        s.top,
        s.right,
        s.bottom,
        null);
    g.dispose();
    g = old;
  }

  public void drawBitmap(Bitmap b, float x, float y, Paint p) {
    drawBitmap(b, null, new RectF(x, y, x + b.getWidth(), y + b.getHeight()), p);
  }

  public void drawColor(int c) {
    g.setColor(new java.awt.Color(c, true));
    g.fillRect(0, 0, 100000, 100000);
  }
}
