package org.mahoyo.port;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.border.*;

/**
 * Controller selection is explicit: native window activation must not reset it to the first item.
 */
final class DesktopDialogFocus {
  private static final Object STATE = new Object(),
      SELECTED = new Object(),
      OBSERVED = new Object(),
      BROWSE_DISABLED = new Object();

  static void browseDisabled(AbstractButton button) {
    button.putClientProperty(BROWSE_DISABLED, true);
  }

  static boolean browsable(Component component) {
    return component instanceof JComponent c
        && Boolean.TRUE.equals(c.getClientProperty(BROWSE_DISABLED));
  }

  private static final class State {
    Component selected;
  }

  private static State state(Container root) {
    return root instanceof RootPaneContainer pane
        ? (State) pane.getRootPane().getClientProperty(STATE)
        : null;
  }

  static void install(JDialog dialog, Component initial, boolean highlight) {
    State state = new State();
    dialog.getRootPane().putClientProperty(STATE, state);
    select(dialog, initial, false);
    dialog.setFocusTraversalPolicy(
        new LayoutFocusTraversalPolicy() {
          @Override
          public Component getDefaultComponent(Container root) {
            Component selected = current(root, null);
            return selected != null ? selected : super.getDefaultComponent(root);
          }
        });
    observe(dialog, dialog, highlight);
  }

  private static void observe(Window dialog, Container root, boolean highlight) {
    for (Component child : root.getComponents()) {
      if (child instanceof AbstractButton button) {
        if (highlight && !(button.getBorder() instanceof FocusBorder)) {
          button.setBorder(new FocusBorder(button.getBorder()));
          button.setFocusPainted(false);
        }
        if (button.getClientProperty(OBSERVED) == null) {
          button.putClientProperty(OBSERVED, true);
          button.addFocusListener(
              new FocusAdapter() {
                @Override
                public void focusGained(FocusEvent event) {
                  switch (event.getCause()) {
                    case MOUSE_EVENT,
                            TRAVERSAL_FORWARD,
                            TRAVERSAL_BACKWARD,
                            TRAVERSAL_UP,
                            TRAVERSAL_DOWN ->
                        select(dialog, button, false);
                    default -> {}
                  }
                }
              });
        }
      }
      if (child instanceof Container container) observe(dialog, container, highlight);
    }
  }

  static Component current(Container root, Component actual) {
    State state = state(root);
    return state != null && valid(root, state.selected) ? state.selected : actual;
  }

  private static boolean valid(Container root, Component component) {
    return component != null
        && component.isVisible()
        && (component.isEnabled() || browsable(component))
        && component.isFocusable()
        && SwingUtilities.isDescendingFrom(component, root);
  }

  static void select(Window dialog, Component component, boolean reveal) {
    State state = state(dialog);
    if (state != null) {
      if (state.selected instanceof JComponent previous) {
        previous.putClientProperty(SELECTED, false);
        previous.repaint();
      }
      state.selected = component;
      if (component instanceof JComponent selected) {
        selected.putClientProperty(SELECTED, true);
        selected.repaint();
      }
    }
    if (component != null && reveal) {
      component.requestFocusInWindow();
      if (component instanceof JComponent target)
        target.scrollRectToVisible(new Rectangle(0, 0, target.getWidth(), target.getHeight()));
    }
  }

  static void restore(Window dialog) {
    Component selected = current(dialog, null);
    if (selected != null) selected.requestFocusInWindow();
  }

  private static final class FocusBorder extends AbstractBorder {
    private final Border original;

    FocusBorder(Border original) {
      this.original = original;
    }

    @Override
    public Insets getBorderInsets(Component c) {
      Insets i = original == null ? new Insets(0, 0, 0, 0) : original.getBorderInsets(c);
      return new Insets(
          Math.max(6, i.top), Math.max(6, i.left), Math.max(6, i.bottom), Math.max(6, i.right));
    }

    @Override
    public void paintBorder(Component c, Graphics graphics, int x, int y, int width, int height) {
      if (original != null) original.paintBorder(c, graphics, x, y, width, height);
      if (c instanceof JComponent component
          && Boolean.TRUE.equals(component.getClientProperty(SELECTED))) {
        Graphics2D g = (Graphics2D) graphics.create();
        try {
          g.setColor(new Color(0x287ddb));
          g.setStroke(new BasicStroke(3));
          g.drawRect(x + 2, y + 2, width - 5, height - 5);
        } finally {
          g.dispose();
        }
      }
    }
  }

  private DesktopDialogFocus() {}
}
