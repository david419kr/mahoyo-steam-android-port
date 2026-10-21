package org.mahoyo.port;
import android.graphics.*;
import java.io.IOException;
/** Original HuneX images and scripts; no image files are transcoded on disk. */
final class NativeAssetCodec {
 private static final byte[] CBG="CompressedBG_MT\0".getBytes(java.nio.charset.StandardCharsets.US_ASCII);
 static {System.loadLibrary("woh_android");}
 private static native int[] bounds(byte[] encoded)throws IOException;
 private static native Bitmap decode(byte[] primary,byte[] patch,int sample,int left,int top,int right,int bottom,boolean premultiply)throws IOException;
 static native byte[] lenzu(byte[] encoded)throws IOException;
 static boolean image(byte[] header){if(header.length>=6&&header[0]=='m'&&header[1]=='r'&&header[2]=='g'&&header[3]=='d'&&header[4]=='0'&&header[5]=='0')return true;if(header.length<CBG.length)return false;for(int i=0;i<CBG.length;i++)if(header[i]!=CBG[i])return false;return true;}
 static Bitmap bitmap(byte[] primary,byte[] patch,BitmapFactory.Options options,Rect region)throws IOException{
  int[] size=bounds(primary);
  if(options!=null&&options.inJustDecodeBounds){options.outWidth=size[0];options.outHeight=size[1];options.outMimeType="image/x-hunex";return null;}
  Rect crop=region==null?new Rect(0,0,size[0],size[1]):region;
  return decode(primary,patch,Math.max(1,options==null?1:options.inSampleSize),crop.left,crop.top,crop.right,crop.bottom,options==null||options.inPremultiplied);
 }
 private NativeAssetCodec(){}
}
