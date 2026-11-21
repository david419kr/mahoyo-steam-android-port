package org.mahoyo.port;

import android.graphics.Bitmap;
import java.io.*;
import java.nio.file.*;
import java.util.concurrent.*;
import org.json.*;

/** Uses only a dedicated test profile; no phone or user save directory is touched. */
public final class WindowsSaveCheck {
  private static JSONObject command(long h, String type, String payload) {
    JSONObject s = new JSONObject(NativeBridge.command(h, type, payload));
    if (!s.optString("error").isEmpty()) throw new AssertionError(s.optString("error"));
    return s;
  }

  public static void main(String[] args) throws Exception {
    File engine = new File(args[0]).getAbsoluteFile(),
        profile = engine.getParentFile(),
        saves = new File(profile, "savedata"),
        thumbs = new File(profile, "slot-thumbnails");
    if (!profile.getPath().contains("windows" + File.separator + "save-check"))
      throw new IllegalArgumentException("Use only the isolated save-check fixture");
    long h = NativeBridge.createConfigured(engine.getPath(), "vanilla", "en");
    SlotThumbnailStore store = new SlotThumbnailStore(saves, thumbs);
    try {
      JSONObject state = command(h, "action", "start");
      for (int i = 0; i < 200 && !state.optBoolean("canSave"); i++) {
        if (state.optString("waiting").equals("transitionReady")) {
          JSONObject t = state.optJSONObject("transition");
          String ack = state.optString("alphaRulePrepareAck", "");
          if (ack.isEmpty() && t != null) ack = t.optString("id");
          state = command(h, "transitionReady", ack);
        }
        state = command(h, "tick", Integer.toString(i * 50));
      }
      if (!state.optBoolean("canSave"))
        throw new AssertionError("First play input is not saveable: " + state.optString("waiting"));
      command(h, "pause", "");
      state = command(h, "save", "1");
      if (!state.optString("notice").equals("Saved"))
        throw new AssertionError(state.optString("notice"));
      SlotSaveMetadata original = SlotSaveMetadata.read(new File(saves, "slot1.whs"));
      Bitmap image = Bitmap.createBitmap(320, 180, Bitmap.Config.ARGB_8888);
      image.eraseColor(0xff294567);
      store.saved(1, image);
      CountDownLatch done = new CountDownLatch(1);
      store.transfer(done::countDown);
      if (!done.await(10, TimeUnit.SECONDS)) throw new AssertionError("Thumbnail timed out");
      if (!new File(thumbs, "slot1.thumb").isFile()) throw new AssertionError("Thumbnail missing");
      command(h, "backupPrepare", "");
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      File workspace = new File(profile, "zip-work");
      try (SaveArchive.Archive archive = SaveArchive.snapshot(saves, thumbs, workspace)) {
        SaveArchive.write(archive, bytes, "windows-test");
      }
      try (SaveArchive.Archive archive =
          SaveArchive.read(new ByteArrayInputStream(bytes.toByteArray()), workspace)) {
        SaveArchive.Plan plan = SaveArchive.plan(archive, saves, thumbs);
        if (plan.slots.get(1) != 2 || !plan.overwrites.isEmpty())
          throw new AssertionError("Import must shift occupied slot1 to slot2");
        SaveArchive.apply(
            plan,
            new File(profile, "save-transfer"),
            false,
            () -> command(h, "progressImport", ""));
      }
      SlotSaveMetadata imported = SlotSaveMetadata.read(new File(saves, "slot2.whs"));
      if (!original.sha256.equals(imported.sha256))
        throw new AssertionError("Portable save changed during ZIP import");
      state = command(h, "load", "2");
      if (!state.optString("scene").equals("play"))
        throw new AssertionError("Native Windows load did not resume play");
      CountDownLatch listed = new CountDownLatch(1);
      boolean[] bound = {false};
      store.list(
          entries -> {
            for (SlotThumbnailStore.Entry entry : entries)
              if (entry.slot == 2) bound[0] = entry.thumbnail != null;
            listed.countDown();
          });
      if (!listed.await(10, TimeUnit.SECONDS) || !bound[0])
        throw new AssertionError("Imported thumbnail is not bound to shifted save");
      Bitmap ruby =
          TextOverlay.captureHistory("AB<CD|@_reading>@a1EF", 720, 22, 40, 0xffeef5ff, false);
      if (ruby.getWidth() != 720 || ruby.getHeight() < 22)
        throw new AssertionError("Rich history layout failed");
      System.out.println(
          "Windows native save/load, portable ZIP, slot shifting, thumbnail binding and rich"
              + " history passed");
    } finally {
      store.close();
      NativeBridge.destroy(h);
    }
  }
}
