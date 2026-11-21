package org.mahoyo.port;

import android.graphics.*;
import java.awt.*;
import java.awt.geom.*;

/** Small vector glyphs rasterized once at their actual display size. */
final class DesktopMenuIcons {
  private static final Bitmap[] ICONS = new Bitmap[8];

  static Bitmap icon(int index) {
    if (ICONS[index] != null) return ICONS[index];
    Bitmap bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
    Graphics2D g = bitmap.image.createGraphics();
    g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    g.scale(32 / 24., 32 / 24.);
    g.setColor(new java.awt.Color(0xe5eefb));
    g.setStroke(new BasicStroke(1.7f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
    switch (index) {
      case 0, 1 -> {
        line(g, 4, 15, 4, 21, 20, 21, 20, 15);
        line(g, 12, index == 0 ? 3 : 16, 12, index == 0 ? 16 : 3);
        line(g, 7, index == 0 ? 11 : 8, 12, index == 0 ? 16 : 3, 17, index == 0 ? 11 : 8);
      }
      case 2 -> {
        for (int y : new int[] {5, 12, 19}) {
          g.draw(new Ellipse2D.Double(3, y - 1, 2, 2));
          line(g, 9, y, 21, y);
        }
      }
      case 3 -> line(g, 8, 4, 20, 12, 8, 20, 8, 4);
      case 4 -> {
        line(g, 3, 4, 12, 12, 3, 20, 3, 4);
        line(g, 13, 4, 22, 12, 13, 20, 13, 4);
      }
      case 5 -> {
        Path2D eye = new Path2D.Double();
        eye.moveTo(2, 12);
        eye.curveTo(6, 3, 18, 3, 22, 12);
        eye.curveTo(18, 21, 6, 21, 2, 12);
        g.draw(eye);
        g.draw(new Ellipse2D.Double(9, 9, 6, 6));
        line(g, 3, 3, 21, 21);
      }
      case 6 -> {
        line(g, 2, 11, 12, 3, 22, 11);
        line(g, 5, 9, 5, 21, 19, 21, 19, 9);
        line(g, 9, 21, 9, 14, 15, 14, 15, 21);
      }
      case 7 -> {
        Path2D gear = new Path2D.Double();
        for (int i = 0; i < 32; i++) {
          double angle = i * Math.PI / 16 - Math.PI / 2,
              r = i % 4 == 1 || i % 4 == 2 ? 10 : 8,
              x = 12 + Math.cos(angle) * r,
              y = 12 + Math.sin(angle) * r;
          if (i == 0) gear.moveTo(x, y);
          else gear.lineTo(x, y);
        }
        gear.closePath();
        g.draw(gear);
        g.draw(new Ellipse2D.Double(8.5, 8.5, 7, 7));
      }
      default -> throw new IllegalArgumentException("Unknown menu icon");
    }
    g.dispose();
    return ICONS[index] = bitmap;
  }

  private static void line(Graphics2D g, double... xy) {
    Path2D path = new Path2D.Double();
    path.moveTo(xy[0], xy[1]);
    for (int i = 2; i < xy.length; i += 2) path.lineTo(xy[i], xy[i + 1]);
    g.draw(path);
  }

  private DesktopMenuIcons() {}
}
