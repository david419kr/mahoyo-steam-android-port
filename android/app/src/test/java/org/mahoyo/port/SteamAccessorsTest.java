package org.mahoyo.port;

import org.junit.Test;
import static org.junit.Assert.*;
import java.io.IOException;
import java.nio.*;
import java.security.MessageDigest;
import java.util.*;

/** Authored instruction fixtures only, never bytes from the original game. */
public final class SteamAccessorsTest {
    private SteamPe image(byte... code)throws Exception{
        byte[] bytes=new byte[0xa6000];ByteBuffer b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        b.putShort(0,(short)0x5a4d);b.putInt(0x3c,0x80);b.putInt(0x80,0x4550);b.putShort(0x84,(short)0x8664);b.putShort(0x86,(short)1);b.putShort(0x94,(short)240);b.putShort(0x98,(short)0x20b);
        int section=0x98+240;b.putInt(section+12,0x1000);b.putInt(section+16,bytes.length-0x200);b.putInt(section+20,0x200);System.arraycopy(code,0,bytes,0xa44d0-0x1000+0x200,code.length);
        StringBuilder sha=new StringBuilder();for(byte value:MessageDigest.getInstance("SHA-256").digest(bytes))sha.append(String.format(Locale.ROOT,"%02x",value&255));return new SteamPe(bytes,sha.toString());
    }
    @Test public void acceptsOnlyDefinedBoundedIntegerComputation()throws Exception{
        SteamAccessors m=new SteamAccessors(image((byte)0x0f,(byte)0xb7,(byte)0xc1,(byte)0x83,(byte)0xc0,(byte)0x01,(byte)0xc3));
        assertEquals(2,m.loop(1));assertEquals(8373,m.loop(8372));
        try{m.loop(0);fail();}catch(IOException expected){}
        try{m.loop(8373);fail();}catch(IOException expected){}
    }
    @Test public void rejectsUndefinedRegistersWritesAndControlFlowEscape()throws Exception{
        byte[][] rejected={{(byte)0x89,(byte)0xd0,(byte)0xc3},{(byte)0x89,(byte)0x01,(byte)0xc3},{(byte)0xeb,(byte)0x80},{(byte)0x0f,(byte)0x05}};
        for(byte[] code:rejected)try{new SteamAccessors(image(code)).loop(1);fail();}catch(IOException expected){}
    }
    @Test public void refusesUnsupportedEncryptedHeader()throws Exception{
        try{image((byte)0xc3).decodeMetadataCode();fail();}catch(IOException expected){}
    }
}
