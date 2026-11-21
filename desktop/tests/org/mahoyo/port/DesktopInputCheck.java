package org.mahoyo.port;

import java.util.*;
import org.lwjgl.glfw.GLFW;

public final class DesktopInputCheck {
  public static void main(String[] args) {
    List<String> events = new ArrayList<>();
    boolean[] blocked = {false};
    DesktopInput input =
        new DesktopInput(
            () -> blocked[0],
            () -> events.add("advance"),
            () -> events.add("text"),
            () -> events.add("history"),
            () -> events.add("fullscreen"),
            () -> events.add("menu"),
            v -> events.add("skip:" + v));
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_ALT);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_SPACE, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_UP, GLFW.GLFW_PRESS, 0);
    input.wheel(1);
    if (!events.equals(Arrays.asList("fullscreen", "advance", "text", "history", "history")))
      throw new AssertionError(events);
    events.clear();
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_REPEAT, 0);
    input.key(GLFW.GLFW_KEY_RIGHT_CONTROL, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_RELEASE, 0);
    if (!events.equals(Arrays.asList("skip:true")))
      throw new AssertionError("second Control still held: " + events);
    input.key(GLFW.GLFW_KEY_RIGHT_CONTROL, GLFW.GLFW_RELEASE, 0);
    if (!events.equals(Arrays.asList("skip:true", "skip:false"))) throw new AssertionError(events);
    events.clear();
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_PRESS, 0);
    input.focus(false);
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_RELEASE, 0);
    if (!events.equals(Arrays.asList("skip:true", "skip:false")))
      throw new AssertionError("focus release: " + events);
    events.clear();
    blocked[0] = true;
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, 0);
    input.wheel(1);
    if (!events.isEmpty()) throw new AssertionError("modal blocks game input: " + events);
    System.out.println("Windows input bindings, dual Control and focus-loss release passed");
  }
}
