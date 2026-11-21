package android.widget;

import android.content.Context;

public class OverScroller {
  private int y, target;
  private long start;

  public OverScroller(Context c) {}

  public boolean isFinished() {
    return y == target;
  }

  public void abortAnimation() {
    y = target;
  }

  public int getCurrY() {
    return y;
  }

  public void fling(int x, int y, int vx, int vy, int minX, int maxX, int minY, int maxY) {
    this.y = y;
    target = Math.max(minY, Math.min(maxY, y + vy / 3));
    start = android.os.SystemClock.uptimeMillis();
  }

  public boolean computeScrollOffset() {
    if (isFinished()) return false;
    float t = Math.min(1, (android.os.SystemClock.uptimeMillis() - start) / 350f);
    y += (int) ((target - y) * t);
    if (t >= 1) y = target;
    return true;
  }
}
