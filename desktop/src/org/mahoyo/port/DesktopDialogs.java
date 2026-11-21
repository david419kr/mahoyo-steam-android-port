package org.mahoyo.port;

import android.content.Context;
import android.graphics.Bitmap;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.function.*;
import javax.swing.*;
import javax.swing.border.*;
import org.json.*;

final class DesktopDialogs implements AutoCloseable {
  interface Host {
    JSONObject call(String type, String payload);

    Bitmap capture();

    void language(String language);

    void open(boolean open, boolean settings);

    int[] windowSize();

    void windowSize(int percent);
  }

  private final Host host;
  private final DesktopText translations;
  private final Context context;
  private final DisplayPrefs prefs;
  private final GameResources resources;
  private final File saves, thumbs, workspace, journal;
  private final SlotThumbnailStore store;
  private boolean showing;

  DesktopDialogs(Host host, Context c, DisplayPrefs prefs, GameResources r) {
    this.host = host;
    translations = new DesktopText(r);
    context = c;
    this.prefs = prefs;
    resources = r;
    saves = new File(Context.home, "savedata");
    thumbs = new File(Context.home, "slot-thumbnails");
    workspace = new File(c.getCacheDir(), "save-transfer");
    journal = new File(Context.home, "save-transfer");
    store = new SlotThumbnailStore(saves, thumbs);
  }

  void open(String action) {
    open(action, () -> {});
  }

  void open(String action, Runnable after) {
    if (showing) return;
    showing = true;
    host.open(true, action.equals("settings"));
    SwingUtilities.invokeLater(
        () -> {
          try {
            switch (action) {
              case "save":
                slots(true);
                break;
              case "load":
                slots(false);
                break;
              case "settings":
                settings();
                break;
              case "extra":
                new DesktopExtraUi(host, resources).show();
                break;
              case "history":
                history();
                break;
              default:
                throw new IllegalArgumentException(action);
            }
          } catch (Exception e) {
            error(e);
          } finally {
            showing = false;
            host.open(false, action.equals("settings"));
            after.run();
          }
        });
  }

  private JDialog dialog(String title) {
    JDialog d = new JDialog((Frame) null, translations.text(title), true);
    d.setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);
    d.setSize(960, 640);
    d.setLocationRelativeTo(null);
    d.getRootPane()
        .registerKeyboardAction(
            e -> d.dispose(),
            KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0),
            JComponent.WHEN_IN_FOCUSED_WINDOW);
    return d;
  }

  private JButton button(String text, Runnable action) {
    JButton b = new JButton(translations.text(text));
    b.addActionListener(
        e -> {
          try {
            action.run();
          } catch (Exception x) {
            error(x);
          }
        });
    return b;
  }

  private void error(Throwable e) {
    JOptionPane.showMessageDialog(
        null, e.getMessage(), "WITCH ON THE HOLY NIGHT", JOptionPane.ERROR_MESSAGE);
  }

  private void slots(boolean saving) {
    Bitmap snapshot = saving ? host.capture() : null;
    JDialog d = dialog(saving ? "Save" : "Load");
    JPanel root = new JPanel(new BorderLayout(10, 10));
    root.setBorder(new EmptyBorder(12, 12, 12, 12));
    d.setContentPane(root);
    JPanel toolbar = new JPanel(new FlowLayout(FlowLayout.RIGHT));
    toolbar.add(button("Export ZIP", () -> transfer(false, d)));
    toolbar.add(
        button(
            "Import ZIP",
            () -> {
              transfer(true, d);
              d.dispose();
            }));
    toolbar.add(button("Close", d::dispose));
    root.add(toolbar, BorderLayout.NORTH);
    JPanel grid = new JPanel(new GridLayout(0, 4, 8, 8));
    root.add(new JScrollPane(grid), BorderLayout.CENTER);
    store.list(
        entries -> {
          if (!d.isDisplayable()) return;
          JButton initial = null;
          for (SlotThumbnailStore.Entry entry : entries) {
            JPanel card = new JPanel(new BorderLayout());
            JButton select = new JButton();
            select.setLayout(new BorderLayout());
            JLabel number = new JLabel(String.format("%02d", entry.slot));
            number.setBorder(new EmptyBorder(4, 6, 4, 6));
            select.add(number, BorderLayout.NORTH);
            if (entry.thumbnail != null)
              select.add(
                  new JLabel(
                      new ImageIcon(
                          entry.thumbnail.image.getScaledInstance(176, 99, Image.SCALE_SMOOTH))),
                  BorderLayout.CENTER);
            JTextArea label = new JTextArea(entry.exists ? entry.text : "Empty");
            label.setEditable(false);
            label.setFocusable(false);
            label.setOpaque(false);
            label.setLineWrap(true);
            label.setWrapStyleWord(true);
            label.setRows(2);
            select.add(label, BorderLayout.SOUTH);
            select.setEnabled(saving || entry.exists);
            if (initial == null && select.isEnabled()) initial = select;
            select.addActionListener(
                e -> {
                  if (saving
                      && entry.exists
                      && JOptionPane.showConfirmDialog(
                              d, "Overwrite this save?", "Save", JOptionPane.YES_NO_OPTION)
                          != JOptionPane.YES_OPTION) return;
                  JSONObject result = host.call(saving ? "save" : "load", "" + entry.slot);
                  String notice = result.optString("notice");
                  if (saving && notice.equals("Saved")) store.saved(entry.slot, snapshot);
                  if (!result.optString("error").isEmpty()) {
                    error(new IOException(result.optString("error")));
                    return;
                  }
                  d.dispose();
                });
            if (entry.exists) {
              JPopupMenu menu = new JPopupMenu();
              JMenuItem delete = new JMenuItem("Delete this save…");
              menu.add(delete);
              delete.addActionListener(
                  e -> {
                    if (JOptionPane.showConfirmDialog(
                            d,
                            "Delete this save? This cannot be undone.",
                            "Delete save",
                            JOptionPane.YES_NO_OPTION,
                            JOptionPane.WARNING_MESSAGE)
                        != JOptionPane.YES_OPTION) return;
                    if (JOptionPane.showConfirmDialog(
                            d,
                            "Permanently delete slot " + entry.slot + "? There is no undo.",
                            "Final confirmation",
                            JOptionPane.YES_NO_OPTION,
                            JOptionPane.WARNING_MESSAGE)
                        != JOptionPane.YES_OPTION) return;
                    try {
                      SaveArchive.Plan plan =
                          SaveArchive.deletion(
                              saves, thumbs, entry.slot, entry.modified, entry.size, entry.sha256);
                      SaveArchive.apply(plan, journal, false, () -> host.call("slotDeleted", ""));
                      d.dispose();
                    } catch (Exception x) {
                      error(x);
                    }
                  });
              select.setComponentPopupMenu(menu);
            }
            card.add(select);
            grid.add(card);
          }
          grid.revalidate();
          grid.repaint();
          DesktopDialogFocus.install(d, initial, true);
          DesktopDialogFocus.restore(d);
        });
    DesktopOverlays.show(d);
  }

  private void transfer(boolean importing, JDialog owner) {
    JFileChooser chooser = new JFileChooser();
    chooser.setSelectedFile(new File("mahoyo-saves.zip"));
    int answer = importing ? chooser.showOpenDialog(owner) : chooser.showSaveDialog(owner);
    if (answer != JFileChooser.APPROVE_OPTION) return;
    File file = chooser.getSelectedFile();
    try {
      if (importing) {
        try (SaveArchive.Archive archive = SaveArchive.read(new FileInputStream(file), workspace)) {
          SaveArchive.Plan plan = SaveArchive.plan(archive, saves, thumbs);
          boolean overwrite =
              plan.overwrites.isEmpty()
                  || JOptionPane.showConfirmDialog(
                          owner,
                          "All save slots are occupied. Overwrite slots " + plan.overwrites + "?",
                          "Import saves",
                          JOptionPane.YES_NO_OPTION,
                          JOptionPane.WARNING_MESSAGE)
                      == JOptionPane.YES_OPTION;
          if (!overwrite) return;
          SaveArchive.apply(
              plan,
              journal,
              !plan.overwrites.isEmpty(),
              () -> {
                JSONObject state = host.call("progressImport", "");
                if (!state.optString("error").isEmpty())
                  throw new IOException(state.optString("error"));
              });
        }
      } else {
        if (file.exists()
            && JOptionPane.showConfirmDialog(
                    owner, "Replace this ZIP file?", "Export", JOptionPane.YES_NO_OPTION)
                != JOptionPane.YES_OPTION) return;
        host.call("backupPrepare", "");
        try (SaveArchive.Archive archive = SaveArchive.snapshot(saves, thumbs, workspace)) {
          SaveArchive.write(archive, new FileOutputStream(file), "0.6.13-windows");
        }
      }
      JOptionPane.showMessageDialog(
          owner, importing ? "Save data imported." : "Save data exported.");
    } catch (Exception e) {
      error(e);
    }
  }

  private void settings() {
    JDialog d = dialog("Settings");
    JTabbedPane tabs = new JTabbedPane();
    d.add(tabs);
    JPanel display = column(), game = column();
    tabs.addTab(translations.text("화면"), new JScrollPane(display));
    tabs.addTab(translations.text("게임"), new JScrollPane(game));
    JPanel sizing = new JPanel(new BorderLayout(12, 4));
    sizing.setMaximumSize(new Dimension(Integer.MAX_VALUE, 48));
    String sizeLabel =
        switch (resources.language()) {
          case "ko" -> "창 크기";
          case "ja" -> "ウィンドウサイズ";
          case "zh-Hans" -> "窗口大小";
          case "zh-Hant" -> "視窗大小";
          default -> "Window size";
        };
    sizing.add(new JLabel(sizeLabel), BorderLayout.WEST);
    String[] labels = new String[DesktopWindowState.PRESETS.length];
    for (int i = 0; i < labels.length; i++) {
      int p = DesktopWindowState.PRESETS[i];
      DesktopWindowState.Size size = DesktopWindowState.preset(p);
      labels[i] = p + "% · " + size.width() + " × " + size.height();
    }
    JComboBox<String> sizes = new JComboBox<>(labels);
    int[] actual = host.windowSize();
    int percent = DesktopWindowState.percent(actual[0], actual[1]);
    if (percent < 0)
      ((DefaultComboBoxModel<String>) sizes.getModel())
          .setSelectedItem(actual[0] + " × " + actual[1]);
    else
      for (int i = 0; i < labels.length; i++)
        if (DesktopWindowState.PRESETS[i] == percent) sizes.setSelectedIndex(i);
    sizes.addActionListener(
        event -> {
          int index = sizes.getSelectedIndex();
          if (index >= 0) host.windowSize(DesktopWindowState.PRESETS[index]);
        });
    sizing.add(sizes);
    display.add(sizing);
    display.add(Box.createVerticalStrut(10));

    choice(
        display,
        "Display mode",
        new String[] {"Original16:9", "Adaptive"},
        prefs.dynamic() ? 1 : 0,
        n -> prefs.values.edit().putBoolean("dynamic", n == 1).apply());
    choice(
        display,
        "Letterboxes",
        new String[] {"Black", "Image colors"},
        prefs.ambient() ? 1 : 0,
        n -> prefs.values.edit().putBoolean("ambient", n == 1).apply());
    slider(display, "Blur strength", 0, 100, prefs.ambientBlurPercent(), "ambientBlurPercent");
    slider(display, "Letterbox dimming", 0, 100, prefs.ambientDimPercent(), "ambientDimPercent");
    choice(
        display,
        "Dialogue",
        new String[] {"Full-screen", "Bottom panel", "Automatic"},
        prefs.dialogueMode(),
        n -> prefs.values.edit().putInt("dialogue", n).apply());
    slider(
        display,
        "Background dimming",
        0,
        100,
        prefs.fullscreenDimPercent(),
        "fullscreenDimPercent");
    slider(display, "Font size", 75, 150, prefs.fontPercent(), "fontPercent");
    slider(display, "Side margins", 0, 160, prefs.marginPercent(), "marginPercent");
    slider(display, "Rendering resolution", 50, 100, prefs.renderPercent(), "renderPercent");
    game.add(
        button(
            "Apply Korean patch…",
            () -> {
              JFileChooser chooser = new JFileChooser();
              chooser.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
              if (chooser.showOpenDialog(d) != JFileChooser.APPROVE_OPTION) return;
              try {
                File patch = DesktopKoreanPatch.folder(chooser.getSelectedFile());
                context
                    .getSharedPreferences("launcher", 0)
                    .edit()
                    .putString("patch", patch.getPath())
                    .apply();
                JOptionPane.showMessageDialog(
                    d, "The Korean patch will be applied when the launcher is restarted.");
              } catch (IOException failure) {
                error(failure);
              }
            }));
    String[] languages = LanguagePolicy.choices(resources.steam.korean());
    int selected = Arrays.asList(languages).indexOf(prefs.values.getString("language", "auto"));
    choice(
        game,
        "Language",
        LanguagePolicy.labels(resources.steam.korean()),
        Math.max(0, selected),
        n -> {
          prefs.values.edit().putString("language", languages[n]).apply();
          host.language(
              LanguagePolicy.resolve(resources.steam.korean(), languages[n], Locale.getDefault()));
        });
    slider(game, "Music volume", 0, 100, prefs.volume("bgm"), "volume_bgm");
    slider(game, "Voice volume", 0, 100, prefs.volume("voice"), "volume_voice");
    slider(game, "Sound effects", 0, 100, prefs.volume("se"), "volume_se");
    slider(game, "Text speed", 0, 100, prefs.textSpeed(), "textSpeed");
    slider(game, "Auto delay (ms)", 500, 10000, prefs.autoDelay(), "autoDelay");
    DesktopOverlays.show(d);
  }

  private JPanel column() {
    JPanel p = new JPanel();
    p.setLayout(new BoxLayout(p, BoxLayout.Y_AXIS));
    p.setBorder(new EmptyBorder(12, 16, 12, 16));
    return p;
  }

  private void choice(
      JPanel root, String label, String[] choices, int selected, IntConsumer change) {
    JPanel row = new JPanel(new BorderLayout(12, 4));
    row.setMaximumSize(new Dimension(Integer.MAX_VALUE, 48));
    row.add(new JLabel(translations.text(label)), BorderLayout.WEST);
    JComboBox<String> combo =
        new JComboBox<>(Arrays.stream(choices).map(translations::text).toArray(String[]::new));
    combo.setSelectedIndex(selected);
    combo.addActionListener(e -> change.accept(combo.getSelectedIndex()));
    row.add(combo);
    root.add(row);
    root.add(Box.createVerticalStrut(10));
  }

  private void slider(JPanel root, String label, int min, int max, int value, String key) {
    JPanel row = new JPanel(new BorderLayout(12, 4));
    row.setMaximumSize(new Dimension(Integer.MAX_VALUE, 64));
    JLabel title = new JLabel(translations.text(label) + " · " + value);
    JSlider slider = new JSlider(min, max, value);
    slider.addChangeListener(
        e -> {
          title.setText(translations.text(label) + " · " + slider.getValue());
          prefs.values.edit().putInt(key, slider.getValue()).apply();
        });
    row.add(title, BorderLayout.NORTH);
    row.add(slider);
    root.add(row);
    root.add(Box.createVerticalStrut(10));
  }

  private void history() {
    JSONObject state = host.call("history", "");
    HistoryModel model = HistoryModel.parse(state);
    JDialog d = dialog("History");
    JPanel rows = column();
    JScrollPane scroll = new JScrollPane(rows);
    scroll.getVerticalScrollBar().setUnitIncrement(36);
    d.add(scroll);
    JButton lastAction = null;
    for (HistoryModel.Group group : model.groups) {
      JPanel row = new JPanel(new BorderLayout(8, 8));
      JPanel text = new JPanel();
      text.setBackground(new Color(0x0f1c2e));
      text.setLayout(new BoxLayout(text, BoxLayout.Y_AXIS));
      for (HistoryModel.Row item : group.rows)
        try {
          Bitmap rich =
              TextOverlay.captureHistory(
                  item.text, 740, 22, item.nativeFont, item.color, item.italic);
          text.add(new JLabel(new ImageIcon(rich.image)));
        } catch (Exception error) {
          throw new IllegalStateException(error);
        }
      row.add(text);
      JPanel actions = new JPanel();
      JButton rewind =
          button(
              "↶",
              () -> {
                if (JOptionPane.showConfirmDialog(
                        d, "Return to this scene?", "Rewind", JOptionPane.YES_NO_OPTION)
                    != JOptionPane.YES_OPTION) return;
                host.call("historyRewind", group.payload(model.revision));
                d.dispose();
              });
      rewind.setEnabled(group.rewindable);
      DesktopDialogFocus.browseDisabled(rewind);
      JButton replay = button("▶", () -> host.call("historyReplay", group.payload(model.revision)));
      replay.setEnabled(group.playable);
      lastAction = rewind;
      actions.add(replay);
      actions.add(rewind);
      row.add(actions, BorderLayout.EAST);
      rows.add(row);
      rows.add(Box.createVerticalStrut(12));
    }
    JButton initialFocus = lastAction;
    DesktopDialogFocus.install(d, initialFocus, true);
    SwingUtilities.invokeLater(
        () -> {
          DesktopDialogFocus.restore(d);
          scroll.getVerticalScrollBar().setValue(scroll.getVerticalScrollBar().getMaximum());
        });
    DesktopOverlays.show(d);
    JSONObject now = host.call("historyPreview", "");
    String pair = HistoryModel.replayPair(now.optJSONObject("historyReplay"));
    if (!pair.isEmpty()) host.call("historyReplayStop", pair);
    if (now.optString("scene").equals("history")) host.call("history", "");
  }

  void awaitStorage() throws Exception {
    java.util.concurrent.CompletableFuture<Void> idle =
        new java.util.concurrent.CompletableFuture<>();
    store.transfer(() -> idle.complete(null));
    idle.get(30, java.util.concurrent.TimeUnit.SECONDS);
  }

  public void close() {
    store.close();
  }
}
