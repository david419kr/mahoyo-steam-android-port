package org.mahoyo.port;

import android.graphics.*;

final class BitmapProbe {
  static void check(GameResources resources) throws Exception {
    Bitmap image = resources.file("images/title_bg1.cbg.png").bitmap(new BitmapFactory.Options());
    if (image.getWidth() != 1920 || image.getHeight() != 1080)
      throw new IllegalStateException("Native title bitmap dimensions");
    System.out.println(
        "Windows original image decode ready: " + image.getWidth() + "x" + image.getHeight());
  }
}
