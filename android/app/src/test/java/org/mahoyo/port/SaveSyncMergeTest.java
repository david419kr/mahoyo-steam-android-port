package org.mahoyo.port;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import org.junit.*;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;

public class SaveSyncMergeTest {
    @Rule public TemporaryFolder temporary=new TemporaryFolder();
    private File dir(String name){File f=new File(temporary.getRoot(),name);f.mkdirs();return f;}
    private File put(File parent,String name,byte[] bytes,long time)throws Exception{File f=new File(parent,name);Files.write(f.toPath(),bytes);assertTrue(f.setLastModified(time));return f;}
    @Test public void selectsNewestSlotsAndUnionsCommonProgressSymmetrically()throws Exception{
        File a=dir("a"),b=dir("b");put(a,"slot1.whs",SaveArchiveTest.save(1),10000);put(b,"slot1.whs",SaveArchiveTest.save(2),20000);put(a,"slot2.whs",SaveArchiveTest.save(3),30000);put(b,"slot3.whs",SaveArchiveTest.save(4),40000);
        put(a,"trophies.whp",SaveCommonTest.text("WOHTROPHY1 1\n1\n"),10000);put(b,"trophies.whp",SaveCommonTest.text("WOHTROPHY1 1\n2\n"),20000);
        try(SaveArchive.Archive left=SaveSyncMerge.snapshot(a,dir("at"),dir("work"));SaveArchive.Archive right=SaveSyncMerge.snapshot(b,dir("bt"),dir("work"));SaveArchive.Archive merged=SaveSyncMerge.merge(left,right,dir("work"));SaveArchive.Archive reverse=SaveSyncMerge.merge(right,left,dir("work"))){
            assertEquals(SaveSyncMerge.fingerprint(merged),SaveSyncMerge.fingerprint(reverse));assertArrayEquals(SaveArchiveTest.save(2),Files.readAllBytes(merged.files.get("saves/slot1.whs").toPath()));assertEquals(Set.of(1,2,3),merged.slots());
            assertArrayEquals(SaveCommonTest.text("WOHTROPHY1 2\n1\n2\n"),Files.readAllBytes(merged.files.get("saves/trophies.whp").toPath()));
            SaveArchive.apply(SaveArchive.exactPlan(merged,a,dir("at")),dir("aj"),true,()->{});SaveArchive.apply(SaveArchive.exactPlan(merged,b,dir("bt")),dir("bj"),true,()->{});
        }
        try(SaveArchive.Archive left=SaveSyncMerge.snapshot(a,dir("at"),dir("work"));SaveArchive.Archive right=SaveSyncMerge.snapshot(b,dir("bt"),dir("work"));SaveArchive.Archive merged=SaveSyncMerge.merge(left,right,dir("work"))){assertEquals(SaveSyncMerge.fingerprint(left),SaveSyncMerge.fingerprint(right));assertEquals(SaveSyncMerge.fingerprint(left),SaveSyncMerge.fingerprint(merged));assertTrue(SaveArchive.exactPlan(merged,a,dir("at")).replacements.isEmpty());}
    }
    @Test public void emptySnapshotsWorkOnlyForExplicitSyncAndDoNotDeleteSlots()throws Exception{
        try(SaveArchive.Archive empty=SaveSyncMerge.snapshot(dir("empty"),dir("et"),dir("work"))){ByteArrayOutputStream bytes=new ByteArrayOutputStream();SaveArchive.write(empty,bytes,"sync");try(SaveArchive.Archive read=SaveArchive.read(new ByteArrayInputStream(bytes.toByteArray()),dir("work"),true)){assertTrue(read.files.isEmpty());}try(SaveArchive.Archive ignored=SaveArchive.read(new ByteArrayInputStream(bytes.toByteArray()),dir("work"))){fail();}catch(IOException expected){}
            File target=dir("target");put(target,"slot1.whs",SaveArchiveTest.save(1),10000);SaveArchive.apply(SaveArchive.exactPlan(empty,target,dir("tt")),dir("journal"),true,()->{});assertTrue(new File(target,"slot1.whs").isFile());
        }
    }
    @Test public void equalTimeConflictsAreStableAndFailedImportRollsBack()throws Exception{
        File a=dir("a"),b=dir("b");put(a,"slot1.whs",SaveArchiveTest.save(1),10000);put(b,"slot1.whs",SaveArchiveTest.save(2),10000);
        try(SaveArchive.Archive left=SaveSyncMerge.snapshot(a,dir("at"),dir("work"));SaveArchive.Archive right=SaveSyncMerge.snapshot(b,dir("bt"),dir("work"));SaveArchive.Archive merged=SaveSyncMerge.merge(left,right,dir("work"));SaveArchive.Archive reverse=SaveSyncMerge.merge(right,left,dir("work"))){assertEquals(SaveSyncMerge.fingerprint(merged),SaveSyncMerge.fingerprint(reverse));File target=dir("target");put(target,"slot1.whs",SaveArchiveTest.save(9),10000);try{SaveArchive.apply(SaveArchive.exactPlan(merged,target,dir("tt")),dir("journal"),true,()->{throw new IOException("injected");});fail();}catch(IOException expected){}assertArrayEquals(SaveArchiveTest.save(9),Files.readAllBytes(new File(target,"slot1.whs").toPath()));}
    }
}
