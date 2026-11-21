package android.view;

public class VelocityTracker {
  private MotionEvent first, last;
  private float speed;

  public static VelocityTracker obtain() {
    return new VelocityTracker();
  }

  public void addMovement(MotionEvent e) {
    if (first == null) first = e;
    last = e;
  }

  public void computeCurrentVelocity(int units, int max) {
    speed =
        Math.max(
            -max,
            Math.min(
                max,
                (last.getY() - first.getY())
                    * units
                    / Math.max(1, last.getEventTime() - first.getEventTime())));
  }

  public float getYVelocity() {
    return speed;
  }

  public void recycle() {}
}
