package android.content;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public final class SharedPreferences {
  private final File file;
  private final Properties values = new Properties();

  public SharedPreferences(File file) {
    this.file = file;
    try (InputStream in = new FileInputStream(file)) {
      values.load(in);
    } catch (FileNotFoundException e) {
    } catch (IOException e) {
      throw new IllegalStateException(e);
    }
  }

  public boolean contains(String k) {
    return values.containsKey(k);
  }

  public boolean getBoolean(String k, boolean d) {
    return Boolean.parseBoolean(values.getProperty(k, "" + d));
  }

  public int getInt(String k, int d) {
    return Integer.parseInt(values.getProperty(k, "" + d));
  }

  public float getFloat(String k, float d) {
    return Float.parseFloat(values.getProperty(k, "" + d));
  }

  public String getString(String k, String d) {
    return values.getProperty(k, d);
  }

  public Editor edit() {
    return new Editor();
  }

  public final class Editor {
    public Editor putBoolean(String k, boolean v) {
      values.setProperty(k, "" + v);
      return this;
    }

    public Editor putInt(String k, int v) {
      values.setProperty(k, "" + v);
      return this;
    }

    public Editor putFloat(String k, float v) {
      values.setProperty(k, "" + v);
      return this;
    }

    public Editor putString(String k, String v) {
      values.setProperty(k, v);
      return this;
    }

    public Editor remove(String k) {
      values.remove(k);
      return this;
    }

    public void apply() {
      try {
        file.getParentFile().mkdirs();
        File tmp = new File(file + ".new");
        try (OutputStream out = new FileOutputStream(tmp)) {
          values.store(out, "Mahoyo Windows");
        }
        Files.move(tmp.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING);
      } catch (IOException e) {
        throw new IllegalStateException(e);
      }
    }

    public boolean commit() {
      apply();
      return true;
    }
  }
}
