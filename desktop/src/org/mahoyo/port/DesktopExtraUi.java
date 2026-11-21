package org.mahoyo.port;

import android.graphics.Bitmap;
import java.awt.*;
import java.util.*;
import javax.swing.*;
import org.json.*;

/** Desktop views for the same archive/gallery/music DTOs used on Android. */
final class DesktopExtraUi {
  private final DesktopDialogs.Host host;
  private final GameResources resources;
  private final DesktopText text;

  DesktopExtraUi(DesktopDialogs.Host host, GameResources resources) {
    this.host = host;
    this.resources = resources;
    text = new DesktopText(resources);
  }

  void show() {
    int kind =
        JOptionPane.showOptionDialog(
            null,
            "",
            text.text("Extras"),
            JOptionPane.DEFAULT_OPTION,
            JOptionPane.PLAIN_MESSAGE,
            null,
            new Object[] {"Archive", "Gallery", "Music"},
            null);
    if (kind == 0) archive();
    else if (kind == 1) gallery();
    else if (kind == 2) music();
  }

  private JDialog dialog(String title) {
    JDialog d = new JDialog((Frame) null, text.text(title), true);
    d.setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);
    d.setSize(1100, 760);
    d.setLocationRelativeTo(null);
    d.getRootPane()
        .registerKeyboardAction(
            e -> d.dispose(), KeyStroke.getKeyStroke("ESCAPE"), JComponent.WHEN_IN_FOCUSED_WINDOW);
    return d;
  }

  private ImageIcon image(String name, int width, int height) {
    try {
      android.graphics.BitmapFactory.Options options = new android.graphics.BitmapFactory.Options();
      options.inJustDecodeBounds = true;
      resources.file(name).bitmap(options);
      options.inJustDecodeBounds = false;
      options.inSampleSize =
          Math.max(
              1,
              Math.min(
                  options.outWidth / Math.max(1, width), options.outHeight / Math.max(1, height)));
      Bitmap image = resources.file(name).bitmap(options);
      double scale =
          Math.min(width / (double) image.getWidth(), height / (double) image.getHeight());
      return new ImageIcon(
          image.image.getScaledInstance(
              Math.max(1, (int) (image.getWidth() * scale)),
              Math.max(1, (int) (image.getHeight() * scale)),
              Image.SCALE_SMOOTH));
    } catch (Exception e) {
      throw new IllegalStateException(e);
    }
  }

  private JButton button(String label, Runnable action) {
    JButton b = new JButton(text.text(label));
    b.addActionListener(
        e -> {
          try {
            action.run();
          } catch (Exception failure) {
            JOptionPane.showMessageDialog(
                null, failure.getMessage(), "Mahoyo", JOptionPane.ERROR_MESSAGE);
          }
        });
    return b;
  }

  private void archive() {
    JSONObject state = host.call("archiveOpen", "");
    ArchiveCatalog model = ArchiveCatalog.parse(state.optJSONObject("archive"));
    if (model == null) return;
    JDialog d = dialog("Archive");
    Dimension available = DesktopOverlays.available(d);
    d.setSize(Math.min(1100, available.width), Math.min(820, available.height));
    Color background = new Color(0x0b1423);
    JPanel root = new JPanel(new BorderLayout(0, 10));
    root.setBackground(background);
    root.setBorder(BorderFactory.createEmptyBorder(12, 16, 16, 16));
    JPanel top = new JPanel(new BorderLayout());
    top.setOpaque(false);
    JLabel heading = new JLabel(text.text("Archive"));
    heading.setFont(heading.getFont().deriveFont(Font.BOLD, 20f));
    heading.setForeground(new Color(0xeaf1fc));
    top.add(heading, BorderLayout.WEST);
    JButton back = button(text.text(model.backLabel()), d::dispose);
    top.add(back, BorderLayout.EAST);
    root.add(top, BorderLayout.NORTH);
    JPanel sections = new JPanel();
    sections.setLayout(new BoxLayout(sections, BoxLayout.Y_AXIS));
    sections.setBackground(background);
    int columns = Math.max(1, Math.min(4, (d.getWidth() - 64) / 244));
    boolean[] selected = {false};
    JButton initial = null;
    for (int start = 0; start < model.entries.size(); ) {
      ArchiveCatalog.Entry first = model.entries.get(start);
      JPanel section = new JPanel(new BorderLayout(0, 10));
      section.setOpaque(false);
      section.setAlignmentX(Component.LEFT_ALIGNMENT);
      JLabel header = new JLabel(image(first.header, 320, 54));
      header.setHorizontalAlignment(SwingConstants.LEFT);
      section.add(header, BorderLayout.NORTH);
      JPanel cards = new JPanel(new GridLayout(0, columns, 8, 8));
      cards.setOpaque(false);
      int count = 0;
      while (start < model.entries.size() && model.entries.get(start).group == first.group) {
        ArchiveCatalog.Entry item = model.entries.get(start++);
        JButton card =
            button(
                "",
                () -> {
                  host.call("archivePlay", model.play(item.id));
                  selected[0] = true;
                  d.dispose();
                });
        card.setEnabled(item.unlocked);
        card.setFocusable(item.unlocked);
        card.setLayout(new BorderLayout(0, 6));
        card.setUI(new javax.swing.plaf.basic.BasicButtonUI());
        card.setBackground(new Color(item.unlocked ? 0x1e3047 : 0x101d2e));
        card.setForeground(new Color(0xe5eefb));
        card.setBorder(
            BorderFactory.createCompoundBorder(
                BorderFactory.createLineBorder(new Color(0x405674)),
                BorderFactory.createEmptyBorder(8, 8, 8, 8)));
        card.setPreferredSize(new Dimension(236, 204));
        JPanel status = new JPanel(new FlowLayout(FlowLayout.RIGHT, 0, 0));
        status.setOpaque(false);
        status.setPreferredSize(new Dimension(1, 21));
        if (item.unread()) {
          JLabel badge = new JLabel(text.text("未読"));
          badge.setOpaque(true);
          badge.setBackground(new Color(0x345d82));
          badge.setForeground(new Color(0xeaf4ff));
          badge.setBorder(BorderFactory.createEmptyBorder(2, 7, 2, 7));
          status.add(badge);
        }
        card.add(status, BorderLayout.NORTH);
        card.add(
            new JLabel(item.unlocked ? image(item.preview, 212, 116) : archiveLock()),
            BorderLayout.CENTER);
        String title = item.label.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
        JLabel label =
            new JLabel(
                item.unlocked
                    ? "<html><div style='width:150px;text-align:center'>" + title + "</div></html>"
                    : " ",
                SwingConstants.CENTER);
        label.setFont(label.getFont().deriveFont(14f));
        label.setForeground(new Color(0xe5eefb));
        label.setPreferredSize(new Dimension(212, 38));
        card.add(label, BorderLayout.SOUTH);
        card.getAccessibleContext()
            .setAccessibleName(
                item.unlocked
                    ? item.label + text.text(item.read ? "　既読" : "　未読")
                    : text.text("未解放"));
        cards.add(card);
        if (item.unlocked && (initial == null || item.id == model.selected)) initial = card;
        count++;
      }
      while (count++ % columns != 0) cards.add(Box.createGlue());
      section.add(cards, BorderLayout.CENTER);
      section.setMaximumSize(new Dimension(Integer.MAX_VALUE, section.getPreferredSize().height));
      sections.add(section);
      sections.add(Box.createVerticalStrut(22));
    }
    JScrollPane scroll = new JScrollPane(sections);
    scroll.setBorder(BorderFactory.createEmptyBorder());
    scroll.setHorizontalScrollBarPolicy(ScrollPaneConstants.HORIZONTAL_SCROLLBAR_NEVER);
    scroll.getVerticalScrollBar().setUnitIncrement(28);
    root.add(scroll, BorderLayout.CENTER);
    d.setContentPane(root);
    DesktopDialogFocus.install(d, initial == null ? back : initial, true);
    DesktopDialogFocus.select(d, initial == null ? back : initial, true);
    SwingUtilities.invokeLater(() -> DesktopDialogFocus.restore(d));
    DesktopOverlays.show(d);
    if (!selected[0]) host.call("archiveBack", model.back());
  }

  private static ImageIcon archiveLock() {
    java.awt.image.BufferedImage bitmap =
        new java.awt.image.BufferedImage(36, 36, java.awt.image.BufferedImage.TYPE_INT_ARGB);
    Graphics2D g = bitmap.createGraphics();
    g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    g.setColor(new Color(0x70839b));
    g.setStroke(new BasicStroke(2));
    g.drawArc(11, 3, 14, 20, 0, 180);
    g.drawRoundRect(7, 13, 22, 19, 3, 3);
    g.drawLine(18, 20, 18, 25);
    g.dispose();
    return new ImageIcon(bitmap);
  }

  private void gallery() {
    JSONObject state = host.call("galleryOpen", "");
    ExtraModels.Gallery model = ExtraModels.gallery(state.optJSONObject("gallery"));
    if (model == null) return;
    JDialog d = dialog("Gallery");
    JPanel cards = new JPanel(new GridLayout(0, 5, 8, 8));
    for (ExtraModels.Group item : model.groups) {
      JButton card =
          button(
              item.count == 0 ? "???" : Integer.toString(item.id + 1),
              () -> {
                ExtraModels.Gallery selected =
                    ExtraModels.gallery(
                        host.call("galleryAction", model.select(item.id)).optJSONObject("gallery"));
                if (selected != null) viewer(selected);
              });
      card.setEnabled(item.count > 0);
      if (item.count > 0) card.setIcon(image(item.preview, 180, 110));
      card.setHorizontalTextPosition(SwingConstants.CENTER);
      card.setVerticalTextPosition(SwingConstants.BOTTOM);
      cards.add(card);
    }
    d.add(new JScrollPane(cards));
    DesktopOverlays.show(d);
    host.call("galleryAction", model.id + " back");
  }

  private void viewer(ExtraModels.Gallery initial) {
    JDialog d = dialog("Gallery");
    ExtraModels.Gallery[] model = {initial};
    JLabel picture = new JLabel();
    picture.setHorizontalAlignment(SwingConstants.CENTER);
    JScrollPane scroll = new JScrollPane(picture);
    d.add(scroll);
    Runnable render =
        () -> {
          ExtraModels.Variant v = model[0].current();
          if (v != null)
            picture.setIcon(
                image(
                    v.name, Math.max(800, d.getWidth() - 30), Math.max(500, d.getHeight() - 100)));
        };
    JPanel buttons = new JPanel();
    buttons.add(
        button(
            "‹",
            () -> {
              model[0] =
                  ExtraModels.gallery(
                      host.call("galleryAction", model[0].action("previous"))
                          .optJSONObject("gallery"));
              render.run();
            }));
    buttons.add(
        button(
            "›",
            () -> {
              model[0] =
                  ExtraModels.gallery(
                      host.call("galleryAction", model[0].action("next")).optJSONObject("gallery"));
              render.run();
            }));
    buttons.add(button("Close", d::dispose));
    d.add(buttons, BorderLayout.SOUTH);
    render.run();
    DesktopOverlays.show(d);
    host.call("galleryAction", model[0].id + " back");
  }

  private void music() {
    ExtraModels.Music model =
        ExtraModels.music(host.call("musicOpen", "").optJSONObject("musicRoom"));
    if (model == null) return;
    host.call("resume", "");
    JDialog d = dialog("Music");
    JPanel rows = new JPanel(new GridLayout(0, 2, 6, 6));
    for (ExtraModels.Track track : model.tracks) {
      JButton play =
          button(
              track.unlocked ? "▶ " + (track.row + 1) : "???",
              () -> host.call("musicAction", model.play(track.row)));
      play.setEnabled(track.unlocked);
      if (track.unlocked)
        try {
          Bitmap atlas =
              resources.file(track.atlas).bitmap(new android.graphics.BitmapFactory.Options());
          Bitmap crop =
              Bitmap.createBitmap(
                  atlas, track.crop[0], track.crop[1], track.crop[2], track.crop[3]);
          play.setIcon(new ImageIcon(crop.image.getScaledInstance(330, 45, Image.SCALE_SMOOTH)));
          play.setText("▶");
        } catch (Exception e) {
          throw new IllegalStateException(e);
        }
      rows.add(play);
    }
    JPanel toolbar = new JPanel();
    toolbar.add(button("Stop", () -> host.call("musicAction", model.id + " stop")));
    toolbar.add(button("Repeat", () -> host.call("musicAction", model.id + " repeat")));
    toolbar.add(button("Close", d::dispose));
    d.add(new JScrollPane(rows));
    d.add(toolbar, BorderLayout.SOUTH);
    DesktopOverlays.show(d);
    host.call("musicAction", model.id + " back");
  }
}
