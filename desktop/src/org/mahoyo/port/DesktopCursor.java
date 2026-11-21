package org.mahoyo.port;

import java.util.function.Consumer;

/** Hides the pointer without capturing or moving it. Menus and focus loss restore it. */
final class DesktopCursor {
  static final long IDLE_NS = 2_000_000_000L;
  private final Consumer<Boolean> visibility;
  private long lastMouse;
  private boolean visible = true;

  DesktopCursor(long now, Consumer<Boolean> visibility) {
    lastMouse = now;
    this.visibility = visibility;
  }

  void mouse(long now) {
    lastMouse = now;
    show(true);
  }

  void advance(boolean eligible) {
    if (eligible) {
      lastMouse = Long.MIN_VALUE;
      show(false);
    }
  }

  void update(boolean eligible, long now) {
    show(!eligible || lastMouse != Long.MIN_VALUE && now - lastMouse < IDLE_NS);
  }

  private void show(boolean value) {
    if (visible != value) {
      visible = value;
      visibility.accept(value);
    }
  }
}
