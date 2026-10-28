package org.mahoyo.port;

import android.graphics.*;
import android.os.*;
import android.util.AtomicFile;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** Thumbnails never change a WHS file. All directory/read/decode/compress I/O is off UI. */
final class SlotThumbnailStore {
    static final int COUNT=12;
    final File saves,sidecars;
    private final ExecutorService io=Executors.newSingleThreadExecutor();
    private final Handler ui=new Handler(Looper.getMainLooper());
    SlotThumbnailStore(File saves,File sidecars){this.saves=saves;this.sidecars=sidecars;}
    static final class Entry {
        final int slot;final boolean exists;final long modified,size;final String text;final Bitmap thumbnail;
        Entry(int slot,boolean exists,long modified,long size,String text,Bitmap thumbnail){this.slot=slot;this.exists=exists;this.modified=modified;this.size=size;this.text=text;this.thumbnail=thumbnail;}
    }
    File saveFile(int slot){if(slot<1||slot>COUNT)throw new IllegalArgumentException("Invalid slot");return new File(saves,"slot"+slot+".whs");}
    void transfer(Runnable work){io.execute(work);} // Serialize backups with pending thumbnail writes.
    void list(Consumer<List<Entry>> completion){io.execute(()->{ArrayList<Entry> entries=new ArrayList<>();for(int slot=1;slot<=COUNT;slot++){File file=saveFile(slot);SlotSaveMetadata metadata=null;Bitmap thumb=null;boolean exists=file.isFile();if(exists)try{metadata=SlotSaveMetadata.read(file);thumb=readThumbnail(slot,metadata);}catch(IOException|JSONException|RuntimeException error){android.util.Log.w("WoH.Slots","Slot metadata "+slot,error);}entries.add(new Entry(slot,exists,metadata==null?file.lastModified():metadata.modified,metadata==null?file.length():metadata.size,metadata==null?"":metadata.text,thumb));}ui.post(()->completion.accept(entries));});}
    /** Run on the serialized engine executor immediately after notice=Saved.
     * Bind before any later save command, then recheck the binding on the I/O worker.
     * Ownership of image transfers here even if binding or submission fails. */
    void saved(int slot,Bitmap image){
        SlotSaveMetadata binding;
        try{binding=SlotSaveMetadata.read(saveFile(slot));}catch(IOException error){if(image!=null)image.recycle();android.util.Log.w("WoH.Slots","Thumbnail save binding",error);return;}
        if(image==null)return;
        try{io.execute(()->writeThumbnail(slot,image,binding));}catch(RejectedExecutionException closed){image.recycle();}
    }
    private AtomicFile thumbnail(int slot){return new AtomicFile(new File(sidecars,"slot"+slot+".thumb"));}
    private Bitmap readThumbnail(int slot,SlotSaveMetadata save)throws IOException,JSONException {
        AtomicFile source=thumbnail(slot);if(!source.getBaseFile().isFile())return null;
        try(DataInputStream input=new DataInputStream(source.openRead())){
            if(input.readInt()!=0x57485431)return null;
            int metadataBytes=input.readInt();if(metadataBytes<1||metadataBytes>4096)return null;
            byte[] bytes=new byte[metadataBytes];input.readFully(bytes);JSONObject meta=new JSONObject(new String(bytes,StandardCharsets.UTF_8));
            if(!meta.optString("sha256").equals(save.sha256)||meta.optLong("size")!=save.size)return null;
            int count=input.readInt();if(count<1||count>1024*1024)return null;bytes=new byte[count];input.readFully(bytes);if(input.read()!=-1)return null;
            BitmapFactory.Options options=new BitmapFactory.Options();options.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(bytes,0,count,options);
            if(options.outWidth<1||options.outHeight<1||options.outWidth>1024||options.outHeight>1024)return null;
            options.inJustDecodeBounds=false;options.inPreferredConfig=Bitmap.Config.RGB_565;return BitmapFactory.decodeByteArray(bytes,0,count,options);
        }
    }
    private void writeThumbnail(int slot,Bitmap image,SlotSaveMetadata binding){
        FileOutputStream output=null;AtomicFile target=thumbnail(slot);
        try{
            if(!SlotSaveMetadata.read(saveFile(slot)).sha256.equals(binding.sha256))return;
            ByteArrayOutputStream jpeg=new ByteArrayOutputStream();if(!image.compress(Bitmap.CompressFormat.JPEG,88,jpeg))return;
            if(jpeg.size()>1024*1024)return;
            JSONObject meta=new JSONObject().put("sha256",binding.sha256).put("size",binding.size).put("modified",binding.modified);
            byte[] json=meta.toString().getBytes(StandardCharsets.UTF_8);sidecars.mkdirs();output=target.startWrite();DataOutputStream data=new DataOutputStream(output);data.writeInt(0x57485431);data.writeInt(json.length);data.write(json);data.writeInt(jpeg.size());jpeg.writeTo(data);data.flush();target.finishWrite(output);output=null;
        }catch(IOException|JSONException|RuntimeException error){if(output!=null)target.failWrite(output);android.util.Log.w("WoH.Slots","Thumbnail sidecar",error);}
        finally{image.recycle();}
    }
    void close(){io.shutdown();}
    static void recycle(List<Entry> entries){for(Entry entry:entries)if(entry.thumbnail!=null&&!entry.thumbnail.isRecycled())entry.thumbnail.recycle();}
}
