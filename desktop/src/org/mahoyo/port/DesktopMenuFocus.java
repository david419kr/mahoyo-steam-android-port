package org.mahoyo.port;

/** The visible rails do not take game input until a direction explicitly selects a side. */
final class DesktopMenuFocus {
  private int selected = -1;

  int selected() {
    return selected;
  }

  void clear() {
    selected = -1;
  }

  void move(int dx, int dy) {
    if (selected < 0) {
      if (dx < 0) selected = 0;
      else if (dx > 0) selected = 4;
      return;
    }
    int first = selected < 4 ? 0 : 4;
    if (dx == (first == 0 ? 1 : -1)) {
      selected = -1;
      return;
    }
    if (dy != 0) selected = Math.max(first, Math.min(first + 3, selected + dy));
  }
}
