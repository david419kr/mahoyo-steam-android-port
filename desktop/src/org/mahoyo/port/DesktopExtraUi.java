package org.mahoyo.port;

import android.graphics.Bitmap;
import java.awt.*;
import java.util.*;
import javax.swing.*;
import org.json.*;

/** Desktop views for the same unlocked archive/gallery/music DTOs used on Android. */
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
    JPanel cards = new JPanel(new GridLayout(0, 4, 8, 8));
    boolean[] selected = {false};
    for (ArchiveCatalog.Entry item : model.entries) {
      JButton card =
          button(
              item.label,
              () -> {
                host.call("archivePlay", model.play(item.id));
                selected[0] = true;
                d.dispose();
              });
      card.setIcon(image(item.preview, 220, 124));
      card.setHorizontalTextPosition(SwingConstants.CENTER);
      card.setVerticalTextPosition(SwingConstants.BOTTOM);
      cards.add(card);
    }
    d.add(new JScrollPane(cards));
    d.setVisible(true);
    if (!selected[0]) host.call("archiveBack", model.back());
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
    d.setVisible(true);
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
    d.setVisible(true);
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
    d.setVisible(true);
    host.call("musicAction", model.id + " back");
  }
}
