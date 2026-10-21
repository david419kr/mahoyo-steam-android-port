package org.mahoyo.port;

import android.graphics.*;
import android.media.MediaExtractor;
import android.media.MediaPlayer;
import java.io.*;

/** One immutable file range, with an independent descriptor for each reader. */
final class GameAsset {
    final File container;final long offset,length;final boolean packed;
    private GameAsset(File file,long offset,long length,boolean packed){container=file;this.offset=offset;this.length=length;this.packed=packed;}
    static GameAsset loose(File file){return new GameAsset(file,0,file.length(),false);}
    static GameAsset packed(File file,ObbIndex.Entry entry){return new GameAsset(file,entry.offset,entry.length,true);}
    boolean isFile(){return container.isFile()&&(!packed||ObbIndex.range(offset,length,container.length()));}
    String key()throws IOException{return container.getCanonicalPath()+(packed?"@"+offset+":"+length:"");}
    InputStream open()throws IOException{
        FileInputStream input=new FileInputStream(container);
        try{
            if(!packed)return input;
            if(!ObbIndex.range(offset,length,input.getChannel().size()))throw new IOException("OBB resource outside file");
            input.getChannel().position(offset);
            return new FilterInputStream(input){
                long remaining=length;
                @Override public int read()throws IOException{if(remaining==0)return -1;int value=in.read();if(value<0)throw new EOFException("Truncated OBB resource");remaining--;return value;}
                @Override public int read(byte[] b,int off,int len)throws IOException{if(off<0||len<0||off>b.length-len)throw new IndexOutOfBoundsException();if(len==0)return 0;if(remaining==0)return -1;int n=in.read(b,off,(int)Math.min(len,remaining));if(n<0)throw new EOFException("Truncated OBB resource");remaining-=n;return n;}
                @Override public long skip(long n)throws IOException{long count=Math.min(Math.max(0,n),remaining);input.getChannel().position(input.getChannel().position()+count);remaining-=count;return count;}
                @Override public int available(){return (int)Math.min(Integer.MAX_VALUE,remaining);}
            };
        }catch(Throwable error){input.close();throw error;}
    }
    Bitmap bitmap(BitmapFactory.Options options)throws IOException{
        if(avif())return AvifCodec.bitmap(encodedAvif(),options);
        if(!packed)return BitmapFactory.decodeFile(container.getPath(),options);
        try(InputStream input=new BufferedInputStream(open(),65536)){return BitmapFactory.decodeStream(input,null,options);}
    }
    Bitmap region(Rect rect,BitmapFactory.Options options)throws IOException{
        if(avif())return AvifCodec.region(encodedAvif(),rect,options);
        if(!packed){BitmapRegionDecoder reader=BitmapRegionDecoder.newInstance(container.getPath(),false);try{return reader.decodeRegion(rect,options);}finally{reader.recycle();}}
        try(InputStream input=new BufferedInputStream(open(),65536)){BitmapRegionDecoder reader=BitmapRegionDecoder.newInstance(input,false);try{return reader.decodeRegion(rect,options);}finally{reader.recycle();}}
    }
    private boolean avif()throws IOException{
        try(InputStream in=open()){byte[] p=new byte[12];int at=0,n;while(at<p.length&&(n=in.read(p,at,p.length-at))!=-1)at+=n;
            return at==12&&p[4]=='f'&&p[5]=='t'&&p[6]=='y'&&p[7]=='p'&&p[8]=='a'&&p[9]=='v'&&p[10]=='i'&&(p[11]=='f'||p[11]=='s');}
    }
    private byte[] encodedAvif()throws IOException{
        final int cap=64*1024*1024;if(length<0||length>cap)throw new IOException("AVIF encoded image budget");
        byte[] encoded;try(InputStream in=open();ByteArrayOutputStream out=new ByteArrayOutputStream((int)Math.min(length,65536))){byte[] block=new byte[65536];int n;while((n=in.read(block))!=-1){if(out.size()>cap-n)throw new IOException("AVIF encoded image budget");out.write(block,0,n);}encoded=out.toByteArray();}
        return encoded;
    }
    void source(MediaPlayer player)throws IOException{if(!packed){player.setDataSource(container.getPath());return;}try(FileInputStream in=new FileInputStream(container)){if(!ObbIndex.range(offset,length,in.getChannel().size()))throw new IOException("OBB media range");player.setDataSource(in.getFD(),offset,length);}}
    void source(MediaExtractor extractor)throws IOException{if(!packed){extractor.setDataSource(container.getPath());return;}try(FileInputStream in=new FileInputStream(container)){if(!ObbIndex.range(offset,length,in.getChannel().size()))throw new IOException("OBB audio range");extractor.setDataSource(in.getFD(),offset,length);}}
}
