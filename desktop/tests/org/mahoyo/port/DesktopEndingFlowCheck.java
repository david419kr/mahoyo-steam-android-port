package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.nio.file.*;
import org.json.*;

/** Runs the real hidden GLFW render loop against an isolated authored ending script. */
public final class DesktopEndingFlowCheck {
  public static void main(String[] args) throws Exception {
    Context.home = new File(args[0]).getCanonicalFile();
    if (!Context.home.getName().startsWith("ending-check-"))
      throw new IllegalArgumentException("Use an isolated ending-check profile");
    Context.assets = new File(args[1]).getCanonicalFile();
    System.setProperty("mahoyo.smoke", "true");
    System.setProperty("mahoyo.smoke.play", "true");
    System.setProperty("mahoyo.smoke.frames", "120");
    System.setProperty("mahoyo.width", "960");
    System.setProperty("mahoyo.height", "540");
    Context context = new Context();
    var prefs = context.getSharedPreferences("display", Context.MODE_PRIVATE).edit();
    for (String channel : new String[] {"bgm", "voice", "se", "system"})
      prefs.putInt("volume_" + channel, 0);
    prefs.apply();
    GameResources resources = new GameResources(context, new File(Context.home, "engine"));
    resources.setLanguage("ja");
    long engine =
        NativeBridge.createConfigured(
            new File(Context.home, "engine").getPath(),
            resources.steam.korean() ? "korean" : "vanilla",
            "ja");
    try {
      DesktopWindow window = new DesktopWindow(context, resources, engine);
      window.run();
      JSONObject state = new JSONObject(NativeBridge.snapshot(engine));
      if (!state.optString("scene").equals("title") || !state.optString("error").isEmpty()) {
        throw new AssertionError(
            "Authored ending did not return to Title: "
                + state.optString("waiting")
                + " "
                + state.optString("error"));
      }
      byte[] bytes = Files.readAllBytes(new File(Context.home, "savedata/shared.whp").toPath());
      int tag =
          new String(bytes, java.nio.charset.StandardCharsets.ISO_8859_1).indexOf("WOHSHARED1");
      java.nio.ByteBuffer data =
          java.nio.ByteBuffer.wrap(bytes).order(java.nio.ByteOrder.LITTLE_ENDIAN);
      int flags = tag + 26 + data.getInt(tag + 18) * 4;
      if (tag < 0 || bytes[flags + 128] != 1)
        throw new AssertionError("Clear flag was not persisted");
      String trophies =
          Files.readString(
              new File(Context.home, "savedata/trophies.whp").toPath(),
              java.nio.charset.StandardCharsets.ISO_8859_1);
      if (!trophies.contains("WOHTROPHY1 1\n14\n"))
        throw new AssertionError("Completion trophy was not persisted");
      System.out.println(
          "Unfocused Windows render loop: authored ending -> automatic Title; clear flag and trophy"
              + " preserved");
    } finally {
      NativeBridge.destroy(engine);
    }
  }
}
