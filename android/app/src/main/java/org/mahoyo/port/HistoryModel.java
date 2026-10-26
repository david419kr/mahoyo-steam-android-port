package org.mahoyo.port;

import org.json.*;
import java.util.*;

/** Native history rows stay ordered and are grouped only by explicit Native boundaries. */
final class HistoryModel {
    final String revision;final List<Group> groups;
    HistoryModel(String revision,List<Group> groups){this.revision=revision;this.groups=Collections.unmodifiableList(groups);}
    static final class Voice {
        final boolean active,available;final int backend,volume,pan,variant,actorId;final String name;
        Voice(JSONObject value){active=value.optBoolean("active");available=value.optBoolean("available");backend=value.optInt("backend",-1);name=value.optString("name");volume=value.optInt("volume");pan=value.optInt("pan");variant=value.optInt("variant");actorId=value.optInt("actorId");}
    }
    static final class Row {
        final int index,leadingBreaks,color;final float nativeFont;final String text;final boolean known,starts,ends,italic,rewindable;
        final List<Voice> voices;
        Row(int index,JSONObject value,String legacy){
            this.index=value==null?index:value.optInt("index",index);known=value!=null&&value.optBoolean("metadataKnown");text=value==null?legacy:value.optString("text");
            rewindable=known&&value.optBoolean("rewindAvailable");
            leadingBreaks=known?Math.max(0,value.optInt("leadingBreaks")):0;starts=!known||value.optBoolean("startsGroup");ends=!known||value.optBoolean("endsGroup");italic=known&&value.optBoolean("initialItalic");
            double font=value==null?48:value.optDouble("fontSize",48);nativeFont=Double.isFinite(font)&&font>0?(float)font:48;color=color(value==null?"":value.optString("fontColor"),0xffffffff);
            ArrayList<Voice> items=new ArrayList<>();JSONArray list=value==null?null:value.optJSONArray("voices");if(known&&list!=null)for(int i=0;i<list.length();i++){JSONObject voice=list.optJSONObject(i);if(voice!=null)items.add(new Voice(voice));}voices=Collections.unmodifiableList(items);
        }
    }
    static final class Group {
        final String key;final int start,count;final List<Row> rows;final boolean hasVoice,playable,rewindable;final String reason;
        Group(String revision,List<Row> input){
            rows=Collections.unmodifiableList(new ArrayList<>(input));start=rows.get(0).index;count=rows.size();key=revision+":"+start+":"+count;
            boolean any=false;String issue=revision.isEmpty()?"履歴情報を確認できません":"";int expected=start;
            for(Row row:rows){if(row.index!=expected++&&issue.isEmpty())issue="履歴が更新されました";if(!row.known){if(issue.isEmpty())issue="この保存データに音声情報はありません";continue;}int eligible=0;
                for(Voice voice:row.voices){if(!voice.active||voice.name.isEmpty())continue;any=true;if(voice.backend!=0){if(issue.isEmpty())issue="この音声形式は未対応です";continue;}eligible++;if(!voice.available&&issue.isEmpty())issue="音声データがありません";}
                if(eligible>1)issue="同じ行の複数音声は未対応です";
            }
            hasVoice=any;playable=any&&issue.isEmpty();reason=issue;rewindable=!revision.isEmpty()&&rows.get(rows.size()-1).ends&&rows.stream().allMatch(row->row.rewindable);
        }
        String payload(String revision){return revision+" "+start+" "+count;}
        boolean contains(int row){return row>=start&&row<start+count;}
    }
    static HistoryModel parse(JSONObject scene){
        String revision=scene.optString("historyRevision");JSONArray values=scene.optJSONArray("history");ArrayList<Group> groups=new ArrayList<>();ArrayList<Row> current=new ArrayList<>();Row previous=null;
        if(values!=null)for(int index=0;index<values.length();index++){Object item=values.opt(index);Row row=new Row(index,item instanceof JSONObject?(JSONObject)item:null,item instanceof String?(String)item:"");if(!current.isEmpty()&&(row.starts||previous.ends)){groups.add(new Group(revision,current));current.clear();}current.add(row);previous=row;}
        if(!current.isEmpty())groups.add(new Group(revision,current));return new HistoryModel(revision,groups);
    }
    Group playingGroup(JSONObject replay){
        if(replay==null||!replay.optString("status").equals("playing"))return null;JSONArray clips=replay.optJSONArray("clips");if(clips==null||clips.length()==0)return null;
        Group found=null;for(int i=0;i<clips.length();i++){JSONObject clip=clips.optJSONObject(i);if(clip==null)return null;int row=clip.optInt("row",-1);if(found==null){for(Group candidate:groups)if(candidate.contains(row)){found=candidate;break;}if(found==null)return null;}else if(!found.contains(row))return null;}return found;
    }
    static String replayPair(JSONObject value){if(value==null||!value.optString("status").equals("playing"))return "";String generation=value.optString("generation"),id=value.optString("id");return generation.isEmpty()||id.isEmpty()?"":generation+" "+id;}
    static int color(String text,int fallback){try{if(text.length()==7&&text.charAt(0)=='#')return (int)Long.parseLong(text.substring(1),16)|0xff000000;if(text.length()==9&&text.charAt(0)=='#')return (int)Long.parseLong(text.substring(1),16);}catch(NumberFormatException ignored){}return fallback;}
    private static Integer nativeHex(String text,int at){if(text.length()<at+6)return null;for(int i=at;i<at+6;i++)if("0123456789abcdef".indexOf(text.charAt(i))<0)return null;return color("#"+text.substring(at,at+6),0);}

    static final class Run {
        final String text;final boolean italic;final int color,shadow;
        Run(String text,boolean italic,int color,int shadow){this.text=text;this.italic=italic;this.color=color;this.shadow=shadow;}
    }
    /** Caption-only static formatting. Raw ruby fields are never recompiled.
     * Typeface family/character animation tags become static text in history. */
    static List<Run> runs(Row row){return runs(row.text,row.italic,row.color);}
    static List<Run> runs(String text,boolean initialItalic,int initialColor){
        ArrayList<Run> out=new ArrayList<>();StringBuilder body=new StringBuilder();boolean italic=initialItalic,runItalic=italic;int color=initialColor,shadow=0xff000000,at=text.startsWith("＆ダミー＆")?"＆ダミー＆".length():0;ArrayList<Integer> colors=new ArrayList<>(),shadows=new ArrayList<>();
        while(at<text.length()){
            char ch=text.charAt(at);int end=ch=='<'?text.indexOf('>',at):-1,split=end>=0?text.indexOf('|',at):-1;
            if(split>at&&split<end){body.append(text,at,end+1);at=end+1;continue;}
            if(text.startsWith("@@x",at)){body.append("@@x");at+=3;continue;}
            boolean reset=false;Integer nextColor=null,nextShadow=null;int consume=0;
            if(text.startsWith("@i",at)){body.append("@i");italic=true;at+=2;continue;}
            if(text.startsWith("@o",at)){body.append("@o");at+=2;continue;}
            if(text.startsWith("@r",at)){reset=true;consume=2;}
            else if(text.startsWith("@x",at)||text.startsWith("@z",at)){at+=2;continue;}
            else if(at+2<text.length()&&text.charAt(at)=='@'&&text.charAt(at+1)=='a'&&(text.charAt(at+2)=='1'||text.charAt(at+2)=='2')){at+=3;continue;}
            else if(at+2<text.length()&&text.startsWith("@c",at)&&text.charAt(at+2)>='0'&&text.charAt(at+2)<='9'){
                int n=text.charAt(at+2)-'0';consume=3;if(n<=4)nextColor=n==0?0xffffffff:n==1?0xffff0000:0xff646464;if(n>=5||n==0)nextShadow=n==5?0xff002142:n==6?0xffff0000:0xff000000;
            }else if(end>=0){String tag=text.substring(at+1,end).toLowerCase(Locale.ROOT);consume=end-at+1;
                if(tag.startsWith("/i")){body.append(text,at,end+1);italic=false;at=end+1;continue;}
                if(tag.startsWith("i")){body.append(text,at,end+1);italic=true;at=end+1;continue;}
                if(tag.startsWith("fr"))reset=true;
                else if(tag.startsWith("/c")){if(!colors.isEmpty())nextColor=colors.remove(colors.size()-1);}
                else if(tag.startsWith("sc"))nextShadow=nativeHex(tag,2);
                else if(tag.startsWith("c"))nextColor=nativeHex(tag,1);
                else{at=end+1;continue;}
            }
            if(consume>0){if(body.length()>0){out.add(new Run(body.toString(),runItalic,color,shadow));body.setLength(0);}
                if(reset){italic=false;color=colors.isEmpty()?initialColor:colors.get(0);shadow=shadows.isEmpty()?0xff000000:shadows.get(0);colors.clear();shadows.clear();}
                else{if(nextColor!=null){if(!text.startsWith("</",at))colors.add(color);color=nextColor;}if(nextShadow!=null){shadows.add(shadow);shadow=nextShadow;}}
                runItalic=italic;at+=consume;continue;
            }
            body.append(ch);at++;
        }
        if(body.length()>0)out.add(new Run(body.toString(),runItalic,color,shadow));return out;
    }
}
