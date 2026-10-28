package org.mahoyo.port;

import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import org.junit.Test;
import static org.junit.Assert.*;

public class SaveCommonTest {
    static byte[] shared(long generation,int[] variables,byte[] flags)throws Exception {ByteBuffer b=ByteBuffer.allocate(26+variables.length*4+flags.length).order(ByteOrder.LITTLE_ENDIAN);b.put("WOHSHARED1".getBytes(StandardCharsets.US_ASCII)).putLong(generation).putInt(variables.length).putInt(flags.length);for(int n:variables)b.putInt(n);b.put(flags);return SaveCommon.container(b.array());}
    static byte[] text(String value)throws Exception {return SaveCommon.container(value.getBytes(StandardCharsets.UTF_8));}
    @Test public void mergesFlagsByUnionAndPreservesExistingScalarSettings()throws Exception {
        byte[] a=shared(4,new int[]{1,9,-3},new byte[]{1,0,0}),b=shared(6,new int[]{7,2,-1,42},new byte[]{0,1,0,1});
        SaveCommon.Shared merged=new SaveCommon.Shared(SaveCommon.payload(SaveCommon.merge("shared.whp",a,b)));
        assertArrayEquals(new int[]{1,9,-3,42},merged.variables);assertArrayEquals(new byte[]{1,1,0,1},merged.flags);assertEquals(7,merged.generation);
    }
    @Test public void nativeUnlockMaskIsAUnionAndNavigationStaysAPair()throws Exception {
        int[] a=new int[27],b=new int[27];a[26]=1<<5;b[26]=1<<8;a[8]=3;a[9]=100;b[8]=4;b[9]=2;
        SaveCommon.Shared merged=new SaveCommon.Shared(SaveCommon.payload(SaveCommon.merge("shared.whp",shared(3,a,new byte[]{1}),shared(4,b,new byte[]{1}))));assertEquals((1<<5)|(1<<8),merged.variables[26]);assertEquals(3,merged.variables[8]);assertEquals(100,merged.variables[9]);
    }
    @Test public void missingCurrentPreservesSignedValues()throws Exception {byte[] a=shared(3,new int[]{Integer.MIN_VALUE,-1,0,Integer.MAX_VALUE},new byte[]{0,1});assertArrayEquals(new int[]{Integer.MIN_VALUE,-1,0,Integer.MAX_VALUE},new SaveCommon.Shared(SaveCommon.payload(SaveCommon.merge("shared.whp",null,a))).variables);}
    @Test public void zeroIsAlsoAValidExistingSetting()throws Exception {byte[] a=shared(3,new int[]{0,5},new byte[]{1}),b=shared(4,new int[]{99,10},new byte[]{0});assertArrayEquals(new int[]{0,5},new SaveCommon.Shared(SaveCommon.payload(SaveCommon.merge("shared.whp",a,b))).variables);}
    @Test public void emptyTitleRegistryRemainsEligibleForLegacySlotMigration()throws Exception {byte[] flags=new byte[322];flags[321]=1;byte[] a=shared(1,new int[]{0,0},flags);SaveCommon.Shared merged=new SaveCommon.Shared(SaveCommon.payload(SaveCommon.merge("shared.whp",a,a)));assertEquals(1,merged.generation);assertEquals(1,merged.flags[321]);}
    @Test public void imageAndTrophyRegistriesAreDeduplicatedUnions()throws Exception {
        String images=new String(SaveCommon.payload(SaveCommon.merge("seen-images.whp",text("WOHSEEN1\n\"img_a.mzp\"\n"),text("WOHSEEN1\n\"img_b.mzp\"\n\"img_a.mzp\"\n"))),StandardCharsets.UTF_8);assertEquals("WOHSEEN1\n\"img_a.mzp\"\n\"img_b.mzp\"\n",images);
        String trophies=new String(SaveCommon.payload(SaveCommon.merge("trophies.whp",text("WOHTROPHY1 2\n1\n3\n"),text("WOHTROPHY1 2\n2\n3\n"))),StandardCharsets.UTF_8);assertEquals("WOHTROPHY1 3\n1\n2\n3\n",trophies);
    }
    @Test public void rejectsCorruptionAndUnexpectedContainerFields()throws Exception {
        byte[] a=text("WOHTROPHY1 0\n");a[a.length-1]^=1;try{SaveCommon.validate("trophies.whp",a);fail();}catch(IOException expected){}
        for(String value:new String[]{"WOHTROPHY1 2\n1\n1\n","WOHTROPHY1 1\n27\n","WOHTROPHY1 0\ntrailing"})try{SaveCommon.validate("trophies.whp",text(value));fail();}catch(IOException expected){}
        for(String value:new String[]{"WOHSEEN1\n\"../secret\"\n","WOHSEEN1\n\"unterminated","WOHSEEN1\nnotquoted"})try{SaveCommon.validate("seen-images.whp",text(value));fail();}catch(IOException expected){}
        try{SaveCommon.validate("shared.whp",shared(1,new int[]{0},new byte[]{2}));fail();}catch(IOException expected){}
    }
}
