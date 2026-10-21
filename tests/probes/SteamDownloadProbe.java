import java.nio.file.*;
import java.security.*;
import java.util.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import in.dragonbra.javasteam.util.VZipUtil;
import in.dragonbra.javasteam.util.stream.MemoryStream;

/** Synthetic shell-VM benchmark; no credentials, game files, or application restart. */
public final class SteamDownloadProbe {
    private static volatile int result;
    public static void main(String[] args)throws Exception{
        if(args.length==3&&args[0].equals("--generate")){
            byte[] input=Files.readAllBytes(Paths.get(args[1]));java.io.ByteArrayOutputStream output=new java.io.ByteArrayOutputStream();
            java.util.zip.CRC32 crc=new java.util.zip.CRC32();crc.update(input);
            java.nio.ByteBuffer header=java.nio.ByteBuffer.allocate(12).order(java.nio.ByteOrder.LITTLE_ENDIAN);
            header.put(new byte[]{'V','Z','a'}).putInt((int)crc.getValue()).put((byte)0x5d).putInt(1<<23);output.write(header.array());
            org.tukaani.xz.LZMA2Options options=new org.tukaani.xz.LZMA2Options(2);options.setDictSize(1<<23);
            try(org.tukaani.xz.LZMAOutputStream encoder=new org.tukaani.xz.LZMAOutputStream(output,options,false)){encoder.write(input);}
            java.nio.ByteBuffer footer=java.nio.ByteBuffer.allocate(10).order(java.nio.ByteOrder.LITTLE_ENDIAN);footer.putInt((int)crc.getValue()).putInt(input.length).put((byte)'z').put((byte)'v');output.write(footer.array());
            Files.write(Paths.get(args[2]),output.toByteArray());return;
        }
        Security.removeProvider("BC");Security.addProvider(new BouncyCastleProvider());
        byte[] plain=new byte[1024*1024];new Random(17).nextBytes(plain);
        SecretKeySpec key=new SecretKeySpec(new byte[32],"AES");IvParameterSpec iv=new IvParameterSpec(new byte[16]);
        Cipher encoder=Cipher.getInstance("AES/CBC/PKCS5Padding","AndroidOpenSSL");encoder.init(Cipher.ENCRYPT_MODE,key,iv);byte[] encrypted=encoder.doFinal(plain);
        for(String provider:new String[]{"BC","AndroidOpenSSL"}){
            String padding=provider.equals("BC")?"PKCS7Padding":"PKCS5Padding";
            Cipher decoder=Cipher.getInstance("AES/CBC/"+padding,provider);byte[] output=new byte[encrypted.length];
            for(int i=0;i<4;i++){decoder.init(Cipher.DECRYPT_MODE,key,iv);decoder.doFinal(encrypted,0,encrypted.length,output);}
            long begin=System.nanoTime();int count=24;
            for(int i=0;i<count;i++){decoder.init(Cipher.DECRYPT_MODE,key,iv);int written=decoder.doFinal(encrypted,0,encrypted.length,output);if(written!=plain.length)throw new AssertionError();result^=output[0];}
            if(!Arrays.equals(plain,Arrays.copyOf(output,plain.length)))throw new AssertionError("AES plaintext differs");
            System.out.printf(Locale.ROOT,"AES %s: %.1f MB/s%n",provider,count*plain.length/((System.nanoTime()-begin)/1e9)/1e6);
        }
        for(boolean nativeChecksum:new boolean[]{false,true}){
            long begin=System.nanoTime();int count=48;
            for(int i=0;i<count;i++){
                if(nativeChecksum){java.util.zip.Adler32 crc=new java.util.zip.Adler32();crc.update(plain);long value=crc.getValue();long a=((value&65535)+65520)%65521,b=((value>>>16)+65521-plain.length%65521)%65521;result^=(int)(b<<16|a);}
                else result^=in.dragonbra.javasteam.util.Adler32.calculate(plain);
            }
            System.out.printf(Locale.ROOT,"Adler %s: %.1f MB/s%n",nativeChecksum?"native":"SDK",count*plain.length/((System.nanoTime()-begin)/1e9)/1e6);
        }
        if(args.length==2){
            byte[] compressed=Files.readAllBytes(Paths.get(args[0])),expected=Files.readAllBytes(Paths.get(args[1])),output=new byte[expected.length];int count=16;
            for(int i=0;i<3;i++)try(MemoryStream stream=new MemoryStream(compressed)){VZipUtil.decompress(stream,output,true);}
            long begin=System.nanoTime();
            for(int i=0;i<count;i++)try(MemoryStream stream=new MemoryStream(compressed)){VZipUtil.decompress(stream,output,true);}
            if(!Arrays.equals(expected,output))throw new AssertionError("LZMA output differs");
            System.out.printf(Locale.ROOT,"SDK LZMA: %.1f MB/s output, %.1f MB/s compressed%n",count*output.length/((System.nanoTime()-begin)/1e9)/1e6,count*compressed.length/((System.nanoTime()-begin)/1e9)/1e6);
        }
    }
}
