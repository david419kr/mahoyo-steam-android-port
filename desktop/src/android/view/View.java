package android.view;

import android.content.Context;
import android.graphics.Canvas;

public class View {
  public static final int IMPORTANT_FOR_ACCESSIBILITY_NO = 2;
  private final Context context;

  public View(Context c) {
    context = c;
  }

  public Context getContext() {
    return context;
  }

  public android.content.res.Resources getResources() {
    return context.getResources();
  }

  public void setImportantForAccessibility(int mode) {}

  public void invalidate() {}

  public void postInvalidateOnAnimation() {}

  public void postInvalidateDelayed(long delay) {}

  public boolean postDelayed(Runnable r, long ms) {
    return true;
  }

  public boolean removeCallbacks(Runnable r) {
    return true;
  }

  protected void onDetachedFromWindow() {}

  protected void onDraw(Canvas c) {}

  public boolean onTouchEvent(MotionEvent e) {
    return false;
  }

  public boolean performClick() {
    return true;
  }

  public void computeScroll() {}

  public void draw(Canvas c) {
    onDraw(c);
  }
}
