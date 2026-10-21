package org.mahoyo.port;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.zip.*;

/** Read-only WOHSAVE2 container metadata. VM and versioned hostExtra stay opaque. */
final class SlotSaveMetadata {
    final long modified,size;final String sha256,text;
    SlotSaveMetadata(long modified,long size,String sha256,String text){this.modified=modified;this.size=size;this.sha256=sha256;this.text=text;}
    static SlotSaveMetadata read(File file)throws IOException {
        long size=file.length(),modified=file.lastModified();
        if(size<16||size>64L*1024*1024)throw new IOException("Invalid save size");
        try {
            MessageDigest digest=MessageDigest.getInstance("SHA-256");
            try(DigestInputStream raw=new DigestInputStream(new BufferedInputStream(new FileInputStream(file)),digest)){
                byte[] header=new byte[16];readFully(raw,header);
                if(!new String(header,0,8,StandardCharsets.US_ASCII).equals("WOHSAVE2")||u32(header,8)!=size-16)throw new IOException("Invalid save header");
                CRC32 crc=new CRC32();Reader body=new Reader(new CheckedInputStream(raw,crc),size-16);
                body.blob(32*1024*1024,false);
                for(int n=body.count(128);n>0;n--){body.skip(8);body.blob(1024*1024,false);body.blob(1024*1024,false);body.blob(1024*1024,false);body.skip(40+16+4);}
                for(int n=body.count(128);n>0;n--){body.blob(1024*1024,false);body.blob(1024*1024,false);body.skip(4+8);}
                for(int n=body.count(512);n>0;n--)body.blob(1024*1024,false);
                String text=new String(body.blob(1024*1024,true),StandardCharsets.UTF_8);
                body.skip(8);body.blob(1024*1024,false);
                if(body.remaining!=0||raw.read()!=-1||crc.getValue()!=u32(header,12))throw new IOException("Invalid save checksum or trailing data");
                if(file.length()!=size||file.lastModified()!=modified)throw new IOException("Save changed during read");
                return new SlotSaveMetadata(modified,size,hex(digest.digest()),preview(text));
            }
        }catch(NoSuchAlgorithmException impossible){throw new IOException(impossible);}
    }
    static String preview(String value){
        if(value.length()>4000)value=value.substring(0,4000);
        value=value.replaceAll("<([^<>|]+)\\|[^<>]*>","$1").replaceAll("(?i)</?i[^>]*>|(?i)<fr[^>]*>","").replaceAll("@[oirx]","").replaceAll("\\s+"," ").trim();
        int count=value.codePointCount(0,value.length());return count>140?value.substring(0,value.offsetByCodePoints(0,140))+"…":value;
    }
    static String hex(byte[] bytes){StringBuilder out=new StringBuilder(bytes.length*2);for(byte b:bytes)out.append(Character.forDigit((b>>>4)&15,16)).append(Character.forDigit(b&15,16));return out.toString();}
    private static long u32(byte[] bytes,int at){return (bytes[at]&255L)|((bytes[at+1]&255L)<<8)|((bytes[at+2]&255L)<<16)|((bytes[at+3]&255L)<<24);}
    private static void readFully(InputStream stream,byte[] bytes)throws IOException {int at=0;while(at<bytes.length){int n=stream.read(bytes,at,bytes.length-at);if(n<0)throw new EOFException();at+=n;}}
    private static final class Reader {
        final InputStream input;long remaining;final byte[] scratch=new byte[8192];
        Reader(InputStream input,long size){this.input=input;remaining=size;}
        long number()throws IOException {byte[] value=new byte[4];bytes(value);return u32(value,0);}
        int count(int limit)throws IOException {long n=number();if(n>limit)throw new IOException("Invalid save count");return (int)n;}
        byte[] blob(int limit,boolean keep)throws IOException {long n=number();if(n>limit)throw new IOException("Invalid save field");if(!keep){skip(n);return new byte[0];}byte[] value=new byte[(int)n];bytes(value);return value;}
        void bytes(byte[] value)throws IOException {if(value.length>remaining)throw new EOFException();readFully(input,value);remaining-=value.length;}
        void skip(long count)throws IOException {if(count<0||count>remaining)throw new EOFException();remaining-=count;while(count>0){int n=input.read(scratch,0,(int)Math.min(count,scratch.length));if(n<0)throw new EOFException();count-=n;}}
    }
}
