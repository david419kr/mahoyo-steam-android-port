package android.graphics;

public class Rect {
  public int left, top, right, bottom;

  public Rect() {
    this(0, 0, 0, 0);
  }

  public Rect(int l, int t, int r, int b) {
    left = l;
    top = t;
    right = r;
    bottom = b;
  }

  public int width() {
    return right - left;
  }

  public int height() {
    return bottom - top;
  }
}
