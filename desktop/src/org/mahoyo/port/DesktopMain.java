package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.util.*;
import javax.swing.*;
import org.json.*;

public final class DesktopMain {
  public static void main(String[] args) {
    try {
      run(args);
    } catch (Throwable failure) {
      failure.printStackTrace();
      if (!Boolean.getBoolean("mahoyo.smoke"))
        JOptionPane.showMessageDialog(
            null, failure.getMessage(), "WITCH ON THE HOLY NIGHT", JOptionPane.ERROR_MESSAGE);
      System.exit(1);
    }
  }

  private static void run(String[] args) throws Exception {
    Context.home =
        new File(
            System.getProperty("mahoyo.home", System.getenv("LOCALAPPDATA") + "/MahoyoUnofficial"));
    Context.assets = new File(System.getProperty("mahoyo.assets", "build/windows/assets"));
    Context context = new Context();
    try (DesktopProfileLease lease = DesktopProfileLease.acquire(Context.home)) {
      if (lease == null)
        throw new IOException("This profile is already in use by the player or save synchronizer.");
      boolean probe = args.length > 0 && args[0].equals("--probe");
      File source;
      if (args.length == 2 && (probe || args[0].equals("--play")))
        source = new File(args[1]).getCanonicalFile();
      else if (args.length == 0) {
        String last = context.getSharedPreferences("launcher", 0).getString("source", "");
        final File[] selected = {null};
        if (!last.isEmpty() && new File(last, "WoH.exe").isFile()) selected[0] = new File(last);
        else
          SwingUtilities.invokeAndWait(
              () -> {
                JFileChooser chooser = new JFileChooser();
                chooser.setDialogTitle("Select the Steam WITCH ON THE HOLY NIGHT folder");
                chooser.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
                if (chooser.showOpenDialog(null) == JFileChooser.APPROVE_OPTION)
                  selected[0] = chooser.getSelectedFile();
              });
        if (selected[0] == null) return;
        source = selected[0].getCanonicalFile();
      } else throw new IllegalArgumentException("Usage: Mahoyo.exe [--play STEAM_DIRECTORY]");
      if (!new File(source, "WoH.exe").isFile())
        throw new IOException("Select the Steam game folder containing WoH.exe.");
      File patch =
          DesktopKoreanPatch.validate(
              context,
              System.getProperty(
                  "mahoyo.patch",
                  context.getSharedPreferences("launcher", 0).getString("patch", "")));
      File root = new File(Context.home, "engine");
      boolean ready =
          SteamResources.available(root) && SteamResources.sourceAvailable(context, root);
      if (ready) {
        JSONObject marker =
            new JSONObject(
                java.nio.file.Files.readString(new File(root, SteamResources.MARKER).toPath()));
        ready =
            new File(marker.getString("source")).getCanonicalFile().equals(source)
                && ((patch == null && !marker.has("patch"))
                    || (patch != null
                        && marker.optString("patch").equals(patch.getCanonicalPath())));
      }
      if (!ready) SteamResources.prepare(context, source, root, patch, System.err::println);
      context
          .getSharedPreferences("launcher", 0)
          .edit()
          .putString("source", source.getPath())
          .apply();
      SaveArchive.recover(
          new File(Context.home, "savedata"),
          new File(Context.home, "slot-thumbnails"),
          new File(Context.home, "save-transfer"));
      GameResources resources = new GameResources(context, root);
      long runtime =
          NativeBridge.createConfigured(
              root.getPath(),
              resources.steam.korean() ? "korean" : "vanilla",
              resources.language());
      try {
        JSONObject state = new JSONObject(NativeBridge.snapshot(runtime));
        if (!state.optString("error").isEmpty()) throw new IOException(state.optString("error"));
        System.out.println(
            "Windows native runtime ready; language="
                + resources.language()
                + " edition="
                + (resources.steam.korean() ? "korean" : "vanilla")
                + " scene="
                + state.optString("scene"));
        BitmapProbe.check(resources);
        if (!probe) new DesktopWindow(context, resources, runtime).run();
      } finally {
        NativeBridge.destroy(runtime);
      }
    }
  }
}
