package org.mahoyo.port;

import android.content.Context;
import java.io.*;
import java.util.*;

final class GameResources {
  final SteamResources steam;
  private volatile String language;

  GameResources(Context c, File root) throws Exception {
    steam = new SteamResources(c, root);
    language =
        LanguagePolicy.resolve(
            steam.korean(),
            c.getSharedPreferences("display", 0).getString("language", "auto"),
            Locale.getDefault());
  }

  GameAsset file(String name) {
    try {
      return steam.file(name, language);
    } catch (IOException e) {
      throw new IllegalArgumentException(e);
    }
  }

  void setLanguage(String v) {
    language = v;
  }

  String language() {
    return language;
  }

  Collection<String> imageFiles() {
    return steam.imageFiles();
  }
}
