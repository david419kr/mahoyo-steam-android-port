package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import org.json.*;

/** Shares the Android UI catalogue instead of maintaining a second set of translations. */
final class DesktopText {
  private final GameResources resources;
  private final Map<String, JSONArray> rows = new HashMap<>();

  DesktopText(GameResources resources) {
    this.resources = resources;
    try {
      JSONArray messages =
          new JSONObject(Files.readString(new File(Context.assets, "ui-text.json").toPath()))
              .getJSONArray("messages");
      for (int i = 0; i < messages.length(); i++) {
        JSONArray row = messages.getJSONArray(i);
        rows.put(row.getString(0), row);
        for (int j = 2; j < row.length(); j++) rows.put(row.getString(j), row);
        JSONArray aliases = row.getJSONArray(1);
        for (int j = 0; j < aliases.length(); j++) rows.put(aliases.getString(j), row);
      }
    } catch (IOException error) {
      throw new IllegalStateException(error);
    }
  }

  String text(String value) {
    JSONArray row = rows.get(value);
    if (row == null) return value;
    int index =
        switch (resources.language()) {
          case "ja" -> 3;
          case "ko" -> 4;
          case "zh-Hans" -> 5;
          case "zh-Hant" -> 6;
          default -> 2;
        };
    return row.getString(index);
  }
}
