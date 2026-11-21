package org.mahoyo.port;

import android.content.Context;
import android.content.SharedPreferences;

/** Window dimensions refer to the complete client viewport, including in-game menus. */
final class DesktopWindowState {
  static final int NATIVE_WIDTH = 1920, NATIVE_HEIGHT = 1080;
  static final int[] PRESETS = {50, 75, 100, 150, 200};

  record Size(int width, int height) {}

  private final SharedPreferences values;

  DesktopWindowState(Context context) {
    values = context.getSharedPreferences("window", 0);
  }

  Size size() {
    int w = values.getInt("width", NATIVE_WIDTH), h = values.getInt("height", NATIVE_HEIGHT);
    return w > 0 && h > 0 ? new Size(w, h) : preset(100);
  }

  boolean fullscreen() {
    return values.getBoolean("fullscreen", false);
  }

  void save(int width, int height, boolean fullscreen) {
    values
        .edit()
        .putInt("width", width)
        .putInt("height", height)
        .putBoolean("fullscreen", fullscreen)
        .apply();
  }

  static Size preset(int percent) {
    for (int allowed : PRESETS)
      if (allowed == percent)
        return new Size(NATIVE_WIDTH * percent / 100, NATIVE_HEIGHT * percent / 100);
    throw new IllegalArgumentException("Unsupported window size preset");
  }

  static int percent(int width, int height) {
    for (int p : PRESETS) {
      Size size = preset(p);
      if (size.width == width && size.height == height) return p;
    }
    return -1;
  }
}
