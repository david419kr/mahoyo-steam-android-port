package org.mahoyo.port;
import java.io.*;
/** Reads only the original image descriptor, not all compressed image bytes. */
final class HfaImageInfo {
 static int word(byte[] p,int at){return (p[at]&255)|((p[at+1]&255)<<8);}
 static int[] read(HfaIndex.Entry entry)throws IOException{
  byte[] header=entry.prefix(16);int width,height;
  if(header[0]=='C'&&header[1]=='o'&&header[2]=='m'&&header[3]=='p'){
   header=entry.prefix(32);long w=HfaIndex.unsigned(header,16),h=HfaIndex.unsigned(header,20),stripe=HfaIndex.unsigned(header,24),depth=HfaIndex.unsigned(header,28);if(w>65535||h>65535||stripe==0||depth!=8&&depth!=24&&depth!=32)throw new IOException("Invalid CBG descriptor");width=(int)w;height=(int)h;
  }else if(header[0]=='m'&&header[1]=='r'&&header[2]=='g'&&header[3]=='d'&&header[4]=='0'&&header[5]=='0'){
   int count=word(header,6),sector=word(header,8),offset=word(header,10);if(count==0||offset>=2048)throw new IOException("Invalid MZP index");long start=8L+count*8L+sector*2048L+offset;byte[] image=entry.slice(start,16).read(16);int w=word(image,0),h=word(image,2),tw=word(image,4),th=word(image,6),nx=word(image,8),ny=word(image,10),border=image[15]&255;
   if(nx==0||ny==0||tw==0||th==0||border*2>=Math.min(tw,th)||1L+nx*(long)ny!=count)throw new IOException("Invalid MZP descriptor");width=w-2*border*(nx-1);height=h-2*border*(ny-1);
  }else throw new IOException("Unknown native image descriptor");
  if(width<=0||height<=0||width>65535||height>65535||(long)width*height>64L*1024*1024)throw new IOException("Native image size budget");return new int[]{width,height};
 }
 private HfaImageInfo(){}
}
