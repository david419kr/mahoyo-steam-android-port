package org.mahoyo.port;

import android.graphics.*;
import android.media.MediaExtractor;
import android.media.MediaPlayer;
import java.io.*;

/** One immutable file range, with an independent descriptor for each reader. */
final class GameAsset {
    final File container;final long offset,length;final boolean ranged;
    private GameAsset patch;
    private GameAsset(File file,long offset,long length,boolean ranged){container=file;this.offset=offset;this.length=length;this.ranged=ranged;}
    static GameAsset loose(File file){return new GameAsset(file,0,file.length(),false);}
    static GameAsset range(File file,long offset,long length){return new GameAsset(file,offset,length,true);}
    static GameAsset composite(GameAsset primary,GameAsset patch){GameAsset value=new GameAsset(primary.container,primary.offset,primary.length,primary.ranged);value.patch=patch;return value;}
    boolean isFile(){return container.isFile()&&(!ranged||AssetBounds.range(offset,length,container.length()));}
    String key()throws IOException{return container.getCanonicalPath()+(ranged?"@"+offset+":"+length:"")+(patch==null?"":"+"+patch.key());}
    InputStream open()throws IOException{
        FileInputStream input=new FileInputStream(container);
        try{
            if(!ranged)return input;
            if(!AssetBounds.range(offset,length,input.getChannel().size()))throw new IOException("HFA resource outside file");
            input.getChannel().position(offset);
            return new FilterInputStream(input){
                long remaining=length;
                @Override public int read()throws IOException{if(remaining==0)return -1;int value=in.read();if(value<0)throw new EOFException("Truncated HFA resource");remaining--;return value;}
                @Override public int read(byte[] b,int off,int len)throws IOException{if(off<0||len<0||off>b.length-len)throw new IndexOutOfBoundsException();if(len==0)return 0;if(remaining==0)return -1;int n=in.read(b,off,(int)Math.min(len,remaining));if(n<0)throw new EOFException("Truncated HFA resource");remaining-=n;return n;}
                @Override public long skip(long n)throws IOException{long count=Math.min(Math.max(0,n),remaining);input.getChannel().position(input.getChannel().position()+count);remaining-=count;return count;}
                @Override public int available(){return (int)Math.min(Integer.MAX_VALUE,remaining);}
            };
        }catch(Throwable error){input.close();throw error;}
    }
    Bitmap bitmap(BitmapFactory.Options options)throws IOException{
        if(nativeImage()){
            if(options!=null&&options.inJustDecodeBounds){int[] size=HfaImageInfo.read(new HfaIndex.Entry(container,"",offset,length));options.outWidth=size[0];options.outHeight=size[1];options.outMimeType="image/x-hunex";return null;}
            return NativeAssetCodec.bitmap(encoded(256*1024*1024),patch==null?null:patch.encoded(256*1024*1024),options,null);
        }
        if(!ranged)return BitmapFactory.decodeFile(container.getPath(),options);
        try(InputStream input=new BufferedInputStream(open(),65536)){return BitmapFactory.decodeStream(input,null,options);}
    }
    Bitmap region(Rect rect,BitmapFactory.Options options)throws IOException{
        if(nativeImage())return NativeAssetCodec.bitmap(encoded(256*1024*1024),patch==null?null:patch.encoded(256*1024*1024),options,rect);
        if(!ranged){BitmapRegionDecoder reader=BitmapRegionDecoder.newInstance(container.getPath(),false);try{return reader.decodeRegion(rect,options);}finally{reader.recycle();}}
        try(InputStream input=new BufferedInputStream(open(),65536)){BitmapRegionDecoder reader=BitmapRegionDecoder.newInstance(input,false);try{return reader.decodeRegion(rect,options);}finally{reader.recycle();}}
    }
    private boolean nativeImage()throws IOException{try(InputStream in=open()){byte[] header=new byte[16];int at=0,n;while(at<header.length&&(n=in.read(header,at,header.length-at))!=-1)at+=n;return NativeAssetCodec.image(header);}}
    byte[] encoded(int cap)throws IOException{if(length<0||length>cap)throw new IOException("Encoded asset budget");try(InputStream in=open();ByteArrayOutputStream out=new ByteArrayOutputStream((int)Math.min(length,65536))){byte[] block=new byte[65536];int n;while((n=in.read(block))!=-1){if(out.size()>cap-n)throw new IOException("Encoded asset budget");out.write(block,0,n);}return out.toByteArray();}}
    void source(MediaPlayer player)throws IOException{if(!ranged){player.setDataSource(container.getPath());return;}try(FileInputStream in=new FileInputStream(container)){if(!AssetBounds.range(offset,length,in.getChannel().size()))throw new IOException("HFA media range");player.setDataSource(in.getFD(),offset,length);}}
    void source(MediaExtractor extractor)throws IOException{if(!ranged){extractor.setDataSource(container.getPath());return;}try(FileInputStream in=new FileInputStream(container)){if(!AssetBounds.range(offset,length,in.getChannel().size()))throw new IOException("HFA audio range");extractor.setDataSource(in.getFD(),offset,length);}}
}
