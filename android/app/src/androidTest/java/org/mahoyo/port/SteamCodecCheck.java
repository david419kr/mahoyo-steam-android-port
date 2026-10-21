package org.mahoyo.port;

import in.dragonbra.javasteam.steam.cdn.DepotChunk;
import in.dragonbra.javasteam.types.ChunkData;
import java.io.*;
import java.nio.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.zip.*;
import javax.crypto.*;
import javax.crypto.spec.*;

/** Synthetic depot chunks only: no login, game assets, network, or player data. */
final class SteamCodecCheck {
    private interface Attempt {void run()throws Exception;}
    private static void reject(Attempt action)throws Exception{try{action.run();}catch(IOException|IllegalArgumentException expected){return;}throw new AssertionError("Corrupt chunk accepted");}
    static String run()throws Exception{
        byte[] key=new byte[32];new Random(73).nextBytes(key);
        byte[] plain=new byte[1024*1024];new Random(17).nextBytes(plain);Arrays.fill(plain,0,plain.length/2,(byte)0);
        byte[] compressed=vzip(plain),encrypted=encrypt(compressed,key);
        ChunkData info=new ChunkData(new byte[20],adler(plain),0,encrypted.length,plain.length);
        byte[] baseline=new byte[plain.length+31],decoded=new byte[plain.length+31];
        DepotChunk.INSTANCE.setVzipDecoder(null);DepotChunk.process(info,encrypted,baseline,key);
        DepotChunk.INSTANCE.setVzipDecoder(SteamChunkCodec::decodeVzip);
        if(DepotChunk.process(info,encrypted,decoded,key)!=plain.length||!Arrays.equals(baseline,decoded)||!Arrays.equals(plain,Arrays.copyOf(decoded,plain.length)))throw new AssertionError("Native depot bytes differ");
        reject(()->DepotChunk.process(new ChunkData(new byte[20],info.getChecksum()^1,0,encrypted.length,plain.length),encrypted,decoded,key));
        byte[] badKey=key.clone();badKey[0]^=1;reject(()->DepotChunk.process(info,encrypted,decoded,badKey));
        for(int size:new int[]{0,1,31,65521}){
            byte[] small=Arrays.copyOf(plain,size),out=new byte[size+8];
            if(SteamChunkCodec.decodeVzip(vzip(small),vzip(small).length,out)!=size||!Arrays.equals(small,Arrays.copyOf(out,size)))throw new AssertionError("Native small chunk differs");
        }
        reject(()->SteamChunkCodec.decodeVzip(compressed,compressed.length+1,decoded));
        reject(()->SteamChunkCodec.decodeVzip(compressed,-1,decoded));
        reject(()->SteamChunkCodec.decodeVzip(compressed,compressed.length,new byte[plain.length-1]));
        byte[] invalid=compressed.clone();invalid[0]=0;reject(()->SteamChunkCodec.decodeVzip(invalid,invalid.length,decoded));
        invalid[0]='V';invalid[11]=(byte)0x7f;reject(()->SteamChunkCodec.decodeVzip(invalid,invalid.length,decoded));
        byte[] truncated=new byte[compressed.length-8];System.arraycopy(compressed,0,truncated,0,truncated.length-10);System.arraycopy(compressed,compressed.length-10,truncated,truncated.length-10,10);
        reject(()->SteamChunkCodec.decodeVzip(truncated,truncated.length,decoded));
        // Execute the same complete encrypted chunk path used by all four download workers.
        ExecutorService workers=Executors.newFixedThreadPool(4);long parallelStart=System.nanoTime(),parallelNanos;
        try{
            ArrayList<Future<?>> work=new ArrayList<>();for(int i=0;i<32;i++)work.add(workers.submit(()->{try{byte[] out=new byte[plain.length];DepotChunk.process(info,encrypted,out,key);if(!Arrays.equals(out,plain))throw new AssertionError("Concurrent output differs");}catch(Exception e){throw new RuntimeException(e);}}));
            for(Future<?> task:work)task.get(20,TimeUnit.SECONDS);
            parallelNanos=Math.max(1,System.nanoTime()-parallelStart);
        }finally{workers.shutdownNow();}
        for(int i=0;i<3;i++)DepotChunk.process(info,encrypted,decoded,key);
        DepotChunk.INSTANCE.setVzipDecoder(null);long before=bench(info,encrypted,key,8);
        DepotChunk.INSTANCE.setVzipDecoder(SteamChunkCodec::decodeVzip);long after=bench(info,encrypted,key,16);
        return String.format(Locale.ROOT,"Synthetic encrypted VZip parity, malformed input, checksum/wrong-key rejection and concurrent decoding passed. Full chunk path: Java LZMA %.1f MB/s; native LZMA %.1f MB/s (uncompressed). Four workers: %.1f MB/s output, %.1f MB/s encrypted input. No download or player data changed.\n",8.0*plain.length*1000/before,16.0*plain.length*1000/after,32.0*plain.length*1000/parallelNanos,32.0*encrypted.length*1000/parallelNanos);
    }
    private static long bench(ChunkData info,byte[] encrypted,byte[] key,int count)throws Exception{byte[] out=new byte[info.getUncompressedLength()];long start=System.nanoTime();for(int i=0;i<count;i++)DepotChunk.process(info,encrypted,out,key);return Math.max(1,System.nanoTime()-start);}
    private static int adler(byte[] data){return in.dragonbra.javasteam.util.Adler32.calculate(data);}
    private static byte[] encrypt(byte[] plain,byte[] key)throws Exception{
        byte[] iv=new byte[16];new Random(83).nextBytes(iv);SecretKeySpec spec=new SecretKeySpec(key,"AES");
        Cipher ecb=Cipher.getInstance("AES/ECB/NoPadding");ecb.init(Cipher.ENCRYPT_MODE,spec);
        Cipher cbc=Cipher.getInstance("AES/CBC/PKCS5Padding");cbc.init(Cipher.ENCRYPT_MODE,spec,new IvParameterSpec(iv));
        ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(ecb.doFinal(iv));out.write(cbc.doFinal(plain));return out.toByteArray();
    }
    private static byte[] vzip(byte[] plain)throws Exception{
        CRC32 crc=new CRC32();crc.update(plain);ByteArrayOutputStream out=new ByteArrayOutputStream();
        ByteBuffer header=ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN);header.put(new byte[]{'V','Z','a'}).putInt((int)crc.getValue()).put((byte)0x5d).putInt(1<<23);out.write(header.array());
        org.tukaani.xz.LZMA2Options options=new org.tukaani.xz.LZMA2Options(2);options.setDictSize(1<<23);
        try(org.tukaani.xz.LZMAOutputStream encoder=new org.tukaani.xz.LZMAOutputStream(out,options,false)){encoder.write(plain);}
        ByteBuffer footer=ByteBuffer.allocate(10).order(ByteOrder.LITTLE_ENDIAN);footer.putInt((int)crc.getValue()).putInt(plain.length).put((byte)'z').put((byte)'v');out.write(footer.array());return out.toByteArray();
    }
}
