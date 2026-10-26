package org.mahoyo.port;
import org.json.*;
import org.junit.Test;
import static org.junit.Assert.*;
public final class HistoryRewindModelTest {
 private HistoryModel.Group group(boolean known,boolean available,boolean ended,boolean voice)throws Exception{
  JSONObject row=new JSONObject().put("index",0).put("metadataKnown",known).put("rewindAvailable",available).put("endsGroup",ended).put("text","SYNTHETIC");
  if(voice)row.put("voices",new JSONArray().put(new JSONObject().put("active",true).put("name","synthetic.hw").put("backend",0).put("available",false)));
  return HistoryModel.parse(new JSONObject().put("historyRevision","123").put("history",new JSONArray().put(row))).groups.get(0);
 }
 @Test public void silentHistoryCanRewindIndependentlyOfVoiceReplay()throws Exception{
  HistoryModel.Group row=group(true,true,true,false);assertTrue(row.rewindable);assertFalse(row.playable);assertEquals("123 0 1",row.payload("123"));
 }
 @Test public void unavailableVoiceDoesNotDisableValidSceneRestore()throws Exception{
  HistoryModel.Group row=group(true,true,true,true);assertTrue(row.rewindable);assertFalse(row.playable);
 }
 @Test public void unknownLegacyAndPartialGroupsCannotRewind()throws Exception{
  assertFalse(group(false,true,true,false).rewindable);assertFalse(group(true,false,true,false).rewindable);assertFalse(group(true,true,false,false).rewindable);
 }
}
