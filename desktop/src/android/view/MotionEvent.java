package android.view;

public class MotionEvent {
  public static final int ACTION_DOWN = 0,
      ACTION_UP = 1,
      ACTION_MOVE = 2,
      ACTION_CANCEL = 3,
      ACTION_POINTER_DOWN = 5,
      ACTION_POINTER_UP = 6;
  private final int action;
  private final float x, y;
  private final long time;

  public MotionEvent(int a, float x, float y, long t) {
    action = a;
    this.x = x;
    this.y = y;
    time = t;
  }

  public int getActionMasked() {
    return action;
  }

  public int getPointerCount() {
    return 1;
  }

  public float getX() {
    return x;
  }

  public float getY() {
    return y;
  }

  public float getX(int i) {
    return x;
  }

  public float getY(int i) {
    return y;
  }

  public long getEventTime() {
    return time;
  }
}
