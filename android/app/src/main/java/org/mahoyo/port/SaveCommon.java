package org.mahoyo.port;

import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.zip.CRC32;

/** The three port-owned progress registries. This never reads Steam credentials. */
final class SaveCommon {
    static final List<String> NAMES=Arrays.asList("shared.whp","seen-images.whp","trophies.whp");
    static byte[] merge(String name,byte[] current,byte[] incoming)throws IOException {
        byte[] b=payload(incoming),a=current==null?null:payload(current),out;
        if(name.equals("shared.whp"))out=shared(a,b);
        else if(name.equals("seen-images.whp")){TreeSet<String> values=seen(b);if(a!=null)values.addAll(seen(a));StringBuilder text=new StringBuilder("WOHSEEN1\n");for(String value:values)text.append('"').append(value.replace("\\","\\\\").replace("\"","\\\"")).append("\"\n");out=text.toString().getBytes(StandardCharsets.UTF_8);}
        else if(name.equals("trophies.whp")){TreeSet<Integer> values=trophies(b);if(a!=null)values.addAll(trophies(a));StringBuilder text=new StringBuilder("WOHTROPHY1 ").append(values.size()).append('\n');for(int id:values)text.append(id).append('\n');out=text.toString().getBytes(StandardCharsets.UTF_8);}
        else throw new IOException("Unknown progress registry");
        return container(out);
    }
    static void validate(String name,byte[] file)throws IOException {merge(name,null,file);}
    static byte[] payload(byte[] file)throws IOException {
        try{
            if(file.length<48||file.length>1024*1024+48)throw new IOException("Invalid progress size");
            ByteBuffer b=ByteBuffer.wrap(file).order(ByteOrder.LITTLE_ENDIAN);byte[] signature=new byte[8];b.get(signature);
            CRC32 crc=new CRC32();crc.update(file,16,file.length-16);
            if(!Arrays.equals(signature,"WOHSAVE2".getBytes(StandardCharsets.US_ASCII))||b.getInt()!=file.length-16||Integer.toUnsignedLong(b.getInt())!=crc.getValue())throw new IOException("Invalid progress checksum");
            for(int i=0;i<7;i++)if(b.getInt()!=0)throw new IOException("Unexpected progress container fields");
            int count=b.getInt();if(count!=b.remaining())throw new IOException("Invalid progress payload");byte[] data=new byte[count];b.get(data);return data;
        }catch(BufferUnderflowException|NegativeArraySizeException error){throw new IOException("Truncated progress file",error);}
    }
    static byte[] container(byte[] payload)throws IOException {
        if(payload.length>1024*1024)throw new IOException("Progress payload too large");
        ByteBuffer b=ByteBuffer.allocate(48+payload.length).order(ByteOrder.LITTLE_ENDIAN);b.put("WOHSAVE2".getBytes(StandardCharsets.US_ASCII));b.putInt(32+payload.length).putInt(0);for(int i=0;i<7;i++)b.putInt(0);b.putInt(payload.length).put(payload);
        CRC32 crc=new CRC32();crc.update(b.array(),16,b.capacity()-16);b.putInt(12,(int)crc.getValue());return b.array();
    }
    static final class Shared {
        final long generation;final int[] variables;final byte[] flags;
        Shared(byte[] bytes)throws IOException {
            try{ByteBuffer b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);byte[] tag=new byte[10];b.get(tag);generation=b.getLong();int vars=b.getInt(),bits=b.getInt();
                if(!Arrays.equals(tag,"WOHSHARED1".getBytes(StandardCharsets.US_ASCII))||generation<=0||generation==Long.MAX_VALUE||vars<1||vars>4096||bits<1||bits>16384||b.remaining()!=vars*4+bits)throw new IOException("Invalid shared progress");
                variables=new int[vars];for(int i=0;i<vars;i++)variables[i]=b.getInt();flags=new byte[bits];b.get(flags);for(byte flag:flags)if(flag!=0&&flag!=1)throw new IOException("Invalid shared flag");
            }catch(BufferUnderflowException error){throw new IOException("Truncated shared progress",error);}
        }
    }
    private static byte[] shared(byte[] a,byte[] b)throws IOException {
        Shared incoming=new Shared(b),current=a==null?null:new Shared(a);int vars=Math.max(incoming.variables.length,current==null?0:current.variables.length),bits=Math.max(incoming.flags.length,current==null?0:current.flags.length);
        boolean pristine=pristine(incoming)&&(current==null||pristine(current));
        ByteBuffer out=ByteBuffer.allocate(26+vars*4+bits).order(ByteOrder.LITTLE_ENDIAN);out.put("WOHSHARED1".getBytes(StandardCharsets.US_ASCII)).putLong(pristine?1:Math.max(incoming.generation,current==null?0:current.generation)+1).putInt(vars).putInt(bits);
        // Native26 is an unlock bitmask. Other numbers may be settings/navigation,
        // so do not invent larger counters or mix the paired chapter/checkpoint8/9.
        boolean keepScalars=current!=null&&!pristine(current);
        for(int i=0;i<vars;i++){int x=i<incoming.variables.length?incoming.variables[i]:0,y=current!=null&&i<current.variables.length?current.variables[i]:0;out.putInt(i==26?x|y:keepScalars&&i<current.variables.length?y:x);}
        for(int i=0;i<bits;i++)out.put((byte)((i<incoming.flags.length?incoming.flags[i]:0)|(current!=null&&i<current.flags.length?current.flags[i]:0)));
        return out.array();
    }
    private static boolean pristine(Shared value){if(value.flags.length<=321||value.flags[321]!=1)return false;for(int variable:value.variables)if(variable!=0)return false;for(int i=0;i<value.flags.length;i++)if(i!=321&&value.flags[i]!=0)return false;return true;}
    private static TreeSet<String> seen(byte[] bytes)throws IOException {
        String text=utf8(bytes);if(!text.startsWith("WOHSEEN1\n"))throw new IOException("Invalid image registry");TreeSet<String> out=new TreeSet<>();int at=9;
        while(at<text.length()){while(at<text.length()&&Character.isWhitespace(text.charAt(at)))at++;if(at==text.length())break;if(text.charAt(at++)!='"')throw new IOException("Invalid quoted image name");StringBuilder name=new StringBuilder();boolean closed=false;
            while(at<text.length()){char c=text.charAt(at++);if(c=='"'){closed=true;break;}if(c=='\\'){if(at==text.length())throw new IOException("Invalid image escape");c=text.charAt(at++);}name.append(c);if(name.length()>4096)throw new IOException("Image name too long");}
            String value=name.toString();if(!closed||value.isEmpty()||value.getBytes(StandardCharsets.UTF_8).length>4096||value.contains("..")||value.indexOf('/')>=0||value.indexOf('\\')>=0||value.indexOf(':')>=0||value.codePoints().anyMatch(c->c<32))throw new IOException("Invalid image name");out.add(value);
        }return out;
    }
    private static TreeSet<Integer> trophies(byte[] bytes)throws IOException {
        try{String[] words=utf8(bytes).trim().split("\\s+");if(words.length<2||!words[0].equals("WOHTROPHY1"))throw new IOException("Invalid trophy registry");int count=Integer.parseInt(words[1]);if(count<0||count>26||words.length!=count+2)throw new IOException("Invalid trophy count");TreeSet<Integer> out=new TreeSet<>();for(int i=2;i<words.length;i++){int id=Integer.parseInt(words[i]);if(id<1||id>26||!out.add(id))throw new IOException("Invalid trophy ID");}return out;}catch(NumberFormatException error){throw new IOException("Invalid trophy value",error);}
    }
    private static String utf8(byte[] bytes)throws IOException {return StandardCharsets.UTF_8.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString();}
    private SaveCommon(){}
}
