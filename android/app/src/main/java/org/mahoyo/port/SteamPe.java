package org.mahoyo.port;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;

/** Bounded data-only reader for the user's pinned Steam executable. Never loads or runs it. */
final class SteamPe {
    private final byte[] data;private final int sections,count,resources;
    SteamPe(File file,String expectedSha)throws Exception{this(bytes(file),expectedSha);}
    private static byte[] bytes(File file)throws IOException{long length=file.length();if(length<128||length>64*1024*1024)throw new IOException("Steam executable size budget");return Files.readAllBytes(file.toPath());}
    SteamPe(byte[] bytes,String expectedSha)throws Exception{
        if(bytes.length<128||bytes.length>64*1024*1024)throw new IOException("Steam executable size budget");data=bytes.clone();
        StringBuilder hash=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(data))hash.append(String.format(Locale.ROOT,"%02x",b&255));
        if(!hash.toString().equals(expectedSha))throw new IOException("Unsupported Steam executable");
        if(u16(0)!=0x5a4d)throw invalid();int pe=number(u32(0x3c));if(u32(pe)!=0x4550||u16(pe+4)!=0x8664)throw invalid();
        count=u16(pe+6);int optional=u16(pe+20);if(count<1||count>96||optional<136||u16(pe+24)!=0x20b)throw invalid();
        sections=pe+24+optional;range(sections,count*40);resources=number(u32(pe+24+112+16));
    }
    private static IOException invalid(){return new IOException("Invalid Steam executable metadata");}
    private void range(int at,int length)throws IOException{if(at<0||length<0||at>data.length||length>data.length-at)throw invalid();}
    private int u16(int at)throws IOException{range(at,2);return (data[at]&255)|((data[at+1]&255)<<8);}
    private long u32(int at)throws IOException{range(at,4);return ((long)data[at]&255)|((long)(data[at+1]&255)<<8)|((long)(data[at+2]&255)<<16)|((long)(data[at+3]&255)<<24);}
    private static int number(long value)throws IOException{if(value<0||value>Integer.MAX_VALUE)throw invalid();return (int)value;}
    byte[] rva(int address,int length)throws IOException{
        for(int i=0;i<count;i++){int at=sections+i*40;long base=u32(at+12),size=u32(at+16),offset=u32(at+20),delta=(long)address-base;
            if(delta>=0&&delta<size&&length>=0&&length<=size-delta){int start=number(offset+delta);range(start,length);return Arrays.copyOfRange(data,start,start+length);}}
        throw invalid();
    }
    private long resource32(int relative)throws IOException{byte[] value=rva(resources+relative,4);return ((long)value[0]&255)|((long)(value[1]&255)<<8)|((long)(value[2]&255)<<16)|((long)(value[3]&255)<<24);}
    private int child(int directory,String key,boolean first)throws IOException{
        byte[] header=rva(resources+directory,16);int n=(header[12]&255)|((header[13]&255)<<8);n+=(header[14]&255)|((header[15]&255)<<8);if(n<1||n>4096)throw invalid();
        for(int i=0;i<n;i++){int at=directory+16+i*8;long id=resource32(at),target=resource32(at+4);String name;
            if((id&0x80000000L)!=0){int where=number(id&0x7fffffffL);byte[] size=rva(resources+where,2);int chars=(size[0]&255)|((size[1]&255)<<8);if(chars>256)throw invalid();name=new String(rva(resources+where+2,chars*2),StandardCharsets.UTF_16LE);}else name=Long.toString(id);
            if(first||name.equals(key))return (int)target;
        }throw invalid();
    }
    private int directory(int target)throws IOException{if((target&0x80000000)==0)throw invalid();return target&0x7fffffff;}
    byte[] resource(String type,int id)throws IOException{
        int typeDir=directory(child(0,type,false)),nameDir=directory(child(typeDir,Integer.toString(id),false)),leaf=child(nameDir,"",true);
        if((leaf&0x80000000)!=0)throw invalid();long address=resource32(leaf),length=resource32(leaf+4);if(length<1||length>4*1024*1024)throw invalid();return rva(number(address),number(length));
    }
    byte[] palette(int mode)throws IOException{
        if(mode!=3&&(mode<8||mode>82))throw invalid();byte[] channels=rva(0x3b4040+mode*3*256,3*256),result=new byte[768];
        for(int i=0;i<256;i++)for(int c=0;c<3;c++)result[i*3+c]=channels[c*256+i];return result;
    }
}
