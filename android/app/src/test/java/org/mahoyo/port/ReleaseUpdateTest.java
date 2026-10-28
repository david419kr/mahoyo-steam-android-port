package org.mahoyo.port;

import java.io.IOException;
import org.json.*;
import org.junit.Test;
import static org.junit.Assert.*;

public class ReleaseUpdateTest {
    private JSONObject release(String tag)throws Exception {return new JSONObject().put("tag_name",tag).put("draft",false).put("prerelease",false).put("assets",new JSONArray().put(new JSONObject().put("name","mahoyo-unofficial.apk").put("size",37000000).put("digest","sha256:"+"a".repeat(64)).put("browser_download_url","https://github.com/"+ReleaseUpdate.REPOSITORY+"/releases/download/"+tag+"/mahoyo-unofficial.apk")));}
    private void rejects(JSONObject json)throws Exception {try{ReleaseUpdate.parse(json.toString());fail("Invalid release accepted");}catch(IOException expected){}}
    @Test public void numericVersionsAndStableAfterLocalTest()throws Exception {
        ReleaseUpdate update=ReleaseUpdate.parse(release("v0.6.12").toString());assertTrue(update.newerThan("0.6.9"));assertTrue(update.newerThan("0.6.11"));assertTrue(update.newerThan("0.6.12-test"));assertFalse(update.newerThan("0.6.12"));assertFalse(update.newerThan("0.7.0"));assertEquals("0.6.12",update.version);
        assertTrue(ReleaseUpdate.parse(release("v1.0.0").toString()).newerThan("0.99.999"));
    }
    @Test public void rejectsDraftsPrereleasesUnknownTagsAndMissingApk()throws Exception {
        rejects(release("v0.6.12").put("draft",true));rejects(release("v0.6.12").put("prerelease",true));rejects(release("v0.6.12-test"));rejects(release("v0.6"));rejects(release("latest"));rejects(release("../v0.6.12"));rejects(release("v0.6.12").put("assets",new JSONArray()));
    }
    @Test public void onlyTheExactRepositoryAndAssetMayBeDownloaded()throws Exception {
        for(String url:new String[]{"http://github.com/"+ReleaseUpdate.REPOSITORY+"/releases/download/v0.6.12/mahoyo-unofficial.apk","https://example.com/update.apk","https://github.com/other/repo/releases/download/v0.6.12/mahoyo-unofficial.apk","https://github.com/"+ReleaseUpdate.REPOSITORY+"/releases/download/v0.6.12/mahoyo-unofficial.apk?other=1"}){
            JSONObject row=release("v0.6.12");row.getJSONArray("assets").getJSONObject(0).put("browser_download_url",url);rejects(row);
        }
    }
    @Test public void validatesAssetSizeDigestAndDuplicates()throws Exception {
        for(long size:new long[]{0,-1,256L*1024*1024+1}){JSONObject row=release("v0.6.12");row.getJSONArray("assets").getJSONObject(0).put("size",size);rejects(row);}
        JSONObject row=release("v0.6.12");row.getJSONArray("assets").getJSONObject(0).put("digest","sha256:bad");rejects(row);
        row=release("v0.6.12");row.getJSONArray("assets").put(row.getJSONArray("assets").get(0));rejects(row);
        row=release("v0.6.12");row.getJSONArray("assets").getJSONObject(0).remove("digest");assertNotNull(ReleaseUpdate.parse(row.toString())); // Installed certificate is still mandatory.
    }
}
