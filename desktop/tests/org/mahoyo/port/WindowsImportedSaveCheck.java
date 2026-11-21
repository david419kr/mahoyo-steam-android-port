package org.mahoyo.port;

import java.io.*;
import java.nio.file.*;
import org.json.*;

/** Loads a copied Android save in an isolated Windows profile; never prints story text. */
public final class WindowsImportedSaveCheck {
  public static void main(String[] args) throws Exception {
    File engine = new File(args[0]).getAbsoluteFile(), profile = engine.getParentFile();
    if (!profile.getPath().contains("windows" + File.separator + "compat-check-"))
      throw new IllegalArgumentException("Use the isolated compatibility fixture");
    File saves = new File(profile, "savedata");
    saves.mkdirs();
    File copy = new File(saves, "slot1.whs");
    Files.copy(Path.of(args[1]), copy.toPath());
    String expected = SaveSyncMerge.hash(copy);
    long h = NativeBridge.createConfigured(engine.getPath(), "korean", "ko");
    try {
      JSONObject result = new JSONObject(NativeBridge.command(h, "load", "1"));
      if (!result.optString("error").isEmpty() || !result.optString("scene").equals("play"))
        throw new AssertionError("Android save did not load on Windows");
      if (!SaveSyncMerge.hash(copy).equals(expected))
        throw new AssertionError("Load modified the saved slot");
      System.out.println("Actual Android slot loaded on Windows; original saved bytes unchanged");
    } finally {
      NativeBridge.destroy(h);
    }
  }
}
