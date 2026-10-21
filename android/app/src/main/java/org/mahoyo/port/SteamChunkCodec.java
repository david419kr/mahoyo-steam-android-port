package org.mahoyo.port;

/** Native LZMA for depot VZip chunks. DepotChunk still verifies the length and Adler checksum. */
final class SteamChunkCodec {
    static { System.loadLibrary("woh_android"); }
    static native int decodeVzip(byte[] encoded, int length, byte[] destination);
}
