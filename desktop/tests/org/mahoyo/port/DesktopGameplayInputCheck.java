package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;
import org.json.*;

/** Uses an isolated native runtime; does not show a window or print story text. */
public final class DesktopGameplayInputCheck {
  public static void main(String[] args) throws Exception {
    File engine = new File(args[0]).getAbsoluteFile();
    Context.home = engine.getParentFile();
    Context.assets = new File("build/windows/assets");
    if (!Context.home.getName().startsWith("gamepad-input-check-"))
      throw new IllegalArgumentException("Use only the isolated controller fixture");
    Context context = new Context();
    GameResources resources = new GameResources(context, engine);
    long handle =
        NativeBridge.createConfigured(
            engine.getPath(),
            resources.steam.korean() ? "korean" : "vanilla",
            resources.language());
    try {
      JSONObject state = call(handle, "action", "start");
      for (int i = 0; i < 200 && !state.optBoolean("canSave"); i++) {
        if (state.optString("waiting").equals("transitionReady")) {
          String ack = state.optString("alphaRulePrepareAck", "");
            if (ack.isEmpty()) ack = state.getJSONObject("transition").optString("id");
          state = call(handle, "transitionReady", ack);
        }
        state = call(handle, "tick", Integer.toString(i * 50));
      }
      if (!state.optBoolean("canSave"))
        throw new AssertionError("Fixture did not reach the first reader input");
      DesktopWindow window = new DesktopWindow(context, resources, handle);
      set(window, "scene", new Scene(state.toString()));
      set(window, "controls", new DesktopControls(resources, (type, payload) -> {}));
      set(window, "fullscreen", true);
      set(window, "windowFocused", true);
      set(window, "menu", true);
      List<Boolean> cursor = new ArrayList<>();
      set(window, "mouseCursor", new DesktopCursor(System.nanoTime(), cursor::add));
      Field focusField = DesktopWindow.class.getDeclaredField("menuFocus");
      focusField.setAccessible(true);
      DesktopMenuFocus focus = (DesktopMenuFocus) focusField.get(window);
      focus.move(-1, 0);
      Method action =
          DesktopWindow.class.getDeclaredMethod("padAction", DesktopGamepad.Action.class);
      action.setAccessible(true);
      long version = call(handle, "tick", "10001").getLong("version");
      action.invoke(window, DesktopGamepad.Action.CANCEL);
      long advanced = new JSONObject(NativeBridge.snapshot(handle)).getLong("version");
      if (advanced <= version || !getBoolean(window, "menu") || focus.selected() != 0)
        throw new AssertionError(
            "Game B must advance without closing or activating the selected rail");
      if (!cursor.equals(List.of(false)))
        throw new AssertionError("Game B did not hide fullscreen pointer");
      set(window, "dialogOpen", true);
      version = advanced;
      action.invoke(window, DesktopGamepad.Action.CANCEL);
      if (new JSONObject(NativeBridge.snapshot(handle)).getLong("version") != version)
        throw new AssertionError("Popup B advanced the game");
      set(window, "dialogOpen", false);
      boolean old = new DisplayPrefs(context).dynamic();
      action.invoke(window, DesktopGamepad.Action.TOGGLE_DISPLAY);
      if (new DisplayPrefs(new Context()).dynamic() == old)
        throw new AssertionError("L1 did not persist its display toggle");
      action.invoke(window, DesktopGamepad.Action.TOGGLE_DISPLAY);
      set(window, "menu", false);
      version = new JSONObject(NativeBridge.snapshot(handle)).getLong("version");
      action.invoke(window, DesktopGamepad.Action.CANCEL);
      if (new JSONObject(NativeBridge.snapshot(handle)).getLong("version") <= version)
        throw new AssertionError("Game B with hidden rails did not advance");
      System.out.println(
          "B advances with/without selected rails, keeps rail focus, hides cursor, does not advance"
              + " popups; L1 display preference persists");
    } finally {
      NativeBridge.destroy(handle);
    }
  }

  private static JSONObject call(long handle, String type, String payload) {
    JSONObject state = new JSONObject(NativeBridge.command(handle, type, payload));
    if (!state.optString("error").isEmpty()) throw new AssertionError(state.optString("error"));
    return state;
  }

  private static void set(Object target, String name, Object value) throws Exception {
    Field field = target.getClass().getDeclaredField(name);
    field.setAccessible(true);
    field.set(target, value);
  }

  private static boolean getBoolean(Object target, String name) throws Exception {
    Field field = target.getClass().getDeclaredField(name);
    field.setAccessible(true);
    return field.getBoolean(target);
  }
}
