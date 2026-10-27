package org.mahoyo.port;

import java.util.List;
import org.junit.Test;
import static org.junit.Assert.*;

public class RubyReadingTest {
    @Test public void nativePrefixIsConsumedOnceOnlyAtReadingStart() {
        String[][] cases={{"@_ABC","ABC"},{"@_",""},{"@_@_ABC","@_ABC"},
            {"A@_ABC","A@_ABC"},{"@p2/@_ABC","@_ABC"},{"@_@p2/ABC","@p2/ABC"},
            {"@@_ABC","@@_ABC"},{"@_😀","😀"}};
        for(String[] row:cases)assertEquals(row[0],row[1],TextOverlay.RubyReading.parse(row[0]).text);
        assertFalse(TextOverlay.RubyReading.parse("A@_ABC").fixedLeft);
        assertFalse(TextOverlay.RubyReading.parse("@p2/@_ABC").fixedLeft);
    }
    @Test public void fixedAnchorScalesWithoutChangingOrdinaryRubyOrOffsets() {
        TextOverlay.RubyReading fixed=TextOverlay.RubyReading.parse("@_reading");
        assertTrue(fixed.fixedLeft);
        assertEquals(73f,fixed.left(200,80,300,1),.001f);
        assertEquals(146f,fixed.left(0,800,30,2),.001f);
        assertEquals(36.5f,fixed.left(10,80,10,.5f),.001f);
        assertEquals(220f,TextOverlay.RubyReading.parse("reading").left(200,80,40,1),.001f);
        TextOverlay.RubyReading offset=TextOverlay.RubyReading.parse("@p-6,3/reading");
        assertEquals(208f,offset.left(200,80,40,2),.001f);
        assertEquals(3,offset.y);
    }
    @Test public void bodyAndHistoryUseTheSameRubyParserWithoutMutatingSavedText() {
        String raw="@i<AB|@_reading>@r";
        List<TextOverlay.NativeTextToken> body=TextOverlay.NativeTextToken.parse(raw);
        assertEquals(1,body.size());assertEquals("AB",body.get(0).base);
        assertEquals("reading",body.get(0).ruby.text);assertTrue(body.get(0).italic);
        List<HistoryModel.Run> runs=HistoryModel.runs(raw,false,0xffffffff);
        TextOverlay.NativeTextToken history=TextOverlay.NativeTextToken.parse(runs.get(0).text).get(0);
        assertEquals(body.get(0).ruby.text,history.ruby.text);assertTrue(history.ruby.fixedLeft);
        assertEquals("@i<AB|@_reading>@r",raw);
        TextOverlay.NativeTextToken literal=TextOverlay.NativeTextToken.parse("<A@_B|@iR>").get(0);
        assertEquals("A@_B",literal.base);assertEquals("@iR",literal.ruby.text);
    }
}
