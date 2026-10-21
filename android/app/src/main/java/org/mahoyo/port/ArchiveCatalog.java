package org.mahoyo.port;

import org.json.*;
import java.util.*;

/** Only eligible entries are supplied by the native runtime's AND predicate. */
final class ArchiveCatalog {
    static final class Entry {
        final int id,group;final String label,preview,header;final boolean read;
        Entry(JSONObject v){id=v.optInt("id",-1);group=v.optInt("group",-1);label=v.optString("label");preview=v.optString("preview");header=v.optString("header");read=v.optBoolean("read");if(id<1||id>73||group<0||group>19||label.isEmpty()||!resource(preview)||!resource(header))throw new IllegalArgumentException("Invalid Archive entry");}
    }
    final long generation;final int selected,checkpoint;final List<Entry> entries;
    private ArchiveCatalog(JSONObject value){generation=value.optLong("id",0);selected=value.optInt("selected");checkpoint=value.optInt("checkpoint");if(generation<=0||checkpoint<0||checkpoint>13)throw new IllegalArgumentException("Invalid Archive generation");ArrayList<Entry> rows=new ArrayList<>();HashSet<Integer> ids=new HashSet<>();JSONArray list=value.optJSONArray("entries");if(list==null||list.length()>73)throw new IllegalArgumentException("Invalid Archive list");for(int i=0;i<list.length();i++){JSONObject row=list.optJSONObject(i);if(row==null)throw new IllegalArgumentException("Invalid Archive row");Entry e=new Entry(row);if(!ids.add(e.id))throw new IllegalArgumentException("Duplicate Archive story");rows.add(e);}entries=Collections.unmodifiableList(rows);}
    static ArchiveCatalog parse(JSONObject value){return value==null?null:new ArchiveCatalog(value);}
    static boolean resource(String name){return !name.isEmpty()&&name.length()<128&&name.matches("[a-z0-9_.-]+")&&!name.equals(".")&&!name.equals("..");}
    String play(int story){for(Entry e:entries)if(e.id==story)return generation+" "+story;throw new IllegalArgumentException("Story is absent from eligible Archive");}
    String back(){return Long.toString(generation);}
    String backLabel(){return checkpoint==0?"タイトルに戻る":"ティータイムに戻る";}
}
