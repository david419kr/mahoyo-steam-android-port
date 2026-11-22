package org.mahoyo.port;

import java.awt.*;
import java.awt.event.*;
import java.util.*;
import javax.swing.*;
import javax.swing.text.JTextComponent;

/** Delivers controller actions inside this JVM only; never synthesizes global OS input. */
final class DesktopDialogInput {
  static Window active() {
    Window window = KeyboardFocusManager.getCurrentKeyboardFocusManager().getActiveWindow();
    if (window instanceof Dialog && window.isShowing()) return window;
    // Controller input targets this JVM's visible dialog even while another app has OS focus.
    Window[] windows = Window.getWindows();
    for (int i = windows.length - 1; i >= 0; i--)
      if (windows[i] instanceof Dialog && windows[i].isShowing()) return windows[i];
    return null;
  }

  static void send(Window expected, DesktopGamepad.Action action) {
    SwingUtilities.invokeLater(
        () -> {
          if (active() != expected) return;
          Component focused = KeyboardFocusManager.getCurrentKeyboardFocusManager().getFocusOwner();
          if (focused == null || SwingUtilities.getWindowAncestor(focused) != expected)
            focused = expected.getMostRecentFocusOwner();
          focused = DesktopDialogFocus.current(expected, focused);
          if (action == DesktopGamepad.Action.CANCEL || action == DesktopGamepad.Action.MENU) {
            if (focused instanceof JComboBox<?> combo && combo.isPopupVisible()) {
              combo.hidePopup();
              return;
            }
            if (MenuSelectionManager.defaultManager().getSelectedPath().length > 0) {
              MenuSelectionManager.defaultManager().clearSelectedPath();
              return;
            }
            if (!binding(focused, expected, KeyEvent.VK_ESCAPE))
              expected.dispatchEvent(new WindowEvent(expected, WindowEvent.WINDOW_CLOSING));
            return;
          }
          if (action == DesktopGamepad.Action.ACCEPT) {
            if (focused instanceof AbstractButton button) {
              if (button.isEnabled()) button.doClick(0);
              return;
            }
            if (focused instanceof JComboBox<?> combo) {
              combo.setPopupVisible(!combo.isPopupVisible());
              return;
            }
            binding(focused, expected, KeyEvent.VK_ENTER);
            return;
          }
          int
              dx =
                  action == DesktopGamepad.Action.LEFT
                      ? -1
                      : action == DesktopGamepad.Action.RIGHT ? 1 : 0,
              dy =
                  action == DesktopGamepad.Action.UP
                      ? -1
                      : action == DesktopGamepad.Action.DOWN ? 1 : 0;
          if (dx == 0 && dy == 0) return;
          int key =
              dx < 0
                  ? KeyEvent.VK_LEFT
                  : dx > 0 ? KeyEvent.VK_RIGHT : dy < 0 ? KeyEvent.VK_UP : KeyEvent.VK_DOWN;
          if (focused instanceof JComboBox<?> combo && combo.isPopupVisible()
              || focused instanceof JSlider && dx != 0
              || focused instanceof JList<?>
              || focused instanceof JTable
              || focused instanceof JTree) {
            if (binding(focused, expected, key)) return;
          }
          if (focused instanceof JTabbedPane tabs && dx != 0) {
            tabs.setSelectedIndex(Math.floorMod(tabs.getSelectedIndex() + dx, tabs.getTabCount()));
            return;
          }
          Component target = next(expected, focused, dx, dy);
          if (target != null) {
            DesktopDialogFocus.select(expected, target, true);
          }
        });
  }

  static Component next(Container root, Component from, int dx, int dy) {
    from = DesktopDialogFocus.current(root, from);
    java.util.List<Component> candidates = new ArrayList<>();
    collect(root, candidates);
    if (candidates.isEmpty()) return null;
    if (from == null || !candidates.contains(from)) return candidates.getFirst();
    Rectangle current = bounds(from, root);
    double score = Double.POSITIVE_INFINITY;
    Component winner = null;
    for (Component candidate : candidates) {
      if (candidate == from) continue;
      Rectangle rect = bounds(candidate, root);
      double x = rect.getCenterX() - current.getCenterX(),
          y = rect.getCenterY() - current.getCenterY(),
          forward = x * dx + y * dy;
      if (forward <= 1) continue;
      double cost = forward + Math.abs(dx != 0 ? y : x) * 3;
      if (cost < score) {
        score = cost;
        winner = candidate;
      }
    }
    return winner;
  }

  private static Rectangle bounds(Component component, Container root) {
    if (component instanceof JTabbedPane tabs && tabs.getSelectedIndex() >= 0) {
      Rectangle header = tabs.getBoundsAt(tabs.getSelectedIndex());
      if (header != null) return SwingUtilities.convertRectangle(tabs, header, root);
    }
    return SwingUtilities.convertRectangle(component.getParent(), component.getBounds(), root);
  }

  private static void collect(Container parent, java.util.List<Component> result) {
    for (Component c : parent.getComponents()) {
      if (!c.isVisible() || !c.isEnabled() && !DesktopDialogFocus.browsable(c)) continue;
      if (c.isFocusable()
          && (c instanceof AbstractButton
              || c instanceof JComboBox<?>
              || c instanceof JSlider
              || c instanceof JTabbedPane
              || c instanceof JList<?>
              || c instanceof JTable
              || c instanceof JTree
              || c instanceof JTextComponent text && text.isEditable())) result.add(c);
      if (c instanceof Container child) collect(child, result);
    }
  }

  private static boolean binding(Component focused, Window window, int code) {
    KeyStroke stroke = KeyStroke.getKeyStroke(code, 0);
    for (Component c = focused; c != null; c = c.getParent())
      if (c instanceof JComponent component
          && invoke(
              component,
              stroke,
              c == focused
                  ? JComponent.WHEN_FOCUSED
                  : JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT)) return true;
    return window instanceof RootPaneContainer root
        && invoke(root.getRootPane(), stroke, JComponent.WHEN_IN_FOCUSED_WINDOW);
  }

  private static boolean invoke(JComponent component, KeyStroke stroke, int condition) {
    Object name = component.getInputMap(condition).get(stroke);
    javax.swing.Action action = name == null ? null : component.getActionMap().get(name);
    if (action == null || !action.isEnabled()) return false;
    action.actionPerformed(
        new ActionEvent(component, ActionEvent.ACTION_PERFORMED, name.toString()));
    return true;
  }

  private DesktopDialogInput() {}
}
