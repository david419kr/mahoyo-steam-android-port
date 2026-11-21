package android.graphics;

import java.io.*;
import javax.imageio.ImageIO;

public class BitmapFactory {
  public static class Options {
    public Bitmap.Config inPreferredConfig = Bitmap.Config.ARGB_8888;
    public int inSampleSize = 1, outWidth, outHeight;
    public boolean inJustDecodeBounds, inPremultiplied = true, inScaled;
    public String outMimeType;
  }

  public static Bitmap decodeFile(String file, Options options) {
    try (InputStream in = new FileInputStream(file)) {
      return decodeStream(in, null, options);
    } catch (IOException e) {
      throw new IllegalArgumentException(e);
    }
  }

  public static Bitmap decodeStream(InputStream in, Object padding, Options o) {
    try {
      Bitmap b = new Bitmap(ImageIO.read(in));
      if (o != null) {
        o.outWidth = b.getWidth();
        o.outHeight = b.getHeight();
        if (o.inJustDecodeBounds) return null;
        if (o.inSampleSize > 1)
          b =
              Bitmap.createScaledBitmap(
                  b,
                  Math.max(1, b.getWidth() / o.inSampleSize),
                  Math.max(1, b.getHeight() / o.inSampleSize),
                  true);
        b.setPremultiplied(o.inPremultiplied);
      }
      return b;
    } catch (IOException e) {
      throw new IllegalArgumentException(e);
    }
  }

  public static Bitmap decodeByteArray(byte[] bytes, int offset, int length, Options o) {
    return decodeStream(new ByteArrayInputStream(bytes, offset, length), null, o);
  }

  public static Bitmap decodeByteArray(byte[] bytes, int offset, int length) {
    return decodeStream(new ByteArrayInputStream(bytes, offset, length), null, null);
  }
}
