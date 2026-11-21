package android.graphics;

public class LinearGradient extends Shader {
  public LinearGradient(float x1, float y1, float x2, float y2, int c1, int c2, TileMode mode) {
    this(x1, y1, x2, y2, new int[] {c1, c2}, null, mode);
  }

  public LinearGradient(
      float x1, float y1, float x2, float y2, int[] colors, float[] positions, TileMode mode) {
    java.awt.Color[] c = new java.awt.Color[colors.length];
    float[] p = positions == null ? new float[c.length] : positions;
    for (int i = 0; i < c.length; i++) {
      c[i] = new java.awt.Color(colors[i], true);
      if (positions == null) p[i] = i / (float) (c.length - 1);
    }
    if (x1 == x2 && y1 == y2) y2 += .001f;
    paint = new java.awt.LinearGradientPaint(x1, y1, x2, y2, p, c);
  }
}
