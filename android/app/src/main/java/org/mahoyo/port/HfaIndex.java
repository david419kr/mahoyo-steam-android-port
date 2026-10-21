package org.mahoyo.port;
import java.io.*;
import java.nio.*;
import java.nio.charset.*;
import java.util.*;
/** Bounded original archive index; later entries/archives override earlier ones. */
final class HfaIndex {
 static final class Entry {
  final File file;final String name;final long offset,length;
  Entry(File file,String name,long offset,long length){this.file=file;this.name=name;this.offset=offset;this.length=length;}
  Entry slice(long start,long size)throws IOException{if(!AssetBounds.range(start,size,length))throw new IOException("HFA subrange outside member");return new Entry(file,name,offset+start,size);}
  byte[] prefix(int length)throws IOException{if(length<0||length>this.length)throw new IOException("Truncated HFA header");try(RandomAccessFile in=new RandomAccessFile(file,"r")){if(!AssetBounds.range(offset,this.length,in.length()))throw new IOException("HFA resource outside file");byte[] data=new byte[length];in.seek(offset);in.readFully(data);return data;}}
  byte[] read(int cap)throws IOException{if(length>cap||length>Integer.MAX_VALUE)throw new IOException("HFA member budget");try(RandomAccessFile in=new RandomAccessFile(file,"r")){if(!AssetBounds.range(offset,length,in.length()))throw new IOException("HFA resource outside file");byte[] data=new byte[(int)length];in.seek(offset);in.readFully(data);return data;}}
 }
 static long unsigned(byte[] raw,int offset){return Integer.toUnsignedLong(ByteBuffer.wrap(raw,offset,4).order(ByteOrder.LITTLE_ENDIAN).getInt());}
 static String name(byte[] row)throws IOException{
  int end=0;while(end<96&&row[end]!=0)end++;if(end==0||end==96)throw new IOException("Invalid HFA filename");
  String name;try{name=StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(row,0,end)).toString().toLowerCase(Locale.ROOT);}catch(CharacterCodingException e){throw new IOException("Invalid HFA filename encoding",e);}
  if(name.equals(".")||name.equals("..")||name.indexOf('/')>=0||name.indexOf('\\')>=0||name.indexOf(':')>=0)throw new IOException("Unsafe HFA filename");for(int i=0;i<name.length();i++)if(name.charAt(i)<32)throw new IOException("Unsafe HFA filename");return name;
 }
 static LinkedHashMap<String,Entry> read(File file)throws IOException{
  LinkedHashMap<String,Entry> entries=new LinkedHashMap<>();List<Entry> ranges=new ArrayList<>();
  try(RandomAccessFile in=new RandomAccessFile(file,"r")){
   byte[] header=new byte[16];in.readFully(header);if(!Arrays.equals(Arrays.copyOf(header,12),"HUNEXGGEFA10".getBytes(StandardCharsets.US_ASCII)))throw new IOException("Not a HFA archive");
   long count=unsigned(header,12),start=16+count*128;if(count*128>64*1024*1024||start>in.length())throw new IOException("HFA index budget");
   byte[] row=new byte[128];for(long i=0;i<count;i++){in.readFully(row);String name=name(row);long offset=start+unsigned(row,96),length=unsigned(row,100);if(!AssetBounds.range(offset,length,in.length()))throw new IOException("HFA entry outside file");Entry e=new Entry(file,name,offset,length);entries.put(name,e);if(length>0)ranges.add(e);}
   ranges.sort(Comparator.comparingLong(e->e.offset));long end=start;for(Entry e:ranges){if(e.offset<end)throw new IOException("Overlapping HFA entries");end=e.offset+e.length;}
  }return entries;
 }
 static LinkedHashMap<String,Entry> directory(File root)throws IOException{
  File[] files=root.listFiles(f->f.isFile()&&f.getName().toLowerCase(Locale.ROOT).endsWith(".hfa"));if(files==null||files.length==0)throw new IOException("Steam archives missing");Arrays.sort(files,Comparator.comparing(File::getName));LinkedHashMap<String,Entry> result=new LinkedHashMap<>();for(File file:files)result.putAll(read(file));return result;
 }
 private HfaIndex(){}
}
