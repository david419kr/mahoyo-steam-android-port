package org.mahoyo.port;

import android.graphics.*;
import java.io.IOException;

/** Direct dav1d/libavif preserves 4:4:4 and bypasses MediaCodec's 4096 limit. */
final class AvifCodec {
    static { System.loadLibrary("woh_android"); }
    private static native int[] bounds(byte[] encoded)throws IOException;
    private static native Bitmap decode(byte[] encoded,int sample,boolean crop,int left,int top,int right,int bottom,boolean premultiply)throws IOException;
    static Bitmap bitmap(byte[] encoded,BitmapFactory.Options options)throws IOException{
        if(options!=null&&options.inJustDecodeBounds){options.outWidth=options.outHeight=-1;int[] size=bounds(encoded);options.outWidth=size[0];options.outHeight=size[1];options.outMimeType="image/avif";return null;}
        return decode(encoded,Math.max(1,options==null?1:options.inSampleSize),false,0,0,0,0,options==null||options.inPremultiplied);
    }
    static Bitmap region(byte[] encoded,Rect rect,BitmapFactory.Options options)throws IOException{
        return decode(encoded,Math.max(1,options==null?1:options.inSampleSize),true,rect.left,rect.top,rect.right,rect.bottom,options==null||options.inPremultiplied);
    }
}
