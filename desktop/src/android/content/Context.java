package android.content;

import android.content.res.*;
import java.io.*;

public class Context {
  public static final int MODE_PRIVATE = 0;
  public static File home;
  public static File assets;
  private final java.util.Map<String, SharedPreferences> preferences =
      new java.util.concurrent.ConcurrentHashMap<>();

  public SharedPreferences getSharedPreferences(String name, int mode) {
    return preferences.computeIfAbsent(
        name, n -> new SharedPreferences(new File(home, n + ".properties")));
  }

  public File getFilesDir() {
    return home;
  }

  public File getCacheDir() {
    File f = new File(home, "cache");
    f.mkdirs();
    return f;
  }

  public Context getApplicationContext() {
    return this;
  }

  public AssetManager getAssets() {
    return new AssetManager(assets);
  }

  public Resources getResources() {
    return new Resources();
  }
}
