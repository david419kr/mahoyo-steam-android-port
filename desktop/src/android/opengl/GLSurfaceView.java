package android.opengl;

import java.util.concurrent.*;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GLSurfaceView {
  public interface Renderer {
    void onSurfaceCreated(GL10 g, EGLConfig c);

    void onSurfaceChanged(GL10 g, int w, int h);

    void onDrawFrame(GL10 g);
  }

  private final ConcurrentLinkedQueue<Runnable> queue = new ConcurrentLinkedQueue<>();
  public volatile boolean dirty = true;

  public void requestRender() {
    dirty = true;
  }

  public void queueEvent(Runnable r) {
    queue.add(r);
  }

  public void drain() {
    Runnable r;
    while ((r = queue.poll()) != null) r.run();
  }
}
