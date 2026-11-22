package org.mahoyo.port;

import java.awt.*;
import java.util.*;
import javax.swing.*;

/**
 * Authored visibility adapters exercise background routing without opening or focusing OS windows.
 */
public final class DesktopBackgroundDialogCheck {
  private static final class DialogFixture extends JDialog {
    boolean shown = true;

    DialogFixture(Dialog owner) {
      super(owner);
    }

    @Override
    public boolean isShowing() {
      return shown;
    }
  }

  public static void main(String[] args) throws Exception {
    DialogFixture[] dialogs = new DialogFixture[2];
    java.util.List<String> events = new ArrayList<>();
    try {
      SwingUtilities.invokeAndWait(
          () -> {
            dialogs[0] = new DialogFixture(null);
            dialogs[1] = new DialogFixture(dialogs[0]);
            for (int i = 0; i < 2; i++) {
              String name = i == 0 ? "parent" : "confirmation";
              JButton button = new JButton(name);
              button.addActionListener(e -> events.add(name));
              dialogs[i].add(button);
              DesktopDialogFocus.install(dialogs[i], button, true);
            }
            if (DesktopDialogInput.active() != dialogs[1])
              throw new AssertionError("Background routing ignored the nested confirmation");
          });
      DesktopDialogInput.send(dialogs[0], DesktopGamepad.Action.ACCEPT);
      DesktopDialogInput.send(dialogs[1], DesktopGamepad.Action.ACCEPT);
      SwingUtilities.invokeAndWait(
          () -> {
            if (!events.equals(java.util.List.of("confirmation")))
              throw new AssertionError("Background dispatch reached the wrong dialog: " + events);
            dialogs[1].shown = false;
            if (DesktopDialogInput.active() != dialogs[0])
              throw new AssertionError("Parent dialog was not restored");
          });
      DesktopDialogInput.send(dialogs[0], DesktopGamepad.Action.ACCEPT);
      SwingUtilities.invokeAndWait(
          () -> {
            if (!events.equals(java.util.List.of("confirmation", "parent")))
              throw new AssertionError("Background parent dispatch failed: " + events);
            dialogs[0].shown = false;
            if (DesktopDialogInput.active() != null)
              throw new AssertionError("Closed dialog remains a controller target");
          });
      System.out.println(
          "Background dialog/confirmation routing, stale-parent rejection and close handoff"
              + " passed");
    } finally {
      SwingUtilities.invokeAndWait(
          () -> {
            for (DialogFixture dialog : dialogs) if (dialog != null) dialog.dispose();
          });
    }
  }
}
