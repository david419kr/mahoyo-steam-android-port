package org.mahoyo.port;

import java.util.function.Consumer;
import org.lwjgl.glfw.*;

/**
 * Standard GLFW mappings normalize Sony and XInput layouts; unmapped duplicate HID interfaces are
 * ignored.
 */
final class DesktopGamepad implements AutoCloseable {
  enum Action {
    ACCEPT,
    CANCEL,
    MENU,
    HISTORY,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    TOGGLE_TEXT
  }

  private final Consumer<Action> action;
  private final Consumer<Boolean> skip;
  private final GLFWGamepadState state = GLFWGamepadState.calloc();
  private int device = -1, source = -1, previous, suppressed, repeat;
  private Object context;
  private boolean enabled, skipping;
  private long nextPoll, nextScan, nextRepeat;

  DesktopGamepad(Consumer<Action> action, Consumer<Boolean> skip) {
    this.action = action;
    this.skip = skip;
  }

  void poll(
      long now, boolean enabled, Object context, boolean repeatDirections, boolean skipAllowed) {
    if (now < nextPoll) return;
    nextPoll = now + 8_000_000L;
    if (device < 0 && now >= nextScan) {
      nextScan = now + 1_000_000_000L;
      for (int id = 0; id <= GLFW.GLFW_JOYSTICK_LAST; id++)
        if (GLFW.glfwJoystickPresent(id) && GLFW.glfwJoystickIsGamepad(id)) {
          device = id;
          break;
        }
    }
    int buttons = 0;
    if (device >= 0) {
      if (GLFW.glfwGetGamepadState(device, state)) {
        for (int button = 0; button <= GLFW.GLFW_GAMEPAD_BUTTON_LAST; button++)
          if (state.buttons(button) == GLFW.GLFW_PRESS) buttons |= 1 << button;
      } else {
        device = -1;
        nextScan = 0;
      }
    }
    update(device, buttons, enabled, context, repeatDirections, skipAllowed, now);
  }

  void update(
      int source,
      int buttons,
      boolean enabled,
      Object context,
      boolean repeatDirections,
      boolean skipAllowed,
      long now) {
    if (source != this.source || context != this.context || !enabled || !this.enabled) {
      this.source = source;
      this.context = context;
      this.enabled = enabled;
      suppressed = buttons;
      previous = 0;
      repeat = 0;
      setSkip(false);
      return;
    }
    suppressed &= buttons;
    int current = buttons & ~suppressed, pressed = current & ~previous;
    previous = current;
    setSkip(skipAllowed && (current & (1 << GLFW.GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER)) != 0);
    int[] indices = {
      GLFW.GLFW_GAMEPAD_BUTTON_A,
      GLFW.GLFW_GAMEPAD_BUTTON_B,
      GLFW.GLFW_GAMEPAD_BUTTON_Y,
      GLFW.GLFW_GAMEPAD_BUTTON_X,
      GLFW.GLFW_GAMEPAD_BUTTON_DPAD_UP,
      GLFW.GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
      GLFW.GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
      GLFW.GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
      GLFW.GLFW_GAMEPAD_BUTTON_BACK
    };
    Action[] actions = Action.values();
    for (int i = 0; i < indices.length; i++)
      if ((pressed & (1 << indices[i])) != 0) action.accept(actions[i]);
    int direction =
        current
            & ((1 << GLFW.GLFW_GAMEPAD_BUTTON_DPAD_UP)
                | (1 << GLFW.GLFW_GAMEPAD_BUTTON_DPAD_DOWN)
                | (1 << GLFW.GLFW_GAMEPAD_BUTTON_DPAD_LEFT)
                | (1 << GLFW.GLFW_GAMEPAD_BUTTON_DPAD_RIGHT));
    if (!repeatDirections || Integer.bitCount(direction) != 1) {
      repeat = 0;
      return;
    }
    if (direction != repeat) {
      repeat = direction;
      nextRepeat = now + 350_000_000L;
    } else if (now >= nextRepeat) {
      nextRepeat = now + 100_000_000L;
      for (int i = 4; i < indices.length; i++)
        if (direction == (1 << indices[i])) action.accept(actions[i]);
    }
  }

  private void setSkip(boolean value) {
    if (skipping != value) {
      skipping = value;
      skip.accept(value);
    }
  }

  @Override
  public void close() {
    setSkip(false);
    state.free();
  }
}
