package org.mahoyo.port;

/** Free bytes exclude existing files; sparse logical length is not reusable data. */
final class InstallSpace {
    static final long OBB_RESERVE=256L*1024*1024,STEAM_RESERVE=512L*1024*1024;
    static boolean enough(long free,long total,long allocated,long reserve){
        return free>=reserve&&Math.max(0,total-Math.max(0,allocated))<=free-reserve;
    }
    private InstallSpace(){}
}
