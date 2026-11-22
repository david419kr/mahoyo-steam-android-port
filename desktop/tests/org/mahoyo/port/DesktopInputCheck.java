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
            () -> events.add("display"),
            v -> events.add("skip:" + v));
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_ALT);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_SHIFT);
    input.key(GLFW.GLFW_KEY_KP_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_SHIFT);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_SPACE, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_UP, GLFW.GLFW_PRESS, 0);
    input.wheel(1);
    if (!events.equals(
        Arrays.asList("fullscreen", "display", "display", "advance", "text", "history", "history")))
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
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_SPACE, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_UP, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_ESCAPE, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_ALT);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_SHIFT);
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_PRESS, 0);
    input.wheel(1);
    if (!events.isEmpty()) throw new AssertionError("Unfocused keyboard/wheel input: " + events);
    input.focus(true);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, 0);
    if (!events.equals(List.of("advance")))
      throw new AssertionError("Refocused keyboard input: " + events);
    events.clear();
    blocked[0] = true;
    input.key(GLFW.GLFW_KEY_LEFT_CONTROL, GLFW.GLFW_PRESS, 0);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, 0);
    input.wheel(1);
    input.key(GLFW.GLFW_KEY_ENTER, GLFW.GLFW_PRESS, GLFW.GLFW_MOD_SHIFT);
    if (!events.isEmpty()) throw new AssertionError("modal blocks game input: " + events);
    System.out.println("Windows input bindings, dual Control and focus-loss release passed");
    List<Boolean> pointer = new ArrayList<>();
    DesktopCursor cursor = new DesktopCursor(0, pointer::add);
    cursor.update(false, DesktopCursor.IDLE_NS * 2);
    if (!pointer.isEmpty()) throw new AssertionError("Cursor hidden outside fullscreen play");
    cursor.mouse(DesktopCursor.IDLE_NS * 2);
    cursor.update(true, DesktopCursor.IDLE_NS * 2 + DesktopCursor.IDLE_NS / 2);
    if (!pointer.isEmpty()) throw new AssertionError("Cursor hidden before idle timeout");
    cursor.update(true, DesktopCursor.IDLE_NS * 4);
    cursor.mouse(DesktopCursor.IDLE_NS * 5);
    cursor.advance(true);
    cursor.update(false, DesktopCursor.IDLE_NS * 6);
    cursor.advance(false);
    if (!pointer.equals(Arrays.asList(false, true, false, true)))
      throw new AssertionError("Pointer idle/advance/motion/menu visibility: " + pointer);
    System.out.println("Shift+Enter display routing and fullscreen cursor policy passed");
  }
}
