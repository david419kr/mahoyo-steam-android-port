package org.mahoyo.port;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Synthetic PE only; no original executable, artwork or text is included. */
public final class SteamPeTest {
    private byte[] image(){
        byte[] raw=new byte[0x11400];ByteBuffer b=ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN);
        b.putShort(0,(short)0x5a4d);b.putInt(0x3c,0x80);b.putInt(0x80,0x4550);b.putShort(0x84,(short)0x8664);b.putShort(0x86,(short)2);b.putShort(0x94,(short)240);b.putShort(0x98,(short)0x20b);b.putInt(0x98+128,0x700000);
        int s=0x98+240;b.putInt(s+12,0x3b4000);b.putInt(s+16,65536);b.putInt(s+20,0x400);b.putInt(s+52,0x700000);b.putInt(s+56,4096);b.putInt(s+60,0x10400);
        for(int c=0;c<3;c++)for(int i=0;i<256;i++)raw[0x400+64+3*768+c*256+i]=(byte)(c*80+i);
        int r=0x10400;b.putShort(r+12,(short)1);b.putInt(r+16,0x80000080);b.putInt(r+20,0x80000020);b.putShort(r+0x2e,(short)1);b.putInt(r+0x30,5);b.putInt(r+0x34,0x80000040);b.putShort(r+0x4e,(short)1);b.putInt(r+0x50,1041);b.putInt(r+0x54,0x60);b.putInt(r+0x60,0x700100);b.putInt(r+0x64,4);b.putShort(r+0x80,(short)4);
        for(int i=0;i<4;i++){b.putChar(r+0x82+i*2,"TEXT".charAt(i));raw[r+0x100+i]=(byte)"test".charAt(i);}return raw;
    }
    private String sha(byte[] bytes)throws Exception{StringBuilder s=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(bytes))s.append(String.format(Locale.ROOT,"%02x",b&255));return s.toString();}
    @Test public void readsOnlyCheckedFileBackedData()throws Exception{
        byte[] raw=image();
            SteamPe pe=new SteamPe(raw,sha(raw));assertArrayEquals("test".getBytes("UTF-8"),pe.resource("TEXT",5));
            byte[] palette=pe.palette(3);for(int i=0;i<256;i++)for(int c=0;c<3;c++)assertEquals((byte)(c*80+i),palette[i*3+c]);
            try{pe.rva(Integer.MAX_VALUE,10);fail();}catch(IOException expected){}
            try{pe.resource("TEXT",7);fail();}catch(IOException expected){}
            try{new SteamPe(raw,"wrong digest");fail();}catch(IOException expected){}
            ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN).putShort(0x86,(short)0);try{new SteamPe(raw,sha(raw));fail();}catch(IOException expected){}
    }
}
