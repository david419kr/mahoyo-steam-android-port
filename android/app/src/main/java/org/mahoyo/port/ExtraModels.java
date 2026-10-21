package org.mahoyo.port;

import org.json.*;
import java.util.*;

final class ExtraModels {
    static long generation(JSONObject v){long id=v.optLong("id",0);if(id<=0)throw new IllegalArgumentException("Invalid Extra generation");return id;}
    static final class Group {
        final int id,count;final String preview;
        Group(JSONObject v){id=v.optInt("id",-1);count=v.optInt("count",-1);preview=v.optString("preview","");if(id<0||id>=420||count<0||count>37||(count>0&&!ArchiveCatalog.resource(preview))||(count==0&&!preview.isEmpty()))throw new IllegalArgumentException("Invalid gallery group");}
    }
    static final class Variant {
        final int index;final String name;
        Variant(JSONObject v){index=v.optInt("index",-1);name=v.optString("name","");if(index<0||index>=37||!ArchiveCatalog.resource(name))throw new IllegalArgumentException("Invalid gallery variant");}
    }
    static final class Gallery {
        final long id;final int group,variant,focus;final List<Group> groups;final List<Variant> variants;
        Gallery(JSONObject v){id=generation(v);group=v.optInt("group",-1);variant=v.optInt("variant",-1);focus=v.optInt("focus",-1);if(group< -1||group>=420||variant< -1||variant>=37)throw new IllegalArgumentException("Invalid gallery selection");ArrayList<Group> gs=new ArrayList<>();HashSet<Integer> seen=new HashSet<>();JSONArray rows=v.optJSONArray("groups");if(rows==null||rows.length()!=401)throw new IllegalArgumentException("Incomplete gallery");for(int i=0;i<rows.length();i++){JSONObject row=rows.optJSONObject(i);if(row==null)throw new IllegalArgumentException("Invalid gallery row");Group g=new Group(row);if(!seen.add(g.id))throw new IllegalArgumentException("Duplicate gallery group");gs.add(g);}groups=Collections.unmodifiableList(gs);ArrayList<Variant> vs=new ArrayList<>();seen.clear();rows=v.optJSONArray("variants");if(rows==null||rows.length()>37)throw new IllegalArgumentException("Invalid gallery variants");for(int i=0;i<rows.length();i++){JSONObject row=rows.optJSONObject(i);if(row==null)throw new IllegalArgumentException("Invalid gallery variant row");Variant x=new Variant(row);if(!seen.add(x.index))throw new IllegalArgumentException("Duplicate gallery variant");vs.add(x);}variants=Collections.unmodifiableList(vs);if(group<0&&variant!=-1||group>=0&&current()==null)throw new IllegalArgumentException("Gallery selection is locked or absent");if(group>=0){boolean found=false;for(Group g:groups)if(g.id==group&&g.count==variants.size())found=true;if(!found)throw new IllegalArgumentException("Gallery selection group mismatch");}}
        Variant current(){for(Variant v:variants)if(v.index==variant)return v;return null;}
        String action(String action){return id+" "+action+(action.equals("next")||action.equals("previous")?" "+group:"");}
        String select(int group){return id+" select "+group;}
    }
    static final class Track {
        final int row;final boolean unlocked;final String atlas;final int[] crop;
        Track(JSONObject v){row=v.optInt("row",-1);unlocked=v.optBoolean("unlocked");atlas=v.optString("atlas","");JSONArray rect=v.optJSONArray("crop");crop=new int[4];if(row<0||row>=56)throw new IllegalArgumentException("Invalid music row");if(unlocked){if(!ArchiveCatalog.resource(atlas)||rect==null||rect.length()!=4)throw new IllegalArgumentException("Missing music label");for(int i=0;i<4;i++)crop[i]=rect.optInt(i,-1);if((crop[0]!=0&&crop[0]!=528)||crop[1]<0||crop[1]>936||crop[1]%72!=0||crop[2]!=528||crop[3]!=72)throw new IllegalArgumentException("Invalid music label crop");}else if(!atlas.isEmpty()||rect!=null)throw new IllegalArgumentException("Locked music label exposed");}
    }
    static final class Music {
        final long id;final int row,selected;final boolean repeat;final String phase;final List<Track> tracks;
        Music(JSONObject v){id=generation(v);row=v.optInt("row",-1);selected=v.optInt("selected",-1);repeat=v.optBoolean("repeat");phase=v.optString("phase","");if(row< -1||row>=56||selected< -1||selected>=56||!Arrays.asList("idle","playing","switching","stopping","closing").contains(phase))throw new IllegalArgumentException("Invalid music state");ArrayList<Track> ts=new ArrayList<>();JSONArray rows=v.optJSONArray("tracks");if(rows==null||rows.length()!=56)throw new IllegalArgumentException("Incomplete music catalog");for(int i=0;i<rows.length();i++){JSONObject r=rows.optJSONObject(i);if(r==null)throw new IllegalArgumentException("Invalid music row");Track t=new Track(r);if(t.row!=i)throw new IllegalArgumentException("Invalid music order");ts.add(t);}tracks=Collections.unmodifiableList(ts);if(row>=0&&!tracks.get(row).unlocked)throw new IllegalArgumentException("Active music row is locked");}
        String action(String action){return id+" "+action+(action.equals("play")?" "+selected:"");}
        String play(int row){if(row<0||row>=tracks.size()||!tracks.get(row).unlocked)throw new IllegalArgumentException("Music row is locked");return id+" play "+row;}
        boolean closing(){return phase.equals("closing");}
    }
    static Gallery gallery(JSONObject v){return v==null?null:new Gallery(v);}
    static Music music(JSONObject v){return v==null?null:new Music(v);}
    static int columns(int width,float density,int cardDp){return Math.max(1,Math.min(8,(int)((width-24*density)/(cardDp*density))));}
}
