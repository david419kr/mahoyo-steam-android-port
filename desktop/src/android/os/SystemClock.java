package android.os;

public final class SystemClock {
  private static final long START = System.nanoTime();

  public static long uptimeMillis() {
    return (System.nanoTime() - START) / 1000000;
  }

  public static long elapsedRealtime() {
    return uptimeMillis();
  }
}
