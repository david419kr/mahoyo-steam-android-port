package android.graphics;

import java.io.*;

public class BitmapRegionDecoder {
  private final Bitmap bitmap;

  private BitmapRegionDecoder(Bitmap b) {
    bitmap = b;
  }

  public static BitmapRegionDecoder newInstance(String path, boolean share) throws IOException {
    return new BitmapRegionDecoder(BitmapFactory.decodeFile(path, null));
  }

  public static BitmapRegionDecoder newInstance(InputStream in, boolean share) throws IOException {
    return new BitmapRegionDecoder(BitmapFactory.decodeStream(in, null, null));
  }

  public Bitmap decodeRegion(Rect r, BitmapFactory.Options o) {
    Bitmap b = Bitmap.createBitmap(bitmap, r.left, r.top, r.width(), r.height());
    int n = o == null ? 1 : Math.max(1, o.inSampleSize);
    return n == 1
        ? b
        : Bitmap.createScaledBitmap(
            b, Math.max(1, b.getWidth() / n), Math.max(1, b.getHeight() / n), true);
  }

  public void recycle() {}
}
