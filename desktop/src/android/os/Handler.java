package android.os;

public class Handler {
  public Handler() {}

  public Handler(Looper l) {}

  public boolean post(Runnable r) {
    javax.swing.SwingUtilities.invokeLater(r);
    return true;
  }

  public boolean postDelayed(Runnable r, long ms) {
    javax.swing.Timer t = new javax.swing.Timer((int) ms, e -> r.run());
    t.setRepeats(false);
    t.start();
    return true;
  }

  public void removeCallbacks(Runnable r) {}
}
