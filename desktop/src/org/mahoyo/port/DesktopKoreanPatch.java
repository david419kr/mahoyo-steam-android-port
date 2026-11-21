package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.nio.file.Files;
import org.json.*;

/** Validates the existing patch once; reads its data in place and never executes its DLL. */
final class DesktopKoreanPatch {
  static File folder(File selected) throws IOException {
    if (new File(selected, "mahoyo_kr.dll").isFile()) return selected.getCanonicalFile();
    File nested = new File(selected, "patch_KR");
    if (new File(nested, "mahoyo_kr.dll").isFile()) return nested.getCanonicalFile();
    throw new IOException(
        "Select the patch_KR folder containing mahoyo_kr.dll and the three mahoyo_KR DAT files.");
  }

  static File validate(Context context, String selected) throws Exception {
    if (selected.isEmpty()) return null;
    File patch = folder(new File(selected));
    JSONObject specification =
        SteamResources.assetJson(context, "steam-profile/source-profile.json", 1024 * 1024)
            .getJSONObject("koreanPatch");
    File receipt = new File(Context.home, "patch-validation.json");
    JSONObject old = null;
    if (receipt.isFile())
      try {
        old = new JSONObject(Files.readString(receipt.toPath()));
      } catch (Exception invalid) {
        old = null;
      }
    String profile =
        SteamResources.sha(
            specification.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
    boolean cached =
        old != null
            && old.optString("path").equals(patch.getPath())
            && old.optString("profile").equals(profile);
    JSONArray files = specification.getJSONArray("files"), stamps = new JSONArray();
    for (int i = 0; i < files.length(); i++) {
      JSONObject row = files.getJSONObject(i);
      File file = new File(patch, row.getString("name"));
      if (!file.isFile() || file.length() != row.getLong("bytes"))
        throw new IOException("Missing or unsupported Korean patch file: " + file.getName());
      JSONObject previous = cached ? old.optJSONObject(file.getName()) : null;
      if (previous == null
          || previous.optLong("modified") != file.lastModified()
          || previous.optLong("bytes") != file.length()) cached = false;
      stamps.put(
          new JSONObject()
              .put("name", file.getName())
              .put("modified", file.lastModified())
              .put("bytes", file.length()));
    }
    if (!cached) {
      for (int i = 0; i < files.length(); i++) {
        JSONObject row = files.getJSONObject(i);
        String expected = row.getString("sha256");
        if (!SaveSyncMerge.hash(new File(patch, row.getString("name"))).equals(expected))
          throw new IOException("Unsupported or damaged Korean patch: " + row.getString("name"));
      }
      JSONObject record = new JSONObject().put("path", patch.getPath()).put("profile", profile);
      for (int i = 0; i < stamps.length(); i++) {
        JSONObject row = stamps.getJSONObject(i);
        record.put(row.getString("name"), row);
      }
      SteamResources.write(
          receipt, record.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }
    return patch;
  }

  private DesktopKoreanPatch() {}
}
