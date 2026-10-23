package org.mahoyo.port;

import org.json.JSONObject;
import org.junit.Test;
import static org.junit.Assert.*;

public final class ChapterFrameTest {
    private Scene scene(JSONObject value)throws Exception{return new Scene(value.toString());}
    @Test public void coversOpeningReadyAndClosingWithoutChangingOrdinaryScenes()throws Exception {
        for(String module:new String[]{"TEATIME1.chs","teatime5A.chs","scripts/TEATIME13.chs","scripts\\TEATIME5B.chs"}){
            Scene card=scene(new JSONObject().put("scene","play").put("module",module));
            assertTrue(module,card.wholeFrame);assertFalse(card.cover(true));
        }
        assertTrue(scene(new JSONObject().put("scene","teatime")).wholeFrame);
        assertTrue(scene(new JSONObject().put("modal",new JSONObject().put("kind","teatime"))).wholeFrame);
        for(String module:new String[]{"1_0.chs","TEATIME.chs","NOT_TEATIME1.chs","TEATIME1.chs.bak"}){
            Scene play=scene(new JSONObject().put("module",module));
            assertFalse(module,play.wholeFrame);assertTrue(play.cover(true));assertFalse(play.cover(false));
        }
    }
    @Test public void outgoingCardRetainsOwnFitPolicy()throws Exception {
        Scene play=scene(new JSONObject().put("module","2_0.chs").put("transition",new JSONObject()
            .put("old",new JSONObject().put("scene","play").put("module","TEATIME1.chs"))));
        assertTrue(play.cover(true));assertFalse(play.transition.old.cover(true));
        Scene card=scene(new JSONObject().put("module","TEATIME1.chs").put("transition",new JSONObject()
            .put("old",new JSONObject().put("module","1_0.chs"))));
        assertFalse(card.cover(true));assertTrue(card.transition.old.cover(true));
    }
}
