package org.mahoyo.port;

import java.io.IOException;
import java.nio.ByteBuffer;

/** Each decoder is owned and closed by its one decoding worker. */
final class NativeVideo {
  static {
    System.loadLibrary("woh_android");
  }

  static native long open(String input, long seekMs) throws IOException;

  static native long[] info(long decoder) throws IOException;

  static native long next(long decoder, ByteBuffer rgba) throws IOException;

  static native void close(long decoder);

  private NativeVideo() {}
}
