package org.mahoyo.port;

import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;
import org.json.*;
import org.junit.*;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;

public class SaveArchiveTest {
    @Rule public TemporaryFolder temporary=new TemporaryFolder();
    private File directory(String name)throws IOException {File file=new File(temporary.getRoot(),name);file.mkdirs();return file;}
    private File put(File parent,String name,byte[] bytes)throws IOException {parent.mkdirs();File file=new File(parent,name);Files.write(file.toPath(),bytes);return file;}
    static byte[] save(int marker){ByteBuffer b=ByteBuffer.allocate(52).order(ByteOrder.LITTLE_ENDIAN);b.put("WOHSAVE2".getBytes(java.nio.charset.StandardCharsets.US_ASCII)).putInt(36).putInt(0).putInt(4).putInt(marker);for(int i=0;i<7;i++)b.putInt(0);CRC32 crc=new CRC32();crc.update(b.array(),16,36);b.putInt(12,(int)crc.getValue());return b.array();}
    private byte[] zip(SaveArchive.Archive archive)throws Exception {ByteArrayOutputStream output=new ByteArrayOutputStream();SaveArchive.write(archive,output,"0.6.12-test");return output.toByteArray();}
    private SaveArchive.Archive input(byte[] bytes)throws Exception {return SaveArchive.read(new ByteArrayInputStream(bytes),directory("work"));}
    @Test public void allocationReservesDesiredFreeSlotsAndWrapsAround(){
        TreeMap<Integer,Integer> plan=SaveArchive.allocate(new HashSet<>(Arrays.asList(1,2)),Collections.singleton(1));assertEquals(Integer.valueOf(3),plan.get(1));assertEquals(Integer.valueOf(2),plan.get(2));
        assertEquals(Integer.valueOf(1),SaveArchive.allocate(Collections.singleton(12),Collections.singleton(12)).get(12));
        Random random=new Random(527);for(int n=0;n<3000;n++){Set<Integer> incoming=new HashSet<>(),occupied=new HashSet<>();for(int i=1;i<=12;i++){if(random.nextBoolean())incoming.add(i);if(random.nextBoolean())occupied.add(i);}Map<Integer,Integer> mapped=SaveArchive.allocate(incoming,occupied);assertEquals(incoming.size(),mapped.size());assertEquals(mapped.size(),new HashSet<>(mapped.values()).size());assertEquals(Math.max(0,incoming.size()-(12-occupied.size())),mapped.values().stream().filter(occupied::contains).count());for(int source:incoming)if(!occupied.contains(source))assertEquals(Integer.valueOf(source),mapped.get(source));}
    }
    @Test public void roundTripRemapsSlotsAndMergesCommonDataWithoutChangingExistingSlots()throws Exception {
        File source=directory("source"),target=directory("target"),thumbs=directory("thumbs"),journal=directory("journal");byte[] existing=save(71),incoming=save(22);put(source,"slot1.whs",incoming);put(target,"slot1.whs",existing);
        put(source,"trophies.whp",SaveCommonTest.text("WOHTROPHY1 1\n2\n"));put(target,"trophies.whp",SaveCommonTest.text("WOHTROPHY1 1\n1\n"));
        try(SaveArchive.Archive snapshot=SaveArchive.snapshot(source,directory("source-thumbs"),directory("snapshot"));SaveArchive.Archive archive=input(zip(snapshot))){SaveArchive.Plan plan=SaveArchive.plan(archive,target,thumbs);assertTrue(plan.overwrites.isEmpty());assertEquals(Integer.valueOf(2),plan.slots.get(1));SaveArchive.apply(plan,journal,false,()->{});}
        assertArrayEquals(existing,Files.readAllBytes(new File(target,"slot1.whs").toPath()));assertArrayEquals(incoming,Files.readAllBytes(new File(target,"slot2.whs").toPath()));assertEquals("WOHTROPHY1 2\n1\n2\n",new String(SaveCommon.payload(Files.readAllBytes(new File(target,"trophies.whp").toPath())),java.nio.charset.StandardCharsets.UTF_8));assertFalse(new File(journal,"transaction.json").exists());
    }
    @Test public void fullSlotsRequireExplicitOverwriteApproval()throws Exception {
        File source=directory("source"),target=directory("target"),thumbs=directory("thumbs");put(source,"slot3.whs",save(99));for(int i=1;i<=12;i++)put(target,"slot"+i+".whs",save(i));
        try(SaveArchive.Archive archive=SaveArchive.snapshot(source,directory("source-thumbs"),directory("work"))){SaveArchive.Plan plan=SaveArchive.plan(archive,target,thumbs);assertEquals(Collections.singletonList(3),plan.overwrites);try{SaveArchive.apply(plan,directory("journal"),false,()->{});fail();}catch(IOException expected){}assertArrayEquals(save(3),Files.readAllBytes(new File(target,"slot3.whs").toPath()));SaveArchive.apply(plan,directory("journal"),true,()->{});assertArrayEquals(save(99),Files.readAllBytes(new File(target,"slot3.whs").toPath()));}
    }
    @Test public void failedApplyRollsBackBothReplacedAndNewFiles()throws Exception {
        File source=directory("source"),target=directory("target"),thumbs=directory("thumbs"),journal=directory("journal");put(source,"slot2.whs",save(22));put(source,"trophies.whp",SaveCommonTest.text("WOHTROPHY1 1\n2\n"));byte[] previous=SaveCommonTest.text("WOHTROPHY1 1\n1\n");put(target,"trophies.whp",previous);
        try(SaveArchive.Archive archive=SaveArchive.snapshot(source,directory("source-thumbs"),directory("work"))){try{SaveArchive.apply(SaveArchive.plan(archive,target,thumbs),journal,false,()->{throw new IOException("Injected failure");});fail();}catch(IOException expected){}}
        assertFalse(new File(target,"slot2.whs").exists());assertArrayEquals(previous,Files.readAllBytes(new File(target,"trophies.whp").toPath()));assertEquals(Collections.singletonList("trophies.whp"),Arrays.asList(target.list()));assertFalse(new File(journal,"transaction.json").exists());
    }
    @Test public void detectsChangesSinceConfirmationBeforeAnyMutation()throws Exception {
        File source=directory("source"),target=directory("target"),thumbs=directory("thumbs");put(source,"slot1.whs",save(1));put(target,"slot1.whs",save(2));
        try(SaveArchive.Archive archive=SaveArchive.snapshot(source,directory("source-thumbs"),directory("work"))){SaveArchive.Plan plan=SaveArchive.plan(archive,target,thumbs);put(target,"slot1.whs",save(3));try{SaveArchive.apply(plan,directory("journal"),false,()->{});fail();}catch(IOException expected){}assertArrayEquals(save(3),Files.readAllBytes(new File(target,"slot1.whs").toPath()));assertFalse(new File(target,"slot2.whs").exists());}
    }
    @Test public void stagingFailureDoesNotStrandPartialSaveCopies()throws Exception {
        File source=directory("source"),target=directory("target"),thumbs=directory("thumbs"),journal=directory("journal");put(source,"slot1.whs",save(1));put(source,"slot2.whs",save(2));put(target,"slot1.whs",save(9));
        try(SaveArchive.Archive archive=SaveArchive.snapshot(source,directory("source-thumbs"),directory("work"))){SaveArchive.Plan plan=SaveArchive.plan(archive,target,thumbs);Files.delete(archive.files.get("saves/slot2.whs").toPath());try{SaveArchive.apply(plan,journal,false,()->{});fail();}catch(IOException expected){}}
        assertEquals(Collections.singletonList("slot1.whs"),Arrays.asList(target.list()));assertArrayEquals(save(9),Files.readAllBytes(new File(target,"slot1.whs").toPath()));assertFalse(new File(journal,"transaction.json").exists());
    }
    @Test public void interruptedTransactionRecoversOnTheNextLaunch()throws Exception {
        File target=directory("target"),thumbs=directory("thumbs"),journal=directory("journal");String id=UUID.randomUUID().toString();put(target,"slot1.whs",save(99));put(target,".slot1.whs-"+id+".old",save(1));put(target,"slot2.whs",save(88));
        JSONArray operations=new JSONArray().put(new JSONObject().put("kind","save").put("name","slot1.whs").put("id",id).put("old",true).put("new",true)).put(new JSONObject().put("kind","save").put("name","slot2.whs").put("id",id).put("old",false).put("new",true));
        put(journal,"transaction.json",new JSONObject().put("schema",1).put("committed",false).put("operations",operations).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));SaveArchive.recover(target,thumbs,journal);assertArrayEquals(save(1),Files.readAllBytes(new File(target,"slot1.whs").toPath()));assertFalse(new File(target,"slot2.whs").exists());
    }
    @Test public void committedRecoveryKeepsNewFilesAndOnlyRemovesBackups()throws Exception {
        File target=directory("target"),thumbs=directory("thumbs"),journal=directory("journal");String id=UUID.randomUUID().toString();put(target,"slot1.whs",save(99));File backup=put(target,".slot1.whs-"+id+".old",save(1));JSONArray rows=new JSONArray().put(new JSONObject().put("kind","save").put("name","slot1.whs").put("id",id).put("old",true).put("new",true));put(journal,"transaction.json",new JSONObject().put("schema",1).put("committed",true).put("operations",rows).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));SaveArchive.recover(target,thumbs,journal);assertArrayEquals(save(99),Files.readAllBytes(new File(target,"slot1.whs").toPath()));assertFalse(backup.exists());assertFalse(new File(journal,"transaction.json").exists());
    }
    @Test public void thumbnailBindingSurvivesSlotRenumberingAndDatesArePreserved()throws Exception {
        File source=directory("source"),sourceThumbs=directory("source-thumbs"),target=directory("target"),thumbs=directory("thumbs");File original=put(source,"slot1.whs",save(1));assertTrue(original.setLastModified(1700000000123L));SlotSaveMetadata meta=SlotSaveMetadata.read(original);ByteArrayOutputStream sidecar=new ByteArrayOutputStream();try(DataOutputStream out=new DataOutputStream(sidecar)){byte[] json=new JSONObject().put("sha256",meta.sha256).put("size",meta.size).put("modified",meta.modified).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);out.writeInt(0x57485431);out.writeInt(json.length);out.write(json);out.writeInt(1);out.writeByte(0);}put(sourceThumbs,"slot1.thumb",sidecar.toByteArray());put(target,"slot1.whs",save(2));
        try(SaveArchive.Archive snapshot=SaveArchive.snapshot(source,sourceThumbs,directory("work"));SaveArchive.Archive archive=input(zip(snapshot))){SaveArchive.Plan plan=SaveArchive.plan(archive,target,thumbs);assertEquals(Integer.valueOf(2),plan.slots.get(1));SaveArchive.apply(plan,directory("journal"),false,()->{});}
        assertArrayEquals(sidecar.toByteArray(),Files.readAllBytes(new File(thumbs,"slot2.thumb").toPath()));assertEquals(meta.modified,new File(target,"slot2.whs").lastModified());assertEquals(meta.sha256,SlotSaveMetadata.read(new File(target,"slot2.whs")).sha256);
    }
    @Test public void rejectsTraversalUnknownEntriesAndBadChecksumsWithoutTouchingSaves()throws Exception {
        for(String entry:new String[]{"../slot1.whs","/saves/slot1.whs","saves\\slot1.whs","saves/slot13.whs","secrets.txt"}){ByteArrayOutputStream buffer=new ByteArrayOutputStream();try(ZipOutputStream out=new ZipOutputStream(buffer)){out.putNextEntry(new ZipEntry(entry));out.write(save(1));out.closeEntry();}try(SaveArchive.Archive ignored=input(buffer.toByteArray())){fail(entry);}catch(IOException expected){}}
        File source=directory("source");put(source,"slot1.whs",save(1));try(SaveArchive.Archive archive=SaveArchive.snapshot(source,directory("thumbs"),directory("work"))){byte[] good=zip(archive);ByteArrayOutputStream bad=new ByteArrayOutputStream();try(ZipInputStream in=new ZipInputStream(new ByteArrayInputStream(good));ZipOutputStream out=new ZipOutputStream(bad)){ZipEntry entry;while((entry=in.getNextEntry())!=null){out.putNextEntry(new ZipEntry(entry.getName()));byte[] bytes=in.readAllBytes();if(entry.getName().endsWith(".whs"))bytes[bytes.length-1]^=1;out.write(bytes);out.closeEntry();}}try(SaveArchive.Archive ignored=input(bad.toByteArray())){fail();}catch(IOException expected){}}
    }
}
