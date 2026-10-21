"""Synthetic slot metadata and Surface/Text-only capture tests. No device save writes."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import zlib
import json
import shutil
import re

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'android/app/src/main/java/org/mahoyo/port'
OUT=ROOT/'build/tests/android-slot-check'


def blob(value):return struct.pack('<I',len(value))+value


def fixture(text,extra=b'WOHHOST16\nSCREENWAVE9 opaque future state\n'):
    body=blob(b'opaque VM')+struct.pack('<I',1)
    body+=struct.pack('<ii',0,1)+blob(b'fake.mzp')+blob(b'event')+blob(b'')+struct.pack('<5d',1,2,1920,1080,1)+struct.pack('<5I',0,0,1920,1080,0)
    body+=struct.pack('<I',1)+blob(b'fake.ogg')+blob(b'bgm')+struct.pack('<Id',1,.4)
    body+=struct.pack('<I',2)+blob('履歴'.encode())+blob(b'previous')+blob(text.encode())+struct.pack('<II',123,0)+blob(extra)
    return b'WOHSAVE2'+struct.pack('<II',len(body),zlib.crc32(body))+body


METADATA_TEST=r'''package org.mahoyo.port;
import java.io.*;import java.nio.file.*;import java.security.*;import java.util.*;
public final class SlotMetadataSelfTest {
static int checks;static void check(boolean value,String text){checks++;if(!value)throw new AssertionError(text);}
public static void main(String[] args)throws Exception{Path dir=Path.of(args[0]);File good=dir.resolve("slot12.whs").toFile();long stamp=good.lastModified(),size=good.length();byte[] before=Files.readAllBytes(good.toPath());SlotSaveMetadata meta=SlotSaveMetadata.read(good);check(meta.text.equals("日本語 ruby 😀 end"),"ruby/control preview preserves visible Unicode");check(meta.modified==stamp&&meta.size==size,"original timestamp/size metadata");check(meta.sha256.equals(SlotSaveMetadata.hex(MessageDigest.getInstance("SHA-256").digest(before))),"thumbnail binding uses complete WHS SHA");check(good.lastModified()==stamp&&Arrays.equals(before,Files.readAllBytes(good.toPath())),"metadata read never writes original slot12");
for(String name:new String[]{"bad-crc.whs","truncated.whs","bad-header.whs","oversize-field.whs","trailing.whs"}){try{SlotSaveMetadata.read(dir.resolve(name).toFile());throw new AssertionError("accepted "+name);}catch(IOException expected){checks++;}}
File changing=new File(good.getPath()){@Override public long lastModified(){return super.lastModified()+(calls++==0?0:1);}int calls;};try{SlotSaveMetadata.read(changing);throw new AssertionError("concurrent source replacement accepted");}catch(IOException expected){checks++;}
check(SlotSaveMetadata.read(dir.resolve("future.whs").toFile()).text.equals("future"),"VM and future hostExtra versions stay opaque");check(SlotSaveMetadata.read(dir.resolve("native.whs").toFile()).text.equals("日本語 ruby 😀 end"),"metadata parser matches actual C++ SaveStore output rather than only the synthetic encoder");String longPreview=SlotSaveMetadata.preview("😀".repeat(160));check(longPreview.codePointCount(0,longPreview.length())==141&&longPreview.endsWith("…"),"preview limit preserves supplementary Unicode");System.out.println("slot metadata: "+checks+" checks passed");}}
'''

STUBS={
'android/graphics/Rect.java':'''package android.graphics;public class Rect{public int left,top,right,bottom;public Rect(int l,int t,int r,int b){left=l;top=t;right=r;bottom=b;}public int width(){return right-left;}public int height(){return bottom-top;}}''',
'android/graphics/Bitmap.java':'''package android.graphics;public class Bitmap{public enum Config{ARGB_8888}public boolean recycled;public float drawnX,drawnY;public int draws;public static Bitmap last;public static Bitmap createBitmap(int w,int h,Config c){last=new Bitmap();return last;}public void recycle(){recycled=true;}}''',
'android/graphics/Canvas.java':'''package android.graphics;public class Canvas{public Bitmap bitmap;float sx=1,sy=1,tx,ty;public Canvas(Bitmap b){bitmap=b;}public void scale(float x,float y){sx*=x;sy*=y;}public void translate(float x,float y){tx+=sx*x;ty+=sy*y;}public void mark(float x,float y){bitmap.drawnX=sx*x+tx;bitmap.drawnY=sy*y+ty;bitmap.draws++;}}''',
'android/os/Looper.java':'''package android.os;public class Looper{public static Looper getMainLooper(){return new Looper();}}''',
'android/os/Handler.java':'''package android.os;import java.util.*;public class Handler{static List<Runnable> callbacks=new ArrayList<>();public Handler(Looper l){}public void postDelayed(Runnable r,long t){callbacks.add(r);}public void removeCallbacks(Runnable r){callbacks.remove(r);}public static void expire(){List<Runnable> copy=new ArrayList<>(callbacks);callbacks.clear();for(Runnable r:copy)r.run();}}''',
'android/view/View.java':'''package android.view;public class View{public static final int VISIBLE=0,GONE=8;public int x,y,width=2480,height=2248,visibility=0;public int getWidth(){return width;}public int getHeight(){return height;}public int getVisibility(){return visibility;}public void getLocationInWindow(int[] out){out[0]=x;out[1]=y;}}''',
'android/opengl/GLSurfaceView.java':'''package android.opengl;public class GLSurfaceView extends android.view.View{}''',
'android/view/PixelCopy.java':'''package android.view;import android.graphics.*;import android.opengl.*;import android.os.*;public class PixelCopy{public static final int SUCCESS=0;public interface OnPixelCopyFinishedListener{void onPixelCopyFinished(int result);}static OnPixelCopyFinishedListener pending;public static boolean fail;public static void request(GLSurfaceView s,Rect r,Bitmap b,OnPixelCopyFinishedListener c,Handler h){if(fail)throw new IllegalArgumentException();pending=c;}public static void complete(int result){OnPixelCopyFinishedListener c=pending;pending=null;c.onPixelCopyFinished(result);}}''',
'org/mahoyo/port/TextOverlay.java':'''package org.mahoyo.port;public class TextOverlay extends android.view.View{public boolean fail;public void draw(android.graphics.Canvas c){if(fail)throw new IllegalStateException("draw fail");c.mark(300,400);}}'''
}

STORE_STUBS=dict(STUBS)
STORE_STUBS['android/os/Handler.java']=STORE_STUBS['android/os/Handler.java'].replace('public void postDelayed','public void post(Runnable r){r.run();}public void postDelayed')
STORE_STUBS['android/graphics/Bitmap.java']='''package android.graphics;import java.io.*;import java.util.concurrent.*;public class Bitmap{public enum Config{ARGB_8888,RGB_565}public enum CompressFormat{JPEG}public float drawnX,drawnY;public int draws;public boolean recycled,fail;public static volatile CountDownLatch entered,release;public boolean compress(CompressFormat f,int quality,OutputStream out){try{if(fail)return false;if(entered!=null){entered.countDown();release.await();}out.write(new byte[]{1,2,3,4});return true;}catch(Exception e){throw new RuntimeException(e);}}public boolean isRecycled(){return recycled;}public void recycle(){recycled=true;}}'''
STORE_STUBS['android/graphics/BitmapFactory.java']='''package android.graphics;public class BitmapFactory{public static class Options{public boolean inJustDecodeBounds;public int outWidth,outHeight;public Bitmap.Config inPreferredConfig;}public static Bitmap decodeByteArray(byte[] bytes,int from,int length,Options o){o.outWidth=320;o.outHeight=180;return o.inJustDecodeBounds?null:new Bitmap();}}'''
STORE_STUBS['android/util/Log.java']='''package android.util;public class Log{public static int w(String tag,String message,Throwable error){return 0;}}'''
STORE_STUBS['android/util/AtomicFile.java']='''package android.util;import java.io.*;import java.nio.file.*;public class AtomicFile{final File base,temp;public static volatile boolean failStart;public AtomicFile(File f){base=f;temp=new File(f.getPath()+".new");}public File getBaseFile(){return base;}public FileInputStream openRead()throws IOException{return new FileInputStream(base);}public FileOutputStream startWrite()throws IOException{if(failStart)throw new IOException("synthetic start failure");return new FileOutputStream(temp);}public void finishWrite(FileOutputStream out)throws IOException{out.close();Files.move(temp.toPath(),base.toPath(),StandardCopyOption.REPLACE_EXISTING);}public void failWrite(FileOutputStream out){try{out.close();}catch(Exception e){}temp.delete();}}'''
STORE_STUBS['org/json/JSONException.java']='''package org.json;public class JSONException extends Exception{public JSONException(String value){super(value);}}'''
STORE_STUBS['org/json/JSONObject.java']='''package org.json;import java.util.*;public class JSONObject{Map<String,String> values=new HashMap<>();public JSONObject(){}public JSONObject(String source)throws JSONException{for(String item:source.split(";")){String[] pair=item.split("=",2);if(pair.length!=2)throw new JSONException("bad fake metadata");values.put(pair[0],pair[1]);}}public JSONObject put(String key,Object value)throws JSONException{values.put(key,String.valueOf(value));return this;}public String optString(String key){return values.getOrDefault(key,"");}public long optLong(String key){try{return Long.parseLong(optString(key));}catch(Exception e){return 0;}}public String toString(){StringBuilder b=new StringBuilder();for(Map.Entry<String,String> v:values.entrySet()){if(b.length()>0)b.append(';');b.append(v.getKey()).append('=').append(v.getValue());}return b.toString();}}'''

STORE_TEST=r'''package org.mahoyo.port;
import java.io.*;import java.nio.file.*;import java.util.*;import java.util.concurrent.*;import java.util.concurrent.atomic.*;import android.graphics.*;
public final class SlotStoreSelfTest{
static int checks;static void check(boolean value,String label){checks++;if(!value)throw new AssertionError(label);}
static List<SlotThumbnailStore.Entry> list(SlotThumbnailStore store)throws Exception{CountDownLatch done=new CountDownLatch(1);AtomicReference<List<SlotThumbnailStore.Entry>> value=new AtomicReference<>();store.list(rows->{value.set(rows);done.countDown();});if(!done.await(5,TimeUnit.SECONDS))throw new AssertionError("metadata callback timeout");return value.get();}
public static void main(String[]args)throws Exception{File dir=new File(args[0]);File saves=new File(dir,"store-saves"),sidecars=new File(dir,"store-thumbs");saves.mkdirs();File slot=new File(saves,"slot12.whs");byte[] a=Files.readAllBytes(new File(dir,"fixtures/slot12.whs").toPath()),b=Files.readAllBytes(new File(dir,"fixtures/future.whs").toPath());Files.write(slot.toPath(),a);long stamp=slot.lastModified();SlotThumbnailStore store=new SlotThumbnailStore(saves,sidecars);ExecutorService engine=Executors.newSingleThreadExecutor();
try{List<SlotThumbnailStore.Entry> rows=list(store);check(rows.size()==12&&rows.get(11).exists&&rows.get(11).thumbnail==null&&rows.get(11).text.equals("日本語 ruby 😀 end"),"legacy slot has source date/text without blank thumbnail");check(slot.lastModified()==stamp&&Arrays.equals(a,Files.readAllBytes(slot.toPath())),"listing preserves original save bytes and mtime");
Bitmap image=new Bitmap();engine.submit(()->store.saved(12,image)).get();rows=list(store);check(image.recycled&&rows.get(11).thumbnail!=null,"successful bound sidecar is decoded and input bitmap released");SlotThumbnailStore.recycle(rows);File thumb=new File(sidecars,"slot12.thumb");byte[] before=Files.readAllBytes(thumb.toPath());
Bitmap failure=new Bitmap();failure.fail=true;engine.submit(()->store.saved(12,failure)).get();rows=list(store);check(failure.recycled&&Arrays.equals(before,Files.readAllBytes(thumb.toPath()))&&rows.get(11).thumbnail!=null,"compression failure preserves existing sidecar");SlotThumbnailStore.recycle(rows);
android.util.AtomicFile.failStart=true;Bitmap noWrite=new Bitmap();engine.submit(()->store.saved(12,noWrite)).get();rows=list(store);check(noWrite.recycled&&Arrays.equals(before,Files.readAllBytes(thumb.toPath())),"atomic sidecar write failure preserves previous thumbnail");SlotThumbnailStore.recycle(rows);android.util.AtomicFile.failStart=false;
Bitmap.entered=new CountDownLatch(1);Bitmap.release=new CountDownLatch(1);Bitmap racing=new Bitmap();engine.submit(()->store.saved(12,racing)).get();check(Bitmap.entered.await(5,TimeUnit.SECONDS),"worker reached synthetic compression barrier");Files.write(slot.toPath(),b);Bitmap.release.countDown();rows=list(store);check(rows.get(11).text.equals("future")&&rows.get(11).thumbnail==null,"save replacement during sidecar write cannot display an older scene thumbnail");Bitmap.entered=null;Bitmap.release=null;
before=Files.readAllBytes(thumb.toPath());engine.submit(()->store.saved(12,null)).get();rows=list(store);check(rows.get(11).thumbnail==null&&Arrays.equals(before,Files.readAllBytes(thumb.toPath())),"capture failure leaves old sidecar untouched and SHA mismatch hides it");
Files.write(thumb.toPath(),new byte[]{0,1});rows=list(store);check(rows.get(11).exists&&rows.get(11).text.equals("future")&&rows.get(11).thumbnail==null,"corrupt optional sidecar never hides valid source metadata");
check(Arrays.equals(b,Files.readAllBytes(slot.toPath())),"all thumbnail operations never modify WHS source");System.out.println("slot sidecar transaction: "+checks+" checks passed");}finally{engine.shutdownNow();store.close();}}
}
'''

MODAL_TEST=r'''import java.util.*;import java.util.function.*;
public final class SlotModalSelfTest{
static final class UiText {static String text(Object context,String value){return value;}}
static int checks;static void check(boolean value,String label){checks++;if(!value)throw new AssertionError(label);}
static final class SystemClock{static long uptimeMillis(){return 1000;}}
static final class Ui{void removeCallbacks(Runnable r){}}
static final class AlertDialog{boolean showing;String message;Consumer<AlertDialog> closed;boolean isShowing(){return showing;}void setMessage(String v){message=v;}void setOnDismissListener(Consumer<AlertDialog> value){closed=value;}void show(){showing=true;}void dismiss(){showing=false;if(closed!=null)closed.accept(this);}static final class Builder{String message;Builder(Object owner){}Builder setTitle(String v){return this;}Builder setMessage(String v){message=v;return this;}Builder setPositiveButton(String v,Object callback){return this;}AlertDialog create(){AlertDialog value=new AlertDialog();value.message=message;return value;}}}
static final class Json{String optString(String key){return "transitionReady";}String optString(String key,String fallback){return fallback;}long optLong(String key){return 1;}long optLong(String key,long fallback){return 1;}Object optJSONObject(String key){return null;}}
static final class Archive{void presented(boolean active){}void presented(Object value){}}
Archive archive=new Archive(),gallery=new Archive(),musicRoom=new Archive(),choice=new Archive();
static final class Particles{String readyAck(long version){return "";}}static final class Renderer{boolean ready(Scene scene){return true;}}
static final class Scene{Particles particles=new Particles();Json json=new Json();Transition transition=new Transition();static final class Transition{String id="native-frame";}}
static final class Text{void update(Scene value,Object viewport,boolean hidden){}}
static final class InputSession{String acknowledge(long v,boolean current,boolean interactive,boolean released,int w,int h){return "";}}static final class Root{int getWidth(){return 1920;}int getHeight(){return 1080;}}static final class Movie{boolean releasedForInput(){return true;}}InputSession inputSession=new InputSession();Root root=new Root();Movie moviePlayer;boolean frontendsActive(){return false;}boolean teatimeActive(){return false;}boolean choiceActive(){return false;}
Renderer renderer=new Renderer();String particleReadyAck="";Ui ui=new Ui();Runnable tick=()->{};boolean destroyed,readerPullOpen,readerPullCommit,paused,slotUiOpen,errorModalOpen,hidden,historyLoading,historyOpen,historyClosing;String lastError="";AlertDialog errorDialog;Scene scene=new Scene(),committedScene;long staleRenderCommits;Object viewport;Text text=new Text();TransitionHandshake transitionHandshake=new TransitionHandshake();List<String> commands=new ArrayList<>();boolean isFinishing(){return false;}boolean recurse;MainHaltReturn haltReturn=new MainHaltReturn();void maybeReturnToTitle(){}void maybeShowReaderHelp(){}void stopHistoryReplay(){}void updateChoiceInput(){}void updateMenuStates(){}
void command(String name,String value){commands.add(name);if(recurse&&name.equals("pause"))error(lastError);}
HELPERS
public static void main(String[]args){SlotModalSelfTest state=new SlotModalSelfTest();state.renderCommitted(state.scene);check(state.commands.equals(List.of("transitionReady")),"normal committed frame acknowledges transition");state.commands.clear();state.recurse=true;Scene original=state.scene;state.error("GPU failure\nline2");check(state.errorModalOpen&&state.errorDialog.isShowing()&&state.commands.equals(List.of("pause")),"visible UI error pauses Core once even when pause snapshot reports the same error");state.renderCommitted(state.scene);check(state.commands.equals(List.of("pause")),"render callback cannot advance the VM behind error dialog");state.error("GPU failure\nline2");check(state.commands.size()==1,"same error does not recursively create dialogs or commands");state.error("new GPU failure");check(state.errorDialog.message.equals("new GPU failure")&&state.commands.size()==1,"new message updates existing error without pause loop");state.slotUiOpen=true;state.errorDialog.dismiss();check(state.commands.get(state.commands.size()-1).equals("pause")&&!state.errorModalOpen&&state.lastError.equals("new GPU failure"),"closing error while slots stay open preserves pause and diagnostic");check(state.scene==original,"UI error never rewrites the Runtime snapshot");state.slotUiOpen=false;state.updateCorePause();check(state.commands.get(state.commands.size()-1).equals("resume"),"last overlay close resumes Core");state.renderCommitted(state.scene);check(state.commands.get(state.commands.size()-1).equals("transitionReady"),"previously blocked frame acknowledgement is retried after resume");state.paused=true;state.updateCorePause();check(state.commands.get(state.commands.size()-1).equals("pause"),"Activity pause remains a reason");state.commands.clear();state.renderCommitted(state.scene);check(state.commands.isEmpty(),"paused Activity does not acknowledge renderer frame");state.destroyed=true;state.updateCorePause();state.error("after destroy");check(state.commands.isEmpty(),"destroyed Activity never resumes or shows another error");
for(int width:new int[]{320,360,600,720,900,1024,1400}){int n=columns(width);check(n>=1&&n<=8,"responsive grid has bounded columns");if(width>=900)check(n>=5,"tablet grid is denser than three cards");}System.out.println("slot/modal pause: "+checks+" checks passed");}}
'''

QUEUE_TEST=r'''import java.util.*;import java.util.function.*;
public final class SlotQueueSelfTest{
static int checks;static void check(boolean value,String label){checks++;if(!value)throw new AssertionError(label);}
static class SystemClock{static long uptimeMillis(){return 1000;}}static class Scene{}static class Engine{List<Runnable> queue=new ArrayList<>();void execute(Runnable r){queue.add(r);}void drain(){while(!queue.isEmpty())queue.remove(0).run();}}static class Ui{void post(Runnable r){r.run();}}static class NativeBridge{static List<String> calls=new ArrayList<>();static void waveClock(long handle,long time){}static String snapshot(long handle){return "";}static String command(long handle,String type,String value){calls.add(type);return type;}}
Engine engine=new Engine();Ui ui=new Ui();MainHaltReturn haltReturn=new MainHaltReturn();static final class JSONObject{JSONObject(String value){}long optLong(String key,long fallback){return fallback;}String optString(String key,String fallback){return fallback;}}static boolean normalMainHalt(JSONObject value){return false;}long handle=1;boolean destroyed,errorModalOpen,readerPullOpen,readerPullCommit,choiceBlocked;Scene apply(String value,long started){return new Scene();}void error(String value){throw new AssertionError(value);}
HELPERS
public static void main(String[]args){SlotQueueSelfTest flow=new SlotQueueSelfTest();flow.command("transitionReady","id");flow.command("tick","1");flow.command("advance","");flow.errorModalOpen=true;flow.engine.drain();check(NativeBridge.calls.isEmpty(),"already queued progression is rejected at engine execution after UI error opens");flow.command("pause","1");flow.command("audioEnded","bgm");flow.engine.drain();check(NativeBridge.calls.equals(List.of("pause","audioEnded")),"Core pause and audio resource completion stay deliverable");flow.errorModalOpen=false;flow.command("transitionReady","id");flow.engine.drain();check(NativeBridge.calls.get(2).equals("transitionReady"),"progression may retry after modal close");List<Scene> rejected=new ArrayList<>();flow.command("choiceSelect","R 0",rejected::add);flow.choiceBlocked=true;flow.engine.drain();check(rejected.size()==1&&rejected.get(0)==null&&!NativeBridge.calls.contains("choiceSelect"),"already queued selection is rejected at engine execution after background or modal pause");flow.choiceBlocked=false;flow.command("choiceSelect","R 0");flow.engine.drain();check(NativeBridge.calls.get(NativeBridge.calls.size()-1).equals("choiceSelect"),"unblocked selection uses normal command route");flow.handle=0;List<Scene> result=new ArrayList<>();flow.command("save","12",result::add);flow.engine.drain();check(result.size()==1&&result.get(0)==null,"missing engine handle completes transaction callback once");flow.destroyed=true;flow.command("advance","");check(flow.engine.queue.isEmpty(),"destroyed UI adds no new engine work");SlotQueueSelfTest sheet=new SlotQueueSelfTest();NativeBridge.calls.clear();sheet.command("tick","1");sheet.readerPullOpen=true;sheet.engine.drain();check(NativeBridge.calls.equals(List.of("tick")),"unconfirmed preview keeps queued game progression running");NativeBridge.calls.clear();sheet.readerPullCommit=true;sheet.command("tick","2");sheet.engine.drain();check(NativeBridge.calls.isEmpty(),"confirmed menu blocks progression");sheet.command("pause","1");sheet.engine.drain();check(NativeBridge.calls.equals(List.of("pause")),"preview pause remains deliverable");sheet.readerPullOpen=false;sheet.readerPullCommit=false;sheet.command("tick","2");sheet.engine.drain();check(NativeBridge.calls.equals(List.of("pause","tick")),"cancelled preview can resume game progression");System.out.println("queued UI-error gate: "+checks+" checks passed");}}
'''


def block(source,marker):
    begin=source.index(marker);end=source.index('{',begin)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]

CAPTURE_TEST=r'''package org.mahoyo.port;
import android.graphics.*;import android.opengl.*;import android.view.*;import android.os.*;import java.util.*;import java.util.concurrent.atomic.*;
public final class SlotCaptureSelfTest{
static int checks;static void check(boolean value,String label){checks++;if(!value)throw new AssertionError(label);}static void near(float a,float b,String label){check(Math.abs(a-b)<.001,label+" "+a+"/"+b);}
static void request(GLSurfaceView surface,TextOverlay text,AtomicBoolean live,List<Bitmap> results){SlotThumbnailCapture.request(surface,text,new Rect(100,50,2100,1050),live::get,results::add);}
public static void main(String[]args){SlotThumbnailCapture.Plan p=new SlotThumbnailCapture.Plan(100,50,2000,1000);check(p.outWidth==480&&p.outHeight==240,"bounded aspect-preserving thumbnail");near(p.x(100),0,"left crop origin");near(p.x(2100),480,"right crop origin");near(p.y(1050),240,"bottom crop origin");for(int w:new int[]{1,160,1920,2248,2480})for(int h:new int[]{1,720,1080,2248}){p=new SlotThumbnailCapture.Plan(0,0,w,h);check(p.outWidth<=480&&p.outHeight<=480&&p.outWidth>0&&p.outHeight>0,"all fold geometry budgets bounded");}
GLSurfaceView surface=new GLSurfaceView();surface.x=20;surface.y=30;TextOverlay text=new TextOverlay();text.x=25;text.y=40;AtomicBoolean live=new AtomicBoolean(true);List<Bitmap> results=new ArrayList<>();request(surface,text,live,results);PixelCopy.complete(PixelCopy.SUCCESS);check(results.size()==1&&results.get(0)!=null&&results.get(0).draws==1,"Surface image receives text-only overlay");near(results.get(0).drawnX,(300+5-100)*.24f,"overlay relative window x/crop/scale");near(results.get(0).drawnY,(400+10-50)*.24f,"overlay relative window y/crop/scale");
results.clear();request(surface,text,live,results);live.set(false);PixelCopy.complete(0);check(results.size()==1&&results.get(0)==null&&Bitmap.last.recycled,"stale/cancelled session never yields thumbnail");live.set(true);
results.clear();request(surface,text,live,results);surface.width++;PixelCopy.complete(0);check(results.size()==1&&results.get(0)==null&&Bitmap.last.recycled,"resize during copy rejects stale coordinates");surface.width--;
results.clear();request(surface,text,live,results);text.x++;PixelCopy.complete(0);check(results.get(0)==null&&Bitmap.last.recycled,"overlay movement rejects stale composition");text.x--;
results.clear();request(surface,text,live,results);Handler.expire();check(results.size()==1&&results.get(0)==null&&!Bitmap.last.recycled,"timeout completes once without recycling in-flight GPU buffer");PixelCopy.complete(0);check(results.size()==1&&Bitmap.last.recycled,"late copy releases buffer without second save callback");
results.clear();text.visibility=View.GONE;request(surface,text,live,results);PixelCopy.complete(0);check(results.get(0)!=null&&results.get(0).draws==0,"hidden TEATIME text layer is not captured");text.visibility=View.VISIBLE;
results.clear();request(surface,text,live,results);PixelCopy.complete(1);check(results.get(0)==null&&Bitmap.last.recycled,"PixelCopy failure falls back to metadata");
results.clear();text.fail=true;request(surface,text,live,results);PixelCopy.complete(0);check(results.size()==1&&results.get(0)==null&&Bitmap.last.recycled,"overlay exception does not block confirmed save");text.fail=false;
results.clear();PixelCopy.fail=true;request(surface,text,live,results);check(results.size()==1&&results.get(0)==null&&Bitmap.last.recycled,"invalid Surface callback completes once");PixelCopy.fail=false;Handler.expire();check(results.size()==1,"failure removes timeout duplicate");
System.out.println("slot capture: "+checks+" checks passed");}}
'''


def main():
    OUT.mkdir(parents=True,exist_ok=True);fixtures=OUT/'fixtures';fixtures.mkdir(exist_ok=True)
    reports={}
    def run_check(name,args):
        result=subprocess.run(args,check=True,capture_output=True,text=True,encoding='utf8');print(result.stdout.strip());reports[name]={'checks':int(re.search(r'(\d+) checks passed',result.stdout)[1]),'output':result.stdout.strip()}
    good=fixture('@i日本語 <ruby|reading> 😀@r end');(fixtures/'slot12.whs').write_bytes(good)
    bad=bytearray(good);bad[22]^=1;(fixtures/'bad-crc.whs').write_bytes(bad)
    (fixtures/'truncated.whs').write_bytes(good[:-1]);(fixtures/'bad-header.whs').write_bytes(b'BAD'+good[3:]);(fixtures/'trailing.whs').write_bytes(good+b'x')
    bad=bytearray(good);struct.pack_into('<I',bad,16,0x7fffffff);struct.pack_into('<I',bad,12,zlib.crc32(bad[16:]));(fixtures/'oversize-field.whs').write_bytes(bad)
    (fixtures/'future.whs').write_bytes(fixture('future',b'new hostExtra WHS999 field layout opaque'))
    writer=OUT/'NativeSlotFixture.cpp';writer.write_text('''#include "save_store.h"
int main(int argc,char**argv){mahoyo::save::Document doc;doc.vm={'v','m'};mahoyo::save::Layer layer;layer.image="fake.mzp";layer.kind="event";layer.w=1920;layer.h=1080;doc.layers.push_back(layer);mahoyo::save::Audio audio;audio.name="fake.ogg";audio.channel="bgm";doc.audio.push_back(audio);doc.history={"previous"};doc.text=u8"@i日本語 <ruby|reading> 😀@r end";doc.hostExtra={'W','H','S','1','6'};mahoyo::save::writeAtomic(argv[1],doc);}
''',encoding='utf8')
    compiler=shutil.which('clang++')
    if not compiler:raise RuntimeError('clang++ required for the actual C++ SaveStore fixture')
    subprocess.run([compiler,'-std=c++17','-O2','-I',str(ROOT/'runtime'),str(writer),str(ROOT/'runtime/save_store.cpp'),'-o',str(OUT/'NativeSlotFixture.exe')],check=True)
    subprocess.run([str(OUT/'NativeSlotFixture.exe'),str(fixtures/'native.whs')],check=True)
    jdk=Path(os.environ.get('JAVA_HOME',''))/'bin';javac=str(jdk/'javac.exe') if (jdk/'javac.exe').exists() else 'javac';java=str(jdk/'java.exe') if (jdk/'java.exe').exists() else 'java'
    metadata=OUT/'SlotMetadataSelfTest.java';metadata.write_text(METADATA_TEST,encoding='utf8')
    subprocess.run([javac,'-encoding','UTF-8','-d',str(OUT/'metadata'),str(SOURCE/'SlotSaveMetadata.java'),str(metadata)],check=True)
    run_check('metadata',[java,'-cp',str(OUT/'metadata'),'org.mahoyo.port.SlotMetadataSelfTest',str(fixtures)])
    paths=[]
    for name,value in STUBS.items():
        p=OUT/'fake'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(value,encoding='utf8');paths.append(str(p))
    capture=OUT/'SlotCaptureSelfTest.java';capture.write_text(CAPTURE_TEST,encoding='utf8')
    subprocess.run([javac,'-encoding','UTF-8','-d',str(OUT/'capture'),*paths,str(SOURCE/'SlotThumbnailCapture.java'),str(capture)],check=True)
    run_check('capture',[java,'-cp',str(OUT/'capture'),'org.mahoyo.port.SlotCaptureSelfTest'])
    paths=[]
    for name,value in STORE_STUBS.items():
        p=OUT/'store-fake'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(value,encoding='utf8');paths.append(str(p))
    store=OUT/'SlotStoreSelfTest.java';store.write_text(STORE_TEST,encoding='utf8')
    subprocess.run([javac,'-encoding','UTF-8','-d',str(OUT/'store'),*paths,str(SOURCE/'SlotSaveMetadata.java'),str(SOURCE/'SlotThumbnailStore.java'),str(store)],check=True)
    run_check('sidecar',[java,'-cp',str(OUT/'store'),'org.mahoyo.port.SlotStoreSelfTest',str(OUT)])
    activity=(SOURCE/'MainActivity.java').read_text(encoding='utf8');grid=(SOURCE/'SlotGridDialog.java').read_text(encoding='utf8')
    helpers='\n'.join(block(activity,marker) for marker in ['private boolean readerPullBlocksGame(', 'private void error(', 'private void updateCorePause(', 'private void renderCommitted(', 'static final class TransitionHandshake'])+'\n'+block(grid,'static int columns(')
    helpers=helpers.replace('android.util.Log.e("WoH.Runtime",message);','')
    halt=OUT/'MainHaltReturn.java';halt.write_text((SOURCE/'MainHaltReturn.java').read_text(encoding='utf8').replace('package org.mahoyo.port;',''),encoding='utf8')
    modal=OUT/'SlotModalSelfTest.java';modal.write_text(MODAL_TEST.replace('HELPERS',helpers),encoding='utf8')
    subprocess.run([javac,'-encoding','UTF-8','-d',str(OUT/'modal'),str(halt),str(modal)],check=True)
    run_check('modal',[java,'-cp',str(OUT/'modal'),'SlotModalSelfTest'])
    queued=OUT/'SlotQueueSelfTest.java';queued_helpers='\n'.join(block(activity,m)for m in ['private boolean readerPullBlocksGame(', 'private void command(String type,String payload){','private void command(String type,String payload,Consumer<Scene> completion){','private void command(String type,String payload,Consumer<Scene> completion,MainHaltReturn.Ticket terminal){'])
    queued.write_text(QUEUE_TEST.replace('HELPERS',queued_helpers),encoding='utf8')
    subprocess.run([javac,'-encoding','UTF-8','-d',str(OUT/'queue'),str(halt),str(queued)],check=True)
    run_check('engineQueue',[java,'-cp',str(OUT/'queue'),'SlotQueueSelfTest'])
    assert 'PixelCopy.request(surface,area,bitmap' in (SOURCE/'SlotThumbnailCapture.java').read_text(encoding='utf8')
    assert 'writer.println(prefix+"WoHUiError "+JSONObject.quote(lastError));' in activity
    assert 'next.json.optString("notice").equals("Saved")' in activity
    assert 'if((errorModalOpen||readerPullBlocksGame())&&(type.equals("choiceSelect")||type.equals("tick")||type.equals("transitionReady")||type.equals("advance")))' in activity
    sources=['MainActivity.java','SlotSaveMetadata.java','SlotThumbnailStore.java','SlotThumbnailCapture.java','SlotGridDialog.java']
    receipt={'checks':reports,'checkCount':sum(row['checks']for row in reports.values()),'syntheticFixtureSHA256':hashlib.sha256(good).hexdigest(),'actualCppSaveStoreFixtureSHA256':hashlib.sha256((fixtures/'native.whs').read_bytes()).hexdigest(),'sourceSnapshotSHA256':{name:hashlib.sha256((SOURCE/name).read_bytes()).hexdigest()for name in sources},'mainScopedSHA256':hashlib.sha256((helpers+'\n'+queued_helpers).encode()).hexdigest(),'deviceSaveWritten':False,'devicePixelCopyOrJpegEncoderExecuted':False,'stubs':['Surface/Canvas coordinates and PixelCopy callbacks','Bitmap JPEG/decode','AtomicFile operations use a synthetic temporary-file adapter','JSONObject sidecar serializer uses a minimal synthetic adapter'],'limits':['Only synthetic PC save fixtures are created','Actual C++ SaveStore writer output is independently read by the metadata parser','Actual Android view composition, JPEG pixels and filesystem atomic behavior still need device evidence','Grid column selection is tested; real touch layout still needs a screenshot'],'mismatchCount':0}
    (OUT/'validation.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
    print('Synthetic fixture SHA256:',hashlib.sha256(good).hexdigest())


if __name__=='__main__':main()
