package org.mahoyo.port;

import android.graphics.*;
import java.awt.AlphaComposite;
import java.awt.Graphics2D;
import java.awt.Rectangle;
import java.awt.Shape;
import java.awt.geom.*;
import java.util.*;

/**
 * Retained software display list. Unchanged glyphs are not rasterized again on each reveal tick.
 */
final class DesktopCanvas extends Canvas {
  static final class Cursor {
    final Bitmap image;
    final RectF bounds, clip;
    final float alpha;

    Cursor(Bitmap image, RectF bounds, RectF clip, float alpha) {
      this.image = image;
      this.bounds = bounds;
      this.clip = clip;
      this.alpha = alpha;
    }
  }

  private record Key(
      String type,
      Object data,
      int color,
      float font,
      Typeface face,
      Paint.Style style,
      float stroke,
      Shader shader,
      AffineTransform transform,
      Rectangle clip) {}

  private record Text(String text, float x, float y) {}

  private record Box(float l, float t, float r, float b, float rx, float ry) {}

  private record Picture(
      Bitmap bitmap, int l, int t, int r, int b, float x, float y, float right, float bottom) {}

  private record Stamp(Key key, Rectangle bounds) {}

  private record Command(Stamp stamp, Graphics2D state, Runnable draw) {}

  static final class Cache {
    Bitmap image;
    java.util.List<Stamp> previous = java.util.List.of();
  }

  private final Bitmap image;
  private final Cache cache;
  private final java.util.List<Command> commands = new ArrayList<>();
  Cursor cursor;

  DesktopCanvas(Bitmap image, Cache cache) {
    super(image);
    this.image = image;
    this.cache = cache;
  }

  private void record(String type, Object data, Rectangle2D local, Paint paint, Runnable draw) {
    AffineTransform transform = graphics().getTransform();
    Rectangle bounds = transform.createTransformedShape(local).getBounds();
    bounds.grow(2, 2);
    Shape clip = graphics().getClip();
    if (clip != null)
      bounds = bounds.intersection(transform.createTransformedShape(clip).getBounds());
    bounds = bounds.intersection(new Rectangle(0, 0, image.getWidth(), image.getHeight()));
    if (bounds.isEmpty()) return;
    boolean text = type.equals("text"), picture = type.equals("image");
    Paint.Style style =
        picture ? Paint.Style.FILL : type.equals("line") ? Paint.Style.STROKE : paint.getStyle();
    Shader shader = picture ? null : paint.getShader();
    int color = picture ? (paint.getColor() & 0xff000000) : shader == null ? paint.getColor() : 0;
    Key key =
        new Key(
            type,
            data,
            color,
            text ? paint.getTextSize() : 0,
            text ? paint.getTypeface() : null,
            style,
            style == Paint.Style.STROKE ? paint.getStrokeWidth() : 0,
            shader,
            transform,
            new Rectangle(bounds));
    commands.add(new Command(new Stamp(key, bounds), (Graphics2D) graphics().create(), draw));
  }

  @Override
  public void drawBitmap(Bitmap bitmap, Rect source, RectF destination, Paint original) {
    Paint paint = original == null ? new Paint() : new Paint(original);
    Rect src =
        source == null
            ? new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight())
            : new Rect(source.left, source.top, source.right, source.bottom);
    RectF dst = new RectF(destination);
    if (DialogueArt.cursorImage(bitmap)) {
      Rectangle2D r =
          graphics()
              .getTransform()
              .createTransformedShape(
                  new Rectangle2D.Float(dst.left, dst.top, dst.width(), dst.height()))
              .getBounds2D();
      RectF bounds =
          new RectF(
              (float) r.getMinX(), (float) r.getMinY(), (float) r.getMaxX(), (float) r.getMaxY());
      Shape local = graphics().getClip();
      RectF clip = null;
      if (local != null) {
        Rectangle2D c = graphics().getTransform().createTransformedShape(local).getBounds2D();
        clip =
            new RectF(
                (float) c.getMinX(), (float) c.getMinY(), (float) c.getMaxX(), (float) c.getMaxY());
      }
      cursor = new Cursor(bitmap, bounds, clip, (paint.getColor() >>> 24) / 255f);
      return;
    }
    record(
        "image",
        new Picture(
            bitmap,
            src.left,
            src.top,
            src.right,
            src.bottom,
            dst.left,
            dst.top,
            dst.right,
            dst.bottom),
        new Rectangle2D.Float(dst.left, dst.top, dst.width(), dst.height()),
        paint,
        () -> DesktopCanvas.super.drawBitmap(bitmap, src, dst, paint));
  }

  @Override
  public void drawText(String text, float x, float y, Paint original) {
    Paint paint = new Paint(original);
    Rectangle2D outline = textOutline(text, paint).getBounds2D(),
        logical = paint.font().getStringBounds(text, graphics().getFontRenderContext());
    float stroke = paint.getStyle() == Paint.Style.STROKE ? paint.getStrokeWidth() : 0;
    Rectangle2D local = outline.createUnion(logical);
    local =
        new Rectangle2D.Double(
            local.getX() + x - stroke,
            local.getY() + y - stroke,
            local.getWidth() + 2 * stroke,
            local.getHeight() + 2 * stroke);
    record(
        "text",
        new Text(text, x, y),
        local,
        paint,
        () -> DesktopCanvas.super.drawText(text, x, y, paint));
  }

  @Override
  public void drawRect(float l, float t, float r, float b, Paint original) {
    Paint paint = new Paint(original);
    float extra = paint.getStyle() == Paint.Style.STROKE ? paint.getStrokeWidth() : 0;
    record(
        "rect",
        new Box(l, t, r, b, 0, 0),
        new Rectangle2D.Float(l - extra, t - extra, r - l + 2 * extra, b - t + 2 * extra),
        paint,
        () -> DesktopCanvas.super.drawRect(l, t, r, b, paint));
  }

  @Override
  public void drawRoundRect(
      float l, float t, float r, float b, float rx, float ry, Paint original) {
    Paint paint = new Paint(original);
    record(
        "round",
        new Box(l, t, r, b, rx, ry),
        new Rectangle2D.Float(
            l - paint.getStrokeWidth(),
            t - paint.getStrokeWidth(),
            r - l + 2 * paint.getStrokeWidth(),
            b - t + 2 * paint.getStrokeWidth()),
        paint,
        () -> DesktopCanvas.super.drawRoundRect(l, t, r, b, rx, ry, paint));
  }

  @Override
  public void drawLine(float x, float y, float x2, float y2, Paint original) {
    Paint paint = new Paint(original);
    record(
        "line",
        new Box(x, y, x2, y2, 0, 0),
        new Rectangle2D.Float(
            Math.min(x, x2) - paint.getStrokeWidth(),
            Math.min(y, y2) - paint.getStrokeWidth(),
            Math.abs(x2 - x) + 2 * paint.getStrokeWidth(),
            Math.abs(y2 - y) + 2 * paint.getStrokeWidth()),
        paint,
        () -> DesktopCanvas.super.drawLine(x, y, x2, y2, paint));
  }

  boolean render() {
    java.util.List<Stamp> next = new ArrayList<>();
    Map<Key, Integer> oldCounts = new HashMap<>(), newCounts = new HashMap<>();
    for (Stamp old : cache.previous) oldCounts.merge(old.key, 1, Integer::sum);
    for (Command c : commands) {
      next.add(c.stamp);
      newCounts.merge(c.stamp.key, 1, Integer::sum);
    }
    Rectangle dirty =
        cache.image == image ? null : new Rectangle(0, 0, image.getWidth(), image.getHeight());
    for (Stamp old : cache.previous)
      if (!Objects.equals(oldCounts.get(old.key), newCounts.get(old.key)))
        dirty = union(dirty, old.bounds);
    for (Stamp value : next)
      if (!Objects.equals(oldCounts.get(value.key), newCounts.get(value.key)))
        dirty = union(dirty, value.bounds);
    java.util.List<Key> oldOrder = new ArrayList<>(), newOrder = new ArrayList<>();
    for (Stamp old : cache.previous) if (newCounts.containsKey(old.key)) oldOrder.add(old.key);
    for (Stamp value : next) if (oldCounts.containsKey(value.key)) newOrder.add(value.key);
    if (!oldOrder.equals(newOrder)) {
      for (Stamp old : cache.previous) dirty = union(dirty, old.bounds);
      for (Stamp value : next) dirty = union(dirty, value.bounds);
    }
    try {
      if (dirty != null && !dirty.isEmpty()) {
        dirty = dirty.intersection(new Rectangle(0, 0, image.getWidth(), image.getHeight()));
        Graphics2D clear = image.image.createGraphics();
        clear.setComposite(AlphaComposite.Clear);
        clear.fillRect(dirty.x, dirty.y, dirty.width, dirty.height);
        clear.dispose();
        Graphics2D original = graphics();
        for (Command command : commands)
          if (command.stamp.bounds.intersects(dirty)) {
            Graphics2D paint = (Graphics2D) command.state.create();
            try {
              paint.clip(paint.getTransform().createInverse().createTransformedShape(dirty));
              graphics(paint);
              command.draw.run();
            } catch (NoninvertibleTransformException invalid) {
              throw new IllegalStateException(invalid);
            } finally {
              graphics(original);
              paint.dispose();
            }
          }
      }
      cache.image = image;
      cache.previous = next;
      return dirty != null && !dirty.isEmpty();
    } finally {
      for (Command command : commands) command.state.dispose();
      commands.clear();
    }
  }

  private static Rectangle union(Rectangle first, Rectangle second) {
    return first == null ? new Rectangle(second) : first.union(second);
  }
}
