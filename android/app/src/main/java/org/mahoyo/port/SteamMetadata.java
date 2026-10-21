package org.mahoyo.port;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** Reconstruct engine tables from the owner's version-pinned files, never from bundled payloads. */
final class SteamMetadata {
    static final String NATIVE_SHA="1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b";
    private final SteamPe pe;private final SteamAccessors accessors;
    private final Map<String,byte[]> files=new LinkedHashMap<>();
    SteamMetadata(SteamPe pe){this.pe=pe;accessors=new SteamAccessors(pe);}
    private static IOException invalid(){return new IOException("Invalid original Steam metadata");}
    private long u(int at,int size)throws IOException{return pe.integer(at,size);}
    private int s(int at,int size)throws IOException{long v=u(at,size);return (int)(v<<(64-size*8)>>(64-size*8));}
    private String name(long pointer,boolean wide)throws IOException{return pe.string(pointer,wide).toLowerCase(Locale.ROOT);}
    private String image(long pointer)throws IOException{String n=name(pointer,false);if(!n.matches("[a-z0-9_]+\\.mzp"))throw invalid();return n;}
    private void file(String name,StringBuilder text){files.put(name,text.toString().getBytes(StandardCharsets.UTF_8));}
    private static String quote(String text)throws IOException{for(int i=0;i<text.length();i++)if(text.charAt(i)<32)throw invalid();return "\""+text.replace("\\","\\\\").replace("\"","\\\"")+"\"";}
    private static void row(StringBuilder text,Object... values){for(int i=0;i<values.length;i++){if(i>0)text.append('\t');text.append(values[i]);}text.append('\n');}
    Map<String,byte[]> generate(byte[] archive,Map<String,List<String>> messages)throws Exception{
        layouts();effects();extra();maps();archive(archive,messages);return files;
    }
    private void layouts()throws Exception{
        TreeMap<String,int[]> records=new TreeMap<>();Map<String,String[]> recipes=new TreeMap<>();Set<String> legacy=new HashSet<>();int reserved=0,pairs=0;
        for(int i=0;i<10856;i++){
            int at=0x2f5308+i*72;long first=u(at,8),second=u(at+8,8);if(first==0&&second==0){reserved++;continue;}if(first==0)throw invalid();
            String primary=image(first),alias=second==0?null:image(second),key=alias==null?primary:alias;int w=(int)u(at+36,2),h=(int)u(at+38,2);if(w==0||h==0||records.put(key,new int[]{w,h,s(at+44,4),s(at+48,4)})!=null)throw invalid();
            recipes.put(key,new String[]{primary,alias});if(alias!=null)pairs++;
            // WH3..21 saves used this sparse row selection; current drawing uses the complete table below.
            if(i<=2490||i>=6819&&i<=10820)legacy.add(key);
        }
        if(records.size()!=10821||reserved!=35||pairs!=3818||legacy.size()!=6493)throw invalid();
        StringBuilder exact=new StringBuilder("WOHEXACTLAYOUTS1 "+NATIVE_SHA+" 10821\n"),layout=new StringBuilder(),origins=new StringBuilder(),body=new StringBuilder(),json=new StringBuilder("{\"schema\":1,\"target\":\"").append(NATIVE_SHA).append("\",\"records\":[");boolean comma=false;
        for(Map.Entry<String,int[]> entry:records.entrySet()){
            String name=entry.getKey(),stem=name.substring(0,name.length()-4);int[] v=entry.getValue();exact.append(stem);for(int n:v)exact.append(' ').append(n);exact.append('\n');body.append(name).append('\n');
            if(legacy.contains(name)){row(layout,stem,v[0],v[1]);row(origins,stem,v[2],v[3]);}
            String[] pair=recipes.get(name);if(comma)json.append(',');comma=true;json.append("{\"key\":").append(quote(name)).append(",\"primary\":").append(quote(pair[0])).append(",\"alias\":").append(pair[1]==null?"null":quote(pair[1])).append('}');
        }
        json.append("]}");long fnv=0xcbf29ce484222325L;for(byte b:body.toString().getBytes(StandardCharsets.US_ASCII))fnv=(fnv^(b&255))*0x100000001b3L;
        file("native-exact-layouts.tsv",exact);file("image-layout.tsv",layout);file("image-origins.tsv",origins);file("bitmap-recipes.json",json);file("native-image-resources.tsv",new StringBuilder(String.format(Locale.ROOT,"WOH_NATIVE_IMAGES1 10821 %016x\n",fnv)).append(body));
    }
    private void effects()throws Exception{
        StringBuilder effects=new StringBuilder("# effect\ttime\tx\ty\talpha\tscaleX\tscaleY\trotation\tcurve\tmode\tloopStart\tduration\n"),windows=new StringBuilder("# effect\tlocalFrame\tglobalFrame\tdescriptorBits\twindowX\twindowY\twindowWidth\twindowHeight\twindowColorMode\tsoftEdgeWidth\n"),properties=new StringBuilder("# effect\tlocalFrame\tglobalFrame\tdescriptorBits\toriginX\toriginY\tblendMode\tcolorMode\tcolorR\tcolorG\tcolorB\tvariantX\tvariantY\tbrightness\tcontrast\tauxResourceAndScale\tpriority\n");
        int begin=0;
        for(int id=1;id<=8372;id++){
            int target=(int)u(0xb8dc8+(id-1)*4,4);byte[] code=pe.rva(target,8);if(code[0]!=(byte)0xba||code[5]!=(byte)0x8b||code[6]!=(byte)0xc2||code[7]!=(byte)0xc3)throw invalid();int end=(int)u(target+1,4);if(end<=begin||end>28141)throw invalid();
            if(id==5714){begin=end;continue;}int duration=(int)u(0x3c68a0+(end-1)*64,4),loop=accessors.loop(id),last=-1;String descriptor="0x"+Long.toHexString(u(0x26cae0+id*8,8));
            for(int frame=begin;frame<end;frame++){
                int at=0x3c68a0+frame*64,time=(int)u(at,4);if(time<last)throw invalid();last=time;
                row(effects,id,time,s(at+8,2),s(at+10,2),s(at+12,2),s(at+24,2),s(at+26,2),s(at+20,4),s(at+4,2),s(at+6,2),loop,duration);
                windows.append(id).append('\t').append(frame-begin).append('\t').append(frame).append('\t').append(descriptor);for(int off=28;off<=38;off+=2)windows.append('\t').append(s(at+off,2));windows.append('\n');
                properties.append(id).append('\t').append(frame-begin).append('\t').append(frame).append('\t').append(descriptor).append('\t').append(s(at+14,2)).append('\t').append(s(at+16,2));for(int off=40;off<=60;off+=2)properties.append('\t').append(s(at+off,2));properties.append('\n');
            }begin=end;
        }
        if(begin!=28141)throw invalid();file("effects.tsv",effects);file("effects-window.tsv",windows);file("effects-properties.tsv",properties);
    }
    private void maps()throws Exception{
        StringBuilder music=new StringBuilder(),masks=new StringBuilder();
        for(int i=0;i<66;i++){long ptr=u(0x5806d0+i*8,8);if(ptr!=0){String n=name(ptr,true);if(!n.matches("[a-z0-9_]+\\.hw"))throw invalid();row(music,i,n);}}
        for(int i=0;i<15;i++){long ptr=u(0x5805e0+i*8,8);if(i==0){if(ptr!=0)throw invalid();continue;}String n=name(ptr,true);if(!n.matches("rule_[0-9]{2}\\.cbg"))throw invalid();row(masks,i,n);}
        StringBuilder wik=new StringBuilder("# WIKCHOICELAYOUT1 canvas=1920x1080 units=parent-canvas-pixels sourceUnits=source-pixels initialFocus=-1 monoState=0 colorState=1\n");Set<Integer> priorities=new HashSet<>();
        for(int i=0;i<14;i++){int at=0x2846a0+i*20,x=s(at,4),y=s(at+4,4),w=s(at+8,4),h=s(at+12,4),priority=s(at+16,4);if(x< -1920||x>=1920||y< -1080||y>=1080||w<1||w>4096||h<1||h>4096||priority<0||priority>13||!priorities.add(priority))throw invalid();row(wik,i,String.format(Locale.ROOT,"wik_sel%02d_mono.mzp",i),String.format(Locale.ROOT,"wik_sel%02d.mzp",i),x,y,w,h,priority,1,1,0,0,w,h);}
        file("music-map.tsv",music);file("mask-map.tsv",masks);file("wik-choice-layout.tsv",wik);
    }
    private void extra()throws Exception{
        List<String> flags=new ArrayList<>();for(int g=0;g<21;g++){StringBuilder text=new StringBuilder();boolean ended=false;for(int i=0;i<6;i++){int v=s(0x3c5f40+g*24+i*4,4);if(v==-1){ended=true;continue;}if(ended||v<0||v>=2048)throw invalid();if(text.length()>0)text.append(',');text.append(v);}if(text.length()==0)throw invalid();flags.add(text.toString());}
        StringBuilder gallery=new StringBuilder("WOHGALLERY1 "+NATIVE_SHA+" 401 1314\n");Set<String> seen=new HashSet<>();int groups=0;
        for(int slot=0;slot<420;slot++){long ptr=u(0x3c5220+slot*8,8);if(ptr<SteamAccessors.BASE||ptr>SteamAccessors.BASE+0x600000)throw invalid();StringBuilder variants=new StringBuilder();int count=0;
            for(int v=0;v<37;v++){long np=u((int)(ptr-SteamAccessors.BASE)+v*56,8);if(np==0)break;String n=image(np);if(!n.matches("img[0-9]{4}\\.mzp")||!seen.add(n))throw invalid();int ident=Integer.parseInt(n.substring(3,7)),group=-1;
                if(ident>=184&&ident<184+0xd84){int leaf=(int)u(0x1b714+(ident-184)*4,4);byte[] code=pe.rva(leaf,8);if((code[0]==0x31||code[0]==0x33)&&code[1]==(byte)0xdb)group=0;else if(code[0]==(byte)0xbb)group=s(leaf+1,4);else if(leaf==0x1b638)group=3;else throw invalid();}
                if(group< -1||group>20)throw invalid();variants.append("V ").append(slot).append(' ').append(v).append(' ').append(quote(n)).append(' ').append(group<0?"-":flags.get(group)).append('\n');count++;
            }if(count>0){gallery.append("G ").append(slot).append(' ').append(count).append('\n').append(variants);groups++;}
        }if(groups!=401||seen.size()!=1314)throw invalid();file("gallery-catalog.tsv",gallery);
        StringBuilder music=new StringBuilder("WOHMUSIC1 "+NATIVE_SHA+" 56\n");Set<Integer> ids=new HashSet<>();
        for(int i=0;i<56;i++){int at=0x57f620+i*56,id=s(at,4),atlas=s(at+4,4),x=s(at+20,4),y=s(at+24,4);if(id<1||id>65||!ids.add(id)||atlas<9||atlas>11||(x!=0&&x!=528)||y<0||y>936||y%72!=0)throw invalid();String n=name(u(0x5806d0+id*8,8),true);if(!n.matches("[a-z0-9_]+\\.hw"))throw invalid();music.append("M ").append(i).append(' ').append(id).append(' ').append(id+256).append(' ').append(quote(n)).append(' ').append(quote("mu_text"+(atlas-8)+"_select_ja.cbg")).append(' ').append(x).append(' ').append(y).append(" 528 72\n");}
        file("music-catalog.tsv",music);StringBuilder cp=new StringBuilder("WOHCHECKPOINT1 "+NATIVE_SHA+" 13\n");if(u(0x580660,8)!=0)throw invalid();Set<String> targets=new HashSet<>();for(int i=1;i<14;i++){String n=name(u(0x580660+i*8,8),true);if(!n.matches("teatime[a-z0-9]+\\.chs")||!targets.add(n))throw invalid();cp.append("C ").append(i).append(' ').append(quote(n)).append('\n');}file("checkpoint-map.tsv",cp);
    }
    private static final class Command {int pc,op,source;int[] args;}
    private static int word(byte[] data,int at)throws IOException{if(at<0||at>data.length-4)throw invalid();return (data[at]&255)|((data[at+1]&255)<<8)|((data[at+2]&255)<<16)|(data[at+3]<<24);}
    private static String literal(byte[] data,int offset)throws IOException{int first=48+offset,last=first;if(first<48||first>=data.length)throw invalid();while(last<data.length&&last-first<1024&&data[last]!=0)last++;if(last>=data.length||data[last]!=0)throw invalid();return new String(data,first,last-first,StandardCharsets.UTF_8).toLowerCase(Locale.ROOT);}
    private void archive(byte[] script,Map<String,List<String>> messages)throws Exception{
        List<Command> commands=new ArrayList<>();int pc=0,source=0;
        while(pc<20124){int op=word(script,48+pc),width=(op>=0&&op<=3||op>=8&&op<=10||op==0x19)?1:op==0x7b?3:op==0x7f?2:0;Command c=new Command();c.pc=pc;c.op=op;c.args=new int[width];for(int i=0;i<width;i++)c.args[i]=word(script,52+pc+i*4);if(op==0x7f)source=c.args[1];c.source=source;commands.add(c);pc+=4+width*4;}if(pc!=20124)throw invalid();
        Map<Integer,String[]> dispatch=new HashMap<>();
        for(int i=0;i<commands.size()-7;i++){
            int[] pattern={0,0,0xea,0,0x30,1,0x19};boolean match=true;for(int j=0;j<7;j++)if(commands.get(i+j).op!=pattern[j])match=false;if(!match||commands.get(i).args[0]!=8||commands.get(i+1).args[0]!=1||commands.get(i+6).args[0]!=0)continue;
            int chapter=commands.get(i+3).args[0],skip=commands.get(i+5).args[0];String target=null;Map<Integer,Integer> writes=new HashMap<>();
            for(int j=i+7;j<commands.size()&&commands.get(j).pc<skip;j++){Command c=commands.get(j);if(c.op==0xf3){if(target!=null||commands.get(j-1).op!=3)throw invalid();target=literal(script,commands.get(j-1).args[0]);if(!target.endsWith(".chs"))target+=".chs";}
                if(c.op==0xe9){Command a=commands.get(j-4),b=commands.get(j-3),d=commands.get(j-2),e=commands.get(j-1);if(a.op!=0||b.op!=3||d.op!=0||e.op!=0||e.args[0]!=0)throw invalid();writes.put(d.args[0],a.args[0]);}}
            if(target==null||writes.size()!=3||!writes.keySet().equals(new HashSet<>(Arrays.asList(24,25,26)))||dispatch.put(chapter,new String[]{target,""+writes.get(24),""+writes.get(25),""+writes.get(26)})!=null)throw invalid();
        }if(dispatch.size()!=73)throw invalid();SteamAccessors.Archive original=accessors.archive();StringBuilder text=new StringBuilder("WOHARCHIVE1 "+NATIVE_SHA+" 73 20\n");String[] headers={"010","015","020","030","040","050","055","060","070","080","085","090","extra","100","110","120","130","extra","extra","130"};for(int i=0;i<headers.length;i++)text.append("G ").append(i).append(' ').append(quote("archive_"+headers[i]+"_ja.cbg")).append('\n');
        for(int i=0;i<73;i++){int group=(int)original.value(i,0,4),chapter=(int)original.value(i,40,4),story=(int)original.value(i,44,4);String key=pe.string(original.value(i,8,8),true),preview=name(original.value(i,24,8),true);List<String> label=messages.get(key);String[] branch=dispatch.get(chapter);if(!key.equals(String.format(Locale.ROOT,"SYSMES_ARCHIVE_%03d",i))||story!=i+1||group<0||group>=20||label==null||branch==null||!branch[3].equals(""+story))throw invalid();
            text.append("E ").append(story).append(' ').append(group).append(' ').append(chapter).append(' ').append(original.value(i,48,4)).append(' ');int[] gates=original.flags.get(i);for(int j=0;j<gates.length;j++){if(gates[j]<0||gates[j]>=2048)throw invalid();if(j>0)text.append(',');text.append(gates[j]);}text.append(' ').append(quote(label.get(1))).append(' ').append(quote(preview)).append(' ').append(quote(branch[0]));for(int j=1;j<4;j++)text.append(' ').append(branch[j]);text.append('\n');
        }file("archive-catalog.tsv",text);
    }
}
