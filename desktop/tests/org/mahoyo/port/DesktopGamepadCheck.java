package org.mahoyo.port;

import java.awt.*;
import java.util.*;
import java.util.List;
import javax.swing.*;
import org.lwjgl.glfw.GLFW;

public final class DesktopGamepadCheck {
  private static int button(int value) {
    return 1 << value;
  }

  public static void main(String[] args) throws Exception {
    List<String> events = new ArrayList<>();
    Object game = new Object(), dialog = new Object();
    try (DesktopGamepad pad =
        new DesktopGamepad(a -> events.add(a.name()), v -> events.add("skip:" + v))) {
      long now = 0;
      pad.update(0, 0, true, game, 0, true, now);
      int[] buttons = {
        GLFW.GLFW_GAMEPAD_BUTTON_A,
        GLFW.GLFW_GAMEPAD_BUTTON_B,
        GLFW.GLFW_GAMEPAD_BUTTON_Y,
        GLFW.GLFW_GAMEPAD_BUTTON_X,
        GLFW.GLFW_GAMEPAD_BUTTON_DPAD_UP,
        GLFW.GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
        GLFW.GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
        GLFW.GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
        GLFW.GLFW_GAMEPAD_BUTTON_BACK,
        GLFW.GLFW_GAMEPAD_BUTTON_START,
        GLFW.GLFW_GAMEPAD_BUTTON_LEFT_BUMPER
      };
      for (int b : buttons) {
        pad.update(0, button(b), true, game, 0, true, ++now);
        pad.update(0, button(b), true, game, 0, true, ++now);
        pad.update(0, 0, true, game, 0, true, ++now);
      }
      if (!events.equals(
          List.of(
              "ACCEPT",
              "CANCEL",
              "MENU",
              "HISTORY",
              "UP",
              "DOWN",
              "LEFT",
              "RIGHT",
              "TOGGLE_TEXT",
              "TOGGLE_FULLSCREEN",
              "TOGGLE_DISPLAY"))) throw new AssertionError(events);
      events.clear();
      int r1 = button(GLFW.GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER);
      pad.update(0, r1, true, game, 0, true, ++now);
      pad.update(0, r1, false, game, 0, true, ++now);
      pad.update(0, r1, true, game, 0, true, ++now);
      pad.update(0, r1, true, game, 0, true, ++now);
      if (!events.equals(List.of("skip:true", "skip:false")))
        throw new AssertionError("Re-enabling blocked input restarted held skip: " + events);
      pad.update(0, 0, true, game, 0, true, ++now);
      pad.update(0, r1, true, game, 0, true, ++now);
      pad.update(-1, 0, true, game, 0, true, ++now);
      if (!events.equals(List.of("skip:true", "skip:false", "skip:true", "skip:false")))
        throw new AssertionError("Disconnect left skip held: " + events);
      events.clear();
      int accept = button(GLFW.GLFW_GAMEPAD_BUTTON_A);
      pad.update(0, accept, true, dialog, DesktopGamepad.DIRECTIONS, false, ++now);
      pad.update(0, accept, true, dialog, DesktopGamepad.DIRECTIONS, false, ++now);
      if (!events.isEmpty())
        throw new AssertionError("Held button activated a newly opened dialog");
      pad.update(0, 0, true, dialog, DesktopGamepad.DIRECTIONS, false, ++now);
      pad.update(0, accept, true, dialog, DesktopGamepad.DIRECTIONS, false, ++now);
      if (!events.equals(List.of("ACCEPT"))) throw new AssertionError(events);
      events.clear();
      pad.update(0, 0, true, dialog, DesktopGamepad.DIRECTIONS, false, 1000);
      int down = button(GLFW.GLFW_GAMEPAD_BUTTON_DPAD_DOWN);
      pad.update(0, down, true, dialog, DesktopGamepad.DIRECTIONS, false, 2000);
      pad.update(0, down, true, dialog, DesktopGamepad.DIRECTIONS, false, 200_000_000);
      pad.update(0, down, true, dialog, DesktopGamepad.DIRECTIONS, false, 400_000_000);
      pad.update(0, down, true, dialog, DesktopGamepad.DIRECTIONS, false, 450_000_000);
      pad.update(0, down, true, dialog, DesktopGamepad.DIRECTIONS, false, 510_000_000);
      if (!events.equals(List.of("DOWN", "DOWN", "DOWN")))
        throw new AssertionError("Menu direction repeat: " + events);
    }
    SwingUtilities.invokeAndWait(
        () -> {
          JPanel root = new JPanel(null);
          JButton a = new JButton("a"), b = new JButton("b"), c = new JButton("c");
          a.setBounds(0, 0, 80, 30);
          b.setBounds(100, 0, 80, 30);
          c.setBounds(0, 60, 80, 30);
          root.add(a);
          root.add(b);
          root.add(c);
          if (DesktopDialogInput.next(root, a, 1, 0) != b
              || DesktopDialogInput.next(root, b, 0, 1) != c
              || DesktopDialogInput.next(root, c, 0, -1) != a)
            throw new AssertionError("Dialog focus direction does not match geometry");
          b.setEnabled(false);
          if (DesktopDialogInput.next(root, a, 1, 0) != null)
            throw new AssertionError("Disabled control can be selected");
          JPanel tabsRoot = new JPanel(null);
          tabsRoot.setSize(320, 300);
          JTabbedPane tabs = new JTabbedPane();
          tabs.setBounds(0, 0, 320, 280);
          JPanel page = new JPanel(null);
          JButton first = new JButton("first"), last = new JButton("last");
          first.setBounds(10, 20, 100, 30);
          last.setBounds(10, 180, 100, 30);
          page.add(first);
          page.add(last);
          tabs.addTab("Display", page);
          tabsRoot.add(tabs);
          tabs.doLayout();
          if (DesktopDialogInput.next(tabsRoot, tabs, 0, 1) != first)
            throw new AssertionError("Tab header skips the first settings row");
        });
    System.out.println(
        "Gamepad face/D-pad mapping, button edges, held skip, focus, hotplug, modal suppression,"
            + " direction repeat and dialog focus passed");
  }
}
