"""Actual installation/checkpoint filesystem logic with minimal Android service stubs.

This does not emulate Android lifecycle, SELinux or its AtomicFile implementation.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
JAVA=ROOT/'android/app/src/main/java/org/mahoyo/port'


class InstallationStorageTest(unittest.TestCase):
    def test_restart_resume_and_activation_never_select_active_media(self):
        home=Path(os.environ.get('JAVA_HOME',''))
        javac=shutil.which('javac') or str(home/'bin/javac.exe')
        java=shutil.which('java') or str(home/'bin/java.exe')
        jars=list((Path.home()/'.gradle/caches/modules-2/files-2.1/org.json/json/20260719').glob('*/*.jar'))
        if not Path(javac).is_file() or not jars:
            self.skipTest('JDK and JSON dependency from Android installer unit tests required')
        sources={
            'android/content/Context.java':r'''package android.content;import java.io.*;
public class Context {public static final int MODE_PRIVATE=0;final File root;public Context(File f){root=f;}
public Context getApplicationContext(){return this;}public File getFilesDir(){File f=new File(root,"internal");f.mkdirs();return f;}
public File getExternalFilesDir(String ignored){File f=new File(root,"external");f.mkdirs();return f;}
public Prefs getSharedPreferences(String key,int mode){return new Prefs();}public static class Prefs{public Prefs edit(){return this;}public Prefs putString(String key,String value){return this;}public boolean commit(){return true;}}}''',
            'android/util/AtomicFile.java':r'''package android.util;import java.io.*;import java.nio.file.*;
public class AtomicFile{
public static volatile java.util.concurrent.CountDownLatch writeStarted,releaseWrite;
private static volatile Thread writer;
final File file;public AtomicFile(File f){file=f;}public File getBaseFile(){return file;}
public FileInputStream openRead()throws IOException{if(writer!=null&&writer!=Thread.currentThread())throw new IOException("Concurrent AtomicFile access across Installation instances");return new FileInputStream(file);}
public FileOutputStream startWrite()throws IOException{if(writer!=null)throw new IOException("Concurrent AtomicFile writers");writer=Thread.currentThread();if(writeStarted!=null){writeStarted.countDown();try{if(!releaseWrite.await(3,java.util.concurrent.TimeUnit.SECONDS))throw new IOException("Writer test timeout");}catch(InterruptedException e){Thread.currentThread().interrupt();throw new IOException(e);}}return new FileOutputStream(new File(file+".new"));}
public void finishWrite(FileOutputStream out)throws IOException{out.close();Files.move(new File(file+".new").toPath(),file.toPath(),StandardCopyOption.REPLACE_EXISTING);writer=null;}
public void failWrite(FileOutputStream out){try{out.close();}catch(IOException ignored){}new File(file+".new").delete();writer=null;}public void delete(){file.delete();}}''',
            'android/system/StructStat.java':'package android.system;public class StructStat{public long st_blocks;}',
            'android/system/Os.java':'package android.system;public class Os{public static StructStat stat(String path){throw new UnsupportedOperationException("Not a filesystem allocation test");}}',
            'org/mahoyo/port/SteamResources.java':r'''package org.mahoyo.port;import android.content.Context;import org.json.*;import java.io.*;
class SteamResources{static final String MARKER=".steam-ready";static byte[] read(InputStream input,int cap)throws IOException{try(InputStream in=input){byte[] bytes=in.readNBytes(cap+1);if(bytes.length>cap)throw new IOException("budget");return bytes;}}
static boolean available(File root){return new File(root,MARKER).isFile();}static boolean sourceAvailable(Context c,File root){throw new UnsupportedOperationException("Not a media verification test");}
static JSONObject assetJson(Context c,String name,int cap){return new JSONObject().put("archives",new JSONArray().put(new JSONObject().put("name","data00000.hfa").put("bytes",128L*1024*1024)));}}''',
            'org/mahoyo/port/InstallationFsCheck.java':r'''package org.mahoyo.port;import android.content.Context;import org.json.*;import java.io.*;import java.nio.file.*;
public class InstallationFsCheck{
static void check(boolean value){if(!value)throw new AssertionError();}static Context context(File root){return new Context(root);}
public static void main(String[]args)throws Exception{File base=new File(args[0]);Installation first=new Installation(context(base));first.confirmOwnership("76561198000000000");
File source=first.downloadSource(),media=new File(source,"data00000.hfa");Files.write(media.toPath(),new byte[]{42});
Installation restart=new Installation(context(base));check(restart.downloadSource().equals(source));
restart.preservePartialSteamFiles(source);check(media.length()==128L*1024*1024);try(RandomAccessFile file=new RandomAccessFile(media,"r")){check(file.read()==42);file.seek(file.length()-1);check(file.read()==0);}
File game=restart.child("game-steam-11111111-2222-3333-4444-555555555555");game.mkdirs();Files.writeString(new File(game,"assets.json").toPath(),"{}");File marker=new File(game,SteamResources.MARKER);Files.writeString(marker.toPath(),new JSONObject().put("schema",1).put("source",source.getPath()).toString());
File save=new File(game.getParentFile(),"savedata/slot1.whs");save.getParentFile().mkdirs();Files.write(save.toPath(),new byte[]{77});restart.activate(game,null,"vanilla","steam");
check(!new File(context(base).getFilesDir(),"download.json").exists());
Installation after=new Installation(context(base));check(after.ready());check(after.dataRoot(after.read()).equals(game));File second=after.downloadSource();check(!second.equals(source));
File checkpoint=new File(context(base).getFilesDir(),"download.json");Files.writeString(checkpoint.toPath(),DownloadRecord.create(source.getName()).toString());check(!new Installation(context(base)).downloadSource().equals(source));
Files.writeString(checkpoint.toPath(),new JSONObject().put("schema",1).put("appId",2052410).put("source","../outside").toString());File recovered=new Installation(context(base)).downloadSource();check(recovered.getCanonicalFile().getParentFile().equals(source.getParentFile()));
Files.writeString(marker.toPath(),"corrupt marker");Files.writeString(checkpoint.toPath(),DownloadRecord.create(second.getName()).toString());check(!new Installation(context(base)).downloadSource().equals(second));
try(FileInputStream file=new FileInputStream(media)){check(file.read()==42);}check(Files.readAllBytes(save.toPath())[0]==77);check(new Installation(context(base)).ready());
android.util.AtomicFile.writeStarted=new java.util.concurrent.CountDownLatch(1);android.util.AtomicFile.releaseWrite=new java.util.concurrent.CountDownLatch(1);
java.util.concurrent.ExecutorService pool=java.util.concurrent.Executors.newFixedThreadPool(2);
try{
 java.util.concurrent.Future<?> writing=pool.submit(()->{try{new Installation(context(base)).confirmOwnership("76561198000000001");}catch(Exception e){throw new RuntimeException(e);}});
 check(android.util.AtomicFile.writeStarted.await(2,java.util.concurrent.TimeUnit.SECONDS));
 java.util.concurrent.CountDownLatch readAttempted=new java.util.concurrent.CountDownLatch(1);
 java.util.concurrent.Future<JSONObject> reading=pool.submit(()->{readAttempted.countDown();return new Installation(context(base)).read();});
 check(readAttempted.await(2,java.util.concurrent.TimeUnit.SECONDS));
 try{reading.get(150,java.util.concurrent.TimeUnit.MILLISECONDS);throw new AssertionError("A different Installation instance read during the write");}
 catch(java.util.concurrent.TimeoutException expected){}
 finally{android.util.AtomicFile.releaseWrite.countDown();}
 writing.get(2,java.util.concurrent.TimeUnit.SECONDS);check(reading.get(2,java.util.concurrent.TimeUnit.SECONDS).optBoolean("ready"));
}finally{android.util.AtomicFile.releaseWrite.countDown();pool.shutdownNow();android.util.AtomicFile.writeStarted=null;android.util.AtomicFile.releaseWrite=null;}
System.out.println("Cross-instance AtomicFile mutual exclusion passed");System.out.println("Installation restart, interrupted resume, activation, stale/corrupt checkpoints and active-file preservation passed");}}
'''}
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name,text in sources.items():
                path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text,encoding='utf-8')
            classes=root/'classes'
            subprocess.run([javac,'-encoding','UTF-8','-cp',str(jars[0]),'-d',str(classes),
                *(str(root/name) for name in sources),*(str(JAVA/name) for name in ('Installation.java','InstallationRecord.java','DownloadRecord.java'))],check=True)
            subprocess.run([java,'-cp',str(classes)+os.pathsep+str(jars[0]),'org.mahoyo.port.InstallationFsCheck',str(root/'state')],check=True)


if __name__=='__main__':unittest.main()
