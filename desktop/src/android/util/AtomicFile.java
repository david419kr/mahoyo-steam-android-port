package android.util;

import java.io.*;
import java.nio.file.*;

public class AtomicFile {
  private final File file, tmp;

  public AtomicFile(File f) {
    file = f;
    tmp = new File(f + ".new");
  }

  public FileOutputStream startWrite() throws IOException {
    file.getParentFile().mkdirs();
    return new FileOutputStream(tmp);
  }

  public void finishWrite(FileOutputStream out) throws IOException {
    out.getFD().sync();
    out.close();
    Files.move(tmp.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING);
  }

  public void failWrite(FileOutputStream out) {
    try {
      if (out != null) out.close();
    } catch (IOException e) {
    }
    tmp.delete();
  }

  public File getBaseFile() {
    return file;
  }

  public InputStream openRead() throws IOException {
    return new FileInputStream(file);
  }
}
