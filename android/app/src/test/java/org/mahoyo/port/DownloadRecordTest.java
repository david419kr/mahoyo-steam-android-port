package org.mahoyo.port;
import org.junit.Test;import static org.junit.Assert.*;import org.json.JSONObject;
public class DownloadRecordTest {
 private static final String SOURCE="steam-download-11111111-2222-3333-4444-555555555555";
 @Test public void interruptedDownloadsResumeButActiveFilesCannotBeTargets()throws Exception{JSONObject saved=DownloadRecord.create(SOURCE);assertEquals(SOURCE,DownloadRecord.reusable(new JSONObject(saved.toString()),"steam-other"));assertNull(DownloadRecord.reusable(saved,SOURCE));}
 @Test public void corruptAndUnsafeCheckpointsCannotSelectFiles()throws Exception{assertNull(DownloadRecord.reusable(null,""));for(String source:new String[]{"../steam","steam","/sdcard/game",SOURCE+"/child"}){JSONObject saved=new JSONObject().put("schema",1).put("appId",2052410).put("source",source);assertNull(DownloadRecord.reusable(saved,""));}JSONObject saved=DownloadRecord.create(SOURCE);saved.put("appId",1);assertNull(DownloadRecord.reusable(saved,""));}
}
