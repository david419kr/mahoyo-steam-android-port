package org.mahoyo.port;

import java.io.*;
import java.nio.file.*;
import org.junit.*;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;

public class SlotDeletionTest {
    @Rule public TemporaryFolder temporary=new TemporaryFolder();
    private File directory(String name){File file=new File(temporary.getRoot(),name);file.mkdirs();return file;}
    private File write(File parent,String name,byte[] bytes)throws IOException {File file=new File(parent,name);Files.write(file.toPath(),bytes);return file;}
    @Test public void removesOnlyTheSelectedSlotAndItsThumbnail()throws Exception {
        File saves=directory("saves"),thumbs=directory("thumbs"),journal=directory("journal");File selected=write(saves,"slot2.whs",SaveArchiveTest.save(22));write(saves,"slot1.whs",SaveArchiveTest.save(11));write(saves,"slot3.whs",SaveArchiveTest.save(33));write(thumbs,"slot2.thumb",new byte[]{2});write(thumbs,"slot1.thumb",new byte[]{1});
        byte[] common=SaveCommonTest.shared(4,new int[]{3},new byte[]{1}),images=SaveCommonTest.text("WOHSEEN1\n\"img_test.mzp\"\n"),awards=SaveCommonTest.text("WOHTROPHY1 1\n1\n");write(saves,"shared.whp",common);write(saves,"seen-images.whp",images);write(saves,"trophies.whp",awards);
        SaveArchive.Plan plan=SaveArchive.deletion(saves,thumbs,2,selected.lastModified(),selected.length(),SaveArchive.hash(selected));SaveArchive.apply(plan,journal,false,()->{});
        assertFalse(selected.exists());assertFalse(new File(thumbs,"slot2.thumb").exists());assertArrayEquals(SaveArchiveTest.save(11),Files.readAllBytes(new File(saves,"slot1.whs").toPath()));assertArrayEquals(SaveArchiveTest.save(33),Files.readAllBytes(new File(saves,"slot3.whs").toPath()));assertArrayEquals(new byte[]{1},Files.readAllBytes(new File(thumbs,"slot1.thumb").toPath()));
        assertArrayEquals(common,Files.readAllBytes(new File(saves,"shared.whp").toPath()));assertArrayEquals(images,Files.readAllBytes(new File(saves,"seen-images.whp").toPath()));assertArrayEquals(awards,Files.readAllBytes(new File(saves,"trophies.whp").toPath()));assertEquals(0,journal.list().length);
    }
    @Test public void failedDeletionRestoresTheSameSlotAndThumbnail()throws Exception {
        File saves=directory("saves"),thumbs=directory("thumbs"),journal=directory("journal"),selected=write(saves,"slot2.whs",SaveArchiveTest.save(22));write(thumbs,"slot2.thumb",new byte[]{2});
        SaveArchive.Plan plan=SaveArchive.deletion(saves,thumbs,2,selected.lastModified(),selected.length(),SaveArchive.hash(selected));try{SaveArchive.apply(plan,journal,false,()->{throw new IOException("Injected failure");});fail();}catch(IOException expected){}
        assertArrayEquals(SaveArchiveTest.save(22),Files.readAllBytes(selected.toPath()));assertArrayEquals(new byte[]{2},Files.readAllBytes(new File(thumbs,"slot2.thumb").toPath()));assertEquals(1,saves.list().length);assertEquals(1,thumbs.list().length);assertEquals(0,journal.list().length);
    }
    @Test public void rejectsInvalidMissingOrChangedSelections()throws Exception {
        File saves=directory("saves"),thumbs=directory("thumbs"),selected=write(saves,"slot2.whs",SaveArchiveTest.save(22));long stamp=selected.lastModified(),size=selected.length();String hash=SaveArchive.hash(selected);
        for(int slot:new int[]{-1,0,13,Integer.MAX_VALUE})try{SaveArchive.deletion(saves,thumbs,slot,stamp,size,hash);fail();}catch(IllegalArgumentException expected){}
        try{SaveArchive.deletion(saves,thumbs,1,stamp,size,hash);fail();}catch(IOException expected){}
        write(saves,"slot2.whs",SaveArchiveTest.save(23));assertTrue(selected.setLastModified(stamp));try{SaveArchive.deletion(saves,thumbs,2,stamp,size,hash);fail();}catch(IOException expected){}assertArrayEquals(SaveArchiveTest.save(23),Files.readAllBytes(selected.toPath()));
    }
    @Test public void aCorruptSlotCanBeRemovedWithoutDeletingOtherData()throws Exception {
        File saves=directory("saves"),thumbs=directory("thumbs"),selected=write(saves,"slot2.whs",new byte[]{1,2,3});SaveArchive.apply(SaveArchive.deletion(saves,thumbs,2,selected.lastModified(),selected.length(),""),directory("journal"),false,()->{});assertFalse(selected.exists());
    }
}
