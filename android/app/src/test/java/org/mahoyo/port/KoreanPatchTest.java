package org.mahoyo.port;
import org.junit.*;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

public class KoreanPatchTest {
 @Rule public TemporaryFolder temp=new TemporaryFolder();
 private static final String[] NAMES={"mahoyo_kr.dll","mahoyo_KR1.dat","mahoyo_KR2.dat","mahoyo_KR3.dat"};
 private JSONArray profile()throws Exception{JSONArray rows=new JSONArray();for(String name:NAMES){byte[] bytes=name.getBytes(StandardCharsets.UTF_8);StringBuilder sha=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(bytes))sha.append(String.format("%02x",b&255));rows.put(new JSONObject().put("name",name).put("bytes",bytes.length).put("sha256",sha.toString()));}return rows;}
 private byte[] zip(boolean duplicate,boolean damaged,boolean missing)throws Exception{ByteArrayOutputStream buffer=new ByteArrayOutputStream();try(ZipOutputStream out=new ZipOutputStream(buffer)){for(String name:NAMES){if(missing&&name.equals(NAMES[3]))continue;out.putNextEntry(new ZipEntry("random/tree/"+name.toUpperCase(Locale.ROOT)));out.write(name.getBytes(StandardCharsets.UTF_8));out.closeEntry();}if(duplicate){out.putNextEntry(new ZipEntry("another/"+NAMES[0]));out.write((damaged?"BAD":"mahoyo_kr.dll").getBytes(StandardCharsets.UTF_8));out.closeEntry();}out.putNextEntry(new ZipEntry("../../unrelated-readme.txt"));out.write("ignored".getBytes(StandardCharsets.UTF_8));out.closeEntry();}return buffer.toByteArray();}
 @Test public void treeIndependentWhitelistAndIdenticalDuplicates()throws Exception{File output=temp.newFolder();KoreanPatch.unpack(new ByteArrayInputStream(zip(true,false,false)),output,profile(),(d,t)->assertTrue(d<=t));assertEquals(4,Objects.requireNonNull(output.list()).length);for(String name:NAMES)assertArrayEquals(name.getBytes(StandardCharsets.UTF_8),Files.readAllBytes(new File(output,name).toPath()));assertFalse(new File(output.getParentFile(),"unrelated-readme.txt").exists());}
 @Test public void missingFilesAndConflictingDuplicatesFail()throws Exception{for(int n=0;n<2;n++)try{KoreanPatch.unpack(new ByteArrayInputStream(zip(n==0,true,n==1)),temp.newFolder(),profile(),(d,t)->{});fail("Incomplete or conflicting patch accepted");}catch(IOException expected){}}
 @Test public void wrongContentIsNotAcceptedByFilenameAlone()throws Exception{JSONArray rows=profile();rows.getJSONObject(0).put("sha256","0".repeat(64));try{KoreanPatch.unpack(new ByteArrayInputStream(zip(false,false,false)),temp.newFolder(),rows,(d,t)->{});fail();}catch(IOException expected){assertTrue(expected.getMessage().contains("corrupt"));}}
 @Test public void originalLanguagesRemainAndKoreanCsvIsQuoted()throws Exception{StringBuilder csv=new StringBuilder("\ufeff"),base=new StringBuilder("WOHMESSAGES1\n");for(int i=0;i<375;i++){String key=String.format("SYSMES_CONF_%03d",i);csv.append(key).append(",日本語,\"한국어, \"\"인용\"\"\",简体,繁體\r\n");base.append(key).append(" ja \"日本語\"\n");}byte[] raw=("MZ"+csv+"\0ignored").getBytes(StandardCharsets.UTF_8);String result=KoreanPatch.menuTable(raw,base.toString());assertTrue(result.startsWith(base.toString()));assertTrue(result.contains("SYSMES_CONF_000 ko \"한국어, \\\"인용\\\"\""));try{KoreanPatch.menuTable(raw,"WOHMESSAGES1\n");fail();}catch(IOException expected){}}
}
