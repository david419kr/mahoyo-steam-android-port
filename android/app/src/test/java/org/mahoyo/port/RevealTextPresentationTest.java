package org.mahoyo.port;

import org.json.*;
import org.junit.Test;
import static org.junit.Assert.*;

public final class RevealTextPresentationTest {
    private JSONObject step(int version,int visible)throws Exception{
        return new JSONObject().put("scene","play").put("waiting","input").put("revealing",true)
            .put("version",version).put("runtimeEpoch","1").put("language","ja").put("module","authored.chs")
            .put("modulePc",128).put("instructions",500).put("textLine",12).put("windowMode",0)
            .put("text",new JSONArray().put(new JSONObject().put("text","Authored reveal text")
                .put("visibleCharacters",visible).put("activeFrom",0).put("cursor",false).put("x",300).put("fontSize",52)));
    }
    @Test public void delayedFramesKeepRevealingWithoutWaitingForFinalSentence()throws Exception{
        int presented=0;
        for(int tick=3;tick<20;tick++){
            JSONObject latest=step(tick,tick),frame=step(tick-2,tick-2);
            assertTrue(RevealTextPresentation.compatible(latest,frame));
            presented=frame.getJSONArray("text").getJSONObject(0).getInt("visibleCharacters");
        }
        assertEquals(17,presented);
    }
    @Test public void differentPageLanguageAndExecutionNeverBypassCommitBarrier()throws Exception{
        for(String key:new String[]{"runtimeEpoch","language","module","modulePc","instructions","textLine","windowMode"}){
            JSONObject changed=step(3,3).put(key,"different");
            assertFalse(key,RevealTextPresentation.compatible(changed,step(2,2)));
        }
        for(String key:new String[]{"text","activeFrom","x","fontSize"}){
            JSONObject changed=step(3,3);changed.getJSONArray("text").getJSONObject(0).put(key,"different");
            assertFalse(key,RevealTextPresentation.compatible(changed,step(2,2)));
        }
    }
    @Test public void transitionsAndOtherJobsKeepStrictAcknowledgements()throws Exception{
        for(String key:new String[]{"movie","transition","choice","modal","archive","gallery","musicRoom"})
            assertFalse(key,RevealTextPresentation.compatible(step(3,3).put(key,new JSONObject()),step(2,2)));
        assertFalse(RevealTextPresentation.compatible(step(3,3),step(2,2).put("waiting","frame")));
        assertFalse(RevealTextPresentation.compatible(step(3,3),step(4,4)));
    }
    @Test public void finalLettersDoNotWaitForBackgroundAnimationToStop()throws Exception{
        JSONObject latest=step(30,20).put("revealing",false);
        assertTrue(RevealTextPresentation.compatible(latest,step(28,19)));
        assertTrue(RevealTextPresentation.compatible(latest,step(29,20).put("revealing",false)));
    }
}
