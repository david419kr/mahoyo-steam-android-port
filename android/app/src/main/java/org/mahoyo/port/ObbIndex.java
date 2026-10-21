package org.mahoyo.port;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** Shared range contract: unsigned decimal offsets are kept as checked signed 64-bit values. */
final class ObbIndex {
    private static final Set<String> FOLDERS=new HashSet<>(Arrays.asList("audio","fonts","images","meta","scripts","text","video"));
    static final class Entry {
        final String path; final long offset,length,crc;
        Entry(String path,long offset,long length,long crc){this.path=path;this.offset=offset;this.length=length;this.crc=crc;}
    }
    static boolean safe(String path){
        if(path.isEmpty()||path.startsWith("/")||path.indexOf('\\')>=0||path.indexOf(':')>=0)return false;
        for(int i=0;i<path.length();i++)if(path.charAt(i)<32)return false;
        for(String part:path.split("/",-1))if(part.isEmpty()||part.equals(".")||part.equals(".."))return false;
        int slash=path.indexOf('/');return path.equals("assets.json")||slash>0&&FOLDERS.contains(path.substring(0,slash));
    }
    static long decimal(String value)throws IOException{if(value.isEmpty()||value.length()>1&&value.charAt(0)=='0')throw new IOException("Invalid OBB integer");for(int i=0;i<value.length();i++)if(value.charAt(i)<'0'||value.charAt(i)>'9')throw new IOException("Invalid OBB integer");try{return Long.parseLong(value);}catch(NumberFormatException e){throw new IOException("OBB integer overflow",e);}}
    private static boolean crc(String text){if(text.length()!=8)return false;for(int i=0;i<8;i++){char c=text.charAt(i);if(!(c>='0'&&c<='9'||c>='a'&&c<='f'))return false;}return true;}
    static boolean range(long offset,long length,long total){return offset>=0&&length>=0&&offset<=total&&length<=total-offset;}
    static LinkedHashMap<String,Entry> parse(byte[] bytes,long total)throws IOException{
        if(bytes.length>16*1024*1024||total<=0)throw new IOException("OBB index budget");
        LinkedHashMap<String,Entry> result=new LinkedHashMap<>();
        java.nio.charset.CharsetDecoder decoder=StandardCharsets.UTF_8.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT);
        try(BufferedReader reader=new BufferedReader(new InputStreamReader(new ByteArrayInputStream(bytes),decoder))){
            if(!"WOHOBB1".equals(reader.readLine()))throw new IOException("Unknown OBB format");
            long end=0;String line;
            while((line=reader.readLine())!=null){
                String[] row=line.split("\t",-1);if(row.length!=4||!safe(row[0])||!crc(row[3]))throw new IOException("Invalid OBB entry");
                long offset=decimal(row[1]),length=decimal(row[2]);if(!range(offset,length,total)||offset<end||result.size()>=100000)throw new IOException("OBB range/overlap");
                if(result.put(row[0],new Entry(row[0],offset,length,Long.parseLong(row[3],16)))!=null)throw new IOException("Duplicate OBB entry");end=offset+length;
            }
        }
        if(!result.containsKey("assets.json"))throw new IOException("OBB manifest missing");return result;
    }
}
