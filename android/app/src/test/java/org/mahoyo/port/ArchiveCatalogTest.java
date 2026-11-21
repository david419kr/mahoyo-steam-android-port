package org.mahoyo.port;

import org.json.*;
import org.junit.Test;
import static org.junit.Assert.*;

public final class ArchiveCatalogTest {
    private JSONObject row(int id,boolean unlocked,boolean read) throws Exception {
        return new JSONObject().put("id",id).put("group",0).put("unlocked",unlocked)
            .put("read",read).put("label",unlocked?"Authored story":"")
            .put("preview",unlocked?"authored.mzp":"").put("header","section.cbg");
    }
    private ArchiveCatalog catalog(JSONObject... rows) throws Exception {
        JSONArray entries=new JSONArray();for(JSONObject row:rows)entries.put(row);
        return ArchiveCatalog.parse(new JSONObject().put("id",42).put("entries",entries));
    }
    @Test public void readUnreadAndLockedRemainDistinct() throws Exception {
        ArchiveCatalog c=catalog(row(1,true,true),row(2,true,false),row(3,false,false));
        assertEquals(3,c.entries.size());
        assertFalse(c.entries.get(0).unread());assertTrue(c.entries.get(1).unread());
        ArchiveCatalog.Entry locked=c.entries.get(2);
        assertFalse(locked.unlocked);assertFalse(locked.unread());
        assertEquals("",locked.label);assertEquals("",locked.preview);
        assertEquals("42 2",c.play(2));
        try{c.play(3);fail("Locked entry was playable");}catch(IllegalArgumentException expected){}
    }
    @Test public void lockedEntriesCannotExposeStoryData() throws Exception {
        for(String field:new String[]{"label","preview","read"}){
            JSONObject row=row(1,false,false);
            row.put(field,field.equals("read")?true:"hidden.mzp");
            try{catalog(row);fail("Locked row leaked "+field);}catch(IllegalArgumentException expected){}
        }
    }
    @Test public void completionUsesGameReadFlagsOnRefresh() throws Exception {
        assertTrue(catalog(row(1,true,false)).entries.get(0).unread());
        assertFalse(catalog(row(1,true,true)).entries.get(0).unread());
    }
}
