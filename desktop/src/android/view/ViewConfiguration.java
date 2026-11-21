package android.view;

import android.content.Context;

public class ViewConfiguration {
  public static ViewConfiguration get(Context c) {
    return new ViewConfiguration();
  }

  public int getScaledTouchSlop() {
    return 8;
  }

  public int getScaledMaximumFlingVelocity() {
    return 8000;
  }

  public int getScaledMinimumFlingVelocity() {
    return 50;
  }
}
