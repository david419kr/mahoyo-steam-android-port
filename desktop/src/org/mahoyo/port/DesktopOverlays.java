package org.mahoyo.port;

import static org.lwjgl.system.jawt.JAWTFunctions.*;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import org.lwjgl.glfw.*;
import org.lwjgl.system.jawt.*;
import org.lwjgl.system.windows.*;

/** Native ownership keeps Swing dialogs above the GLFW fullscreen window without global topmost. */
final class DesktopOverlays implements AutoCloseable {
  private static DesktopOverlays current;
  private final long game;
  private final JAWT awt = JAWT.calloc().version(JAWT_VERSION_1_4);
  private final AWTEventListener listener =
      event -> {
        if (event instanceof WindowEvent e
            && e.getID() == WindowEvent.WINDOW_OPENED
            && e.getWindow() instanceof Dialog dialog) prepare(dialog, true);
      };

  DesktopOverlays(long window) {
    game = GLFWNativeWin32.glfwGetWin32Window(window);
    if (!JAWT_GetAWT(awt))
      throw new IllegalStateException("Cannot connect Windows overlay ownership");
    current = this;
    Toolkit.getDefaultToolkit().addAWTEventListener(listener, AWTEvent.WINDOW_EVENT_MASK);
  }

  static void show(JDialog dialog) {
    if (current == null) throw new IllegalStateException("The game window is not ready");
    current.prepare(dialog, false);
    dialog.setVisible(true);
  }

  void prepare(Dialog dialog, boolean focus) {
    if (!SwingUtilities.isEventDispatchThread())
      throw new IllegalStateException("Dialog ownership must run on the UI thread");
    if (!dialog.isDisplayable()) dialog.addNotify();
    long target = handle(dialog),
        owner =
            dialog.getOwner() instanceof Dialog parent && parent.isDisplayable()
                ? handle(parent)
                : game;
    User32.SetWindowLongPtr(null, target, User32.GWL_HWNDPARENT, owner);
    if (User32.GetWindowLongPtr(null, target, User32.GWL_HWNDPARENT) != owner)
      throw new IllegalStateException("Cannot attach the overlay to its game window");
    try (var stack = org.lwjgl.system.MemoryStack.stackPush()) {
      RECT parent = RECT.malloc(stack), child = RECT.malloc(stack);
      if (!User32.GetWindowRect(null, owner, parent) || !User32.GetWindowRect(null, target, child))
        throw new IllegalStateException("Cannot position the game overlay");
      int x = parent.left() + (parent.right() - parent.left() - (child.right() - child.left())) / 2,
          y = parent.top() + (parent.bottom() - parent.top() - (child.bottom() - child.top())) / 2;
      User32.SetWindowPos(
          null,
          target,
          User32.HWND_TOP,
          x,
          y,
          0,
          0,
          User32.SWP_NOSIZE | User32.SWP_NOACTIVATE | User32.SWP_NOOWNERZORDER);
    }
    if (focus) {
      dialog.toFront();
      dialog.requestFocus();
      SwingUtilities.invokeLater(() -> DesktopDialogFocus.restore(dialog));
    }
  }

  long handle(Window window) {
    JAWTDrawingSurface surface = JAWT_GetDrawingSurface(window, awt.GetDrawingSurface());
    if (surface == null) throw new IllegalStateException("Cannot read the overlay window handle");
    try {
      if ((JAWT_DrawingSurface_Lock(surface, surface.Lock()) & JAWT_LOCK_ERROR) != 0)
        throw new IllegalStateException("Cannot lock the overlay window");
      try {
        JAWTDrawingSurfaceInfo info =
            JAWT_DrawingSurface_GetDrawingSurfaceInfo(surface, surface.GetDrawingSurfaceInfo());
        if (info == null) throw new IllegalStateException("Overlay surface is unavailable");
        try {
          return JAWTWin32DrawingSurfaceInfo.create(info.platformInfo()).hwnd();
        } finally {
          JAWT_DrawingSurface_FreeDrawingSurfaceInfo(info, surface.FreeDrawingSurfaceInfo());
        }
      } finally {
        JAWT_DrawingSurface_Unlock(surface, surface.Unlock());
      }
    } finally {
      JAWT_FreeDrawingSurface(surface, awt.FreeDrawingSurface());
    }
  }

  void raise() {
    SwingUtilities.invokeLater(
        () -> {
          Window active = DesktopDialogInput.active();
          if (active instanceof Dialog dialog) {
            prepare(dialog, true);
            return;
          }
          for (Window window : Window.getWindows())
            if (window instanceof Dialog dialog && dialog.isShowing()) {
              prepare(dialog, true);
              break;
            }
        });
  }

  @Override
  public void close() {
    Toolkit.getDefaultToolkit().removeAWTEventListener(listener);
    if (current == this) current = null;
    awt.free();
  }
}
