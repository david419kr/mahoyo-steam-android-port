package android.graphics;

public class RectF {
  public float left, top, right, bottom;

  public RectF() {
    this(0, 0, 0, 0);
  }

  public RectF(RectF r) {
    this(r.left, r.top, r.right, r.bottom);
  }

  public RectF(float l, float t, float r, float b) {
    set(l, t, r, b);
  }

  public void set(float l, float t, float r, float b) {
    left = l;
    top = t;
    right = r;
    bottom = b;
  }

  public void set(RectF r) {
    set(r.left, r.top, r.right, r.bottom);
  }

  public float width() {
    return right - left;
  }

  public float height() {
    return bottom - top;
  }

  public float centerX() {
    return (left + right) / 2;
  }

  public float centerY() {
    return (top + bottom) / 2;
  }

  public boolean contains(float x, float y) {
    return x >= left && y >= top && x < right && y < bottom;
  }

  public void inset(float x, float y) {
    left += x;
    right -= x;
    top += y;
    bottom -= y;
  }

  public void offset(float x, float y) {
    left += x;
    right += x;
    top += y;
    bottom += y;
  }

  public String toString() {
    return left + "," + top + "," + right + "," + bottom;
  }
}
