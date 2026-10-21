package org.mahoyo.port;

final class NativeBridge {
    static { System.loadLibrary("woh_android"); }
    static native long create(String root);
    static native long createConfigured(String root,String edition,String language);
    static native String command(long handle, String type, String payload);
    static native void waveClock(long handle, long uptimeMs);
    static native String snapshot(long handle);
    static native void destroy(long handle);
    private NativeBridge() {}
}
