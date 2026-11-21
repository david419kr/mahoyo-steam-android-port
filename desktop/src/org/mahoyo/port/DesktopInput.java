package org.mahoyo.port;

import java.util.HashSet;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import org.lwjgl.glfw.GLFW;

/** PC bindings; a held skip key is always released on focus loss. */
final class DesktopInput {
  private final BooleanSupplier blocked;
  private final Runnable advance, toggleText, history, fullscreen, toggleMenu;
  private final Consumer<Boolean> skip;
  private final Set<Integer> controls = new HashSet<>();
  private boolean skipping;

  DesktopInput(
      BooleanSupplier blocked,
      Runnable advance,
      Runnable toggleText,
      Runnable history,
      Runnable fullscreen,
      Runnable toggleMenu,
      Consumer<Boolean> skip) {
    this.blocked = blocked;
    this.advance = advance;
    this.toggleText = toggleText;
    this.history = history;
    this.fullscreen = fullscreen;
    this.toggleMenu = toggleMenu;
    this.skip = skip;
  }

  void key(int key, int action, int modifiers) {
    if (key == GLFW.GLFW_KEY_LEFT_CONTROL || key == GLFW.GLFW_KEY_RIGHT_CONTROL) {
      if (action == GLFW.GLFW_RELEASE) controls.remove(key);
      else if (action == GLFW.GLFW_PRESS && !blocked.getAsBoolean()) controls.add(key);
      boolean next = !controls.isEmpty();
      if (next != skipping) {
        skipping = next;
        skip.accept(next);
      }
      return;
    }
    if (action != GLFW.GLFW_PRESS || blocked.getAsBoolean()) return;
    if ((key == GLFW.GLFW_KEY_ENTER || key == GLFW.GLFW_KEY_KP_ENTER)
        && (modifiers & GLFW.GLFW_MOD_ALT) != 0) fullscreen.run();
    else if (key == GLFW.GLFW_KEY_ENTER || key == GLFW.GLFW_KEY_KP_ENTER) advance.run();
    else if (key == GLFW.GLFW_KEY_SPACE) toggleText.run();
    else if (key == GLFW.GLFW_KEY_UP) history.run();
    else if (key == GLFW.GLFW_KEY_ESCAPE) toggleMenu.run();
  }

  void wheel(double y) {
    if (y > 0 && !blocked.getAsBoolean()) history.run();
  }

  void focus(boolean focused) {
    if (!focused) {
      controls.clear();
      if (skipping) {
        skipping = false;
        skip.accept(false);
      }
    }
  }
}
