package org.mahoyo.port;

import java.io.*;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;

/** Symmetric newest-slot selection, with the same union rules as normal save ZIP import. */
final class SaveSyncMerge {
    static String hash(File file)throws Exception {
        MessageDigest hash=MessageDigest.getInstance("SHA-256");byte[] block=new byte[65536];
        try(InputStream in=new FileInputStream(file)){int n;while((n=in.read(block))!=-1)hash.update(block,0,n);}
        return SlotSaveMetadata.hex(hash.digest());
    }
    static SaveArchive.Archive snapshot(File saves,File thumbs,File workspace)throws Exception {
        boolean any=false;for(int slot=1;slot<=SaveArchive.COUNT;slot++)any|=new File(saves,"slot"+slot+".whs").isFile();
        for(String name:SaveCommon.NAMES)any|=new File(saves,name).isFile();
        return any?SaveArchive.snapshot(saves,thumbs,workspace):new SaveArchive.Archive(workspace);
    }
    static SaveArchive.Archive merge(SaveArchive.Archive a,SaveArchive.Archive b,File workspace)throws Exception {
        SaveArchive.Archive out=new SaveArchive.Archive(workspace);
        try{
            for(int slot=1;slot<=SaveArchive.COUNT;slot++){
                String name="saves/slot"+slot+".whs";SaveArchive.Archive winner=winner(a,b,name);if(winner==null)continue;
                add(out,name,winner.files.get(name),winner.times.get(name));
                String thumb="thumbnails/slot"+slot+".thumb";SaveArchive.Archive picture=winner;
                if(!picture.files.containsKey(thumb)){
                    SaveArchive.Archive other=winner==a?b:a;
                    if(other.files.containsKey(thumb)&&other.files.containsKey(name)&&hash(other.files.get(name)).equals(hash(winner.files.get(name))))picture=other;
                }
                if(picture.files.containsKey(thumb))add(out,thumb,picture.files.get(thumb),picture.times.get(thumb));
            }
            for(String common:SaveCommon.NAMES){
                String name="saves/"+common;SaveArchive.Archive latest=winner(a,b,name);if(latest==null)continue;
                SaveArchive.Archive older=latest==a?b:a;File source=latest.files.get(name),previous=older.files.get(name);
                if(previous==null||hash(source).equals(hash(previous)))add(out,name,source,latest.times.get(name));
                else{
                    byte[] merged=SaveCommon.merge(common,Files.readAllBytes(source.toPath()),Files.readAllBytes(previous.toPath()));
                    File file=new File(out.root,common);Files.write(file.toPath(),merged);long modified=Math.max(latest.times.get(name),older.times.get(name));
                    if(!file.setLastModified(modified))throw new IOException("Cannot preserve common progress date");add(out,name,file,modified);
                }
            }
            return out;
        }catch(Exception failure){out.close();throw failure;}
    }
    private static SaveArchive.Archive winner(SaveArchive.Archive a,SaveArchive.Archive b,String name)throws Exception {
        File left=a.files.get(name),right=b.files.get(name);if(left==null)return right==null?null:b;if(right==null)return a;
        int age=Long.compare(a.times.get(name),b.times.get(name));
        if(age!=0)return age>0?a:b;
        // Equal timestamps carry no ordering information; stable hashes make the result symmetric.
        return hash(left).compareTo(hash(right))>=0?a:b;
    }
    private static void add(SaveArchive.Archive out,String name,File file,long modified){out.files.put(name,file);out.times.put(name,modified);}
    static String fingerprint(SaveArchive.Archive archive)throws Exception {
        MessageDigest digest=MessageDigest.getInstance("SHA-256");for(Map.Entry<String,File> entry:archive.files.entrySet()){
            digest.update(entry.getKey().getBytes(java.nio.charset.StandardCharsets.UTF_8));digest.update((byte)0);digest.update(hash(entry.getValue()).getBytes(java.nio.charset.StandardCharsets.US_ASCII));
        }return SlotSaveMetadata.hex(digest.digest());
    }
    private SaveSyncMerge(){}
}
