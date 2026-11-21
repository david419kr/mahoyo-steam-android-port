package org.mahoyo.port;

import java.awt.*;
import java.awt.image.BufferedImage;
import java.io.File;
import java.nio.file.*;
import javax.swing.*;

/** A long authored backlog with native focus deliberately pointing at its oldest row. */
public final class DesktopDialogStateCheck {
  public static void main(String[] args) throws Exception {
    SwingUtilities.invokeAndWait(
        () -> {
          JDialog dialog = new JDialog((Frame) null, "authored-history-focus");
          dialog.setSize(400, 240);
          JPanel rows = new JPanel(null);
          rows.setPreferredSize(new Dimension(340, 2400));
          JButton[] buttons = new JButton[30];
          for (int i = 0; i < buttons.length; i++) {
            buttons[i] = new JButton("History " + i);
            buttons[i].setBounds(220, i * 80 + 10, 90, 40);
            rows.add(buttons[i]);
          }
          JScrollPane scroll = new JScrollPane(rows);
          dialog.add(scroll);
          dialog.addNotify();
          dialog.validate();
          try {
            DesktopDialogFocus.install(dialog, buttons[29], true);
            scroll.getVerticalScrollBar().setValue(scroll.getVerticalScrollBar().getMaximum());
            int bottom = scroll.getVerticalScrollBar().getValue();
            check(
                DesktopDialogFocus.current(dialog, buttons[0]) == buttons[29],
                "native oldest-row focus overrode the explicit latest row");
            check(
                dialog.getFocusTraversalPolicy().getDefaultComponent(dialog) == buttons[29],
                "wrong dialog default focus");
            Component previous = DesktopDialogInput.next(dialog, buttons[0], 0, -1);
            check(previous == buttons[28], "first controller up did not start from the latest row");
            DesktopDialogFocus.select(dialog, previous, true);
            check(
                scroll.getVerticalScrollBar().getValue() > bottom / 2,
                "first controller up jumped to the top");
            BufferedImage selected = paint(buttons[28]), old = paint(buttons[29]);
            check(
                (selected.getRGB(2, 20) & 0xffffff) == 0x287ddb,
                "selected card has no visible blue ring");
            check((old.getRGB(2, 20) & 0xffffff) != 0x287ddb, "old card kept its selection ring");
            DesktopDialogFocus.select(dialog, buttons[29], false);
            check(
                DesktopDialogInput.next(dialog, buttons[0], 0, 1) == null,
                "down at the latest row restarted at the oldest row");
            buttons[29].setEnabled(false);
            buttons[28].setEnabled(false);
            DesktopDialogFocus.browseDisabled(buttons[29]);
            DesktopDialogFocus.browseDisabled(buttons[28]);
            DesktopDialogFocus.select(dialog, buttons[29], false);
            check(
                DesktopDialogFocus.current(dialog, buttons[0]) == buttons[29],
                "unavailable latest actions reset history navigation to older rows");
            check(
                DesktopDialogInput.next(dialog, buttons[0], 0, -1) == buttons[28],
                "unavailable history rows cannot be browsed");
          } finally {
            dialog.dispose();
          }
        });
    File profile =
        Files.createTempDirectory(Path.of(".local/windows"), "window-state-check-").toFile();
    android.content.Context.home = profile;
    DesktopWindowState initial = new DesktopWindowState(new android.content.Context());
    check(
        initial.size().equals(new DesktopWindowState.Size(1920, 1080)) && !initial.fullscreen(),
        "clean start is not native1080p");
    initial.save(1111, 777, true);
    DesktopWindowState restored = new DesktopWindowState(new android.content.Context());
    check(
        restored.size().equals(new DesktopWindowState.Size(1111, 777)) && restored.fullscreen(),
        "windowed size/fullscreen were not persisted");
    for (int percent : new int[] {50, 75, 100, 150, 200}) {
      var size = DesktopWindowState.preset(percent);
      check(
          size.width() == 1920 * percent / 100 && size.height() == 1080 * percent / 100,
          "wrong viewport preset");
      check(
          DesktopWindowState.percent(size.width(), size.height()) == percent,
          "preset lookup failed");
    }
    System.out.println(
        "Long-history first-direction navigation stays at latest rows; slot focus rings,"
            + " clean1080p, persisted size/fullscreen and all five presets passed");
  }

  private static BufferedImage paint(JButton button) {
    BufferedImage image =
        new BufferedImage(button.getWidth(), button.getHeight(), BufferedImage.TYPE_INT_ARGB);
    Graphics2D g = image.createGraphics();
    button.paint(g);
    g.dispose();
    return image;
  }

  private static void check(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }
}
