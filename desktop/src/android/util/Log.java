package android.util;

public class Log {
  public static int d(String t, String m) {
    System.err.println(t + ": " + m);
    return 0;
  }

  public static int d(String t, String m, Throwable e) {
    d(t, m);
    e.printStackTrace();
    return 0;
  }

  public static int i(String t, String m) {
    return d(t, m);
  }

  public static int w(String t, String m) {
    return d(t, m);
  }

  public static int w(String t, String m, Throwable e) {
    d(t, m);
    e.printStackTrace();
    return 0;
  }

  public static int e(String t, String m) {
    return d(t, m);
  }

  public static String getStackTraceString(Throwable e) {
    return e.toString();
  }
}
