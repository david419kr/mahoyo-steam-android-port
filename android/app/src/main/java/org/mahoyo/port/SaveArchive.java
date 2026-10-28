package org.mahoyo.port;

import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;
import org.json.*;

/** Bounded, staged save backups. All public methods run on a storage worker. */
final class SaveArchive {
    static final int COUNT=12;
    static final String MANIFEST="mahoyo-saves.json";
    static final long MAX_TOTAL=800L*1024*1024,MAX_SAVE=64L*1024*1024;
    static final class Archive implements AutoCloseable {
        final File root;final TreeMap<String,File> files=new TreeMap<>();final Map<String,Long> times=new HashMap<>();
        Archive(File workspace)throws IOException {workspace.mkdirs();root=Files.createTempDirectory(workspace.toPath(),"save-zip-").toFile();}
        public void close()throws IOException {erase(root);}
        SortedSet<Integer> slots(){TreeSet<Integer> result=new TreeSet<>();for(String name:files.keySet())if(name.startsWith("saves/slot"))result.add(number(name));return result;}
    }
    static final class Plan {
        final Archive archive;final File saves,thumbs;final TreeMap<Integer,Integer> slots;final List<Integer> overwrites;
        final LinkedHashMap<File,File> replacements=new LinkedHashMap<>();final Map<File,String> expected=new LinkedHashMap<>();
        Plan(Archive archive,File saves,File thumbs,TreeMap<Integer,Integer> slots,List<Integer> overwrites){this.archive=archive;this.saves=saves;this.thumbs=thumbs;this.slots=slots;this.overwrites=Collections.unmodifiableList(overwrites);}
    }
    interface CheckedAction {void run()throws Exception;}
    static Archive snapshot(File saves,File thumbs,File workspace)throws Exception {
        Archive result=new Archive(workspace);
        try{for(int slot=1;slot<=COUNT;slot++){File source=new File(saves,"slot"+slot+".whs");if(!source.isFile())continue;SlotSaveMetadata.read(source);add(result,"saves/"+source.getName(),source);
                File thumbnail=new File(thumbs,"slot"+slot+".thumb");if(thumbnail.isFile()){try{checkThumbnail(thumbnail,SlotSaveMetadata.read(source));add(result,"thumbnails/"+thumbnail.getName(),thumbnail);}catch(IOException|JSONException badThumbnail){/* Optional stale thumbnails are omitted. */}}
            }
            for(String name:SaveCommon.NAMES){File source=new File(saves,name);if(source.isFile()){SaveCommon.validate(name,small(source));add(result,"saves/"+name,source);}}
            if(result.files.isEmpty())throw new IOException("No save data");return result;
        }catch(Exception error){result.close();throw error;}
    }
    private static void add(Archive archive,String name,File source)throws IOException {if(Files.isSymbolicLink(source.toPath()))throw new IOException("Symbolic save files are not supported");File target=new File(archive.root,Integer.toString(archive.files.size()));Files.copy(source.toPath(),target.toPath());if(!target.setLastModified(source.lastModified()))throw new IOException("Cannot preserve save date");archive.files.put(name,target);archive.times.put(name,source.lastModified());}
    static void write(Archive archive,OutputStream output,String version)throws Exception {
        JSONObject manifest=new JSONObject().put("schema",1).put("application","org.mahoyo.port.unofficial").put("version",version);JSONArray entries=new JSONArray();
        for(Map.Entry<String,File> item:archive.files.entrySet())entries.put(new JSONObject().put("name",item.getKey()).put("size",item.getValue().length()).put("sha256",hash(item.getValue())).put("modified",archive.times.get(item.getKey())));
        manifest.put("files",entries);
        try(ZipOutputStream zip=new ZipOutputStream(new BufferedOutputStream(output))){zip.putNextEntry(new ZipEntry(MANIFEST));zip.write(manifest.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));zip.closeEntry();
            byte[] buffer=new byte[65536];for(Map.Entry<String,File> item:archive.files.entrySet()){ZipEntry entry=new ZipEntry(item.getKey());entry.setTime(archive.times.get(item.getKey()));zip.putNextEntry(entry);try(InputStream input=new BufferedInputStream(new FileInputStream(item.getValue()))){copy(input,zip,MAX_SAVE,buffer);}zip.closeEntry();}}
    }
    static Archive read(InputStream input,File workspace)throws Exception {
        Archive result=new Archive(workspace);Set<String> seen=new HashSet<>();byte[] manifest=null;long total=0;int count=0;
        try(ZipInputStream zip=new ZipInputStream(new BufferedInputStream(input))){ZipEntry entry;byte[] buffer=new byte[65536];while((entry=zip.getNextEntry())!=null){if(++count>64)throw new IOException("Too many archive entries");String name=entry.getName();
                if(name.indexOf('\\')>=0||name.startsWith("/")||name.contains("..")||!seen.add(name))throw new IOException("Unsafe or duplicate backup entry");
                if(entry.isDirectory()){if(!name.equals("saves/")&&!name.equals("thumbnails/"))throw new IOException("Unknown backup directory");zip.closeEntry();continue;}
                long limit=name.equals(MANIFEST)?32768:limit(name);if(limit<0||entry.getSize()>limit)throw new IOException("Unknown or oversized backup entry");File staged=new File(result.root,Integer.toString(count));long size;
                try(OutputStream output=new FileOutputStream(staged)){size=copy(zip,output,Math.min(limit,MAX_TOTAL-total),buffer);}total+=size;zip.closeEntry();
                if(name.equals(MANIFEST))manifest=small(staged);else{result.files.put(name,staged);result.times.put(name,Math.max(0,entry.getTime()));}
            }
            if(manifest==null||result.files.isEmpty())throw new IOException("Not a save backup");JSONObject metadata=new JSONObject(new String(manifest,java.nio.charset.StandardCharsets.UTF_8));
            if(metadata.optInt("schema")!=1||!metadata.optString("application").equals("org.mahoyo.port.unofficial"))throw new IOException("Unsupported save backup");JSONArray rows=metadata.getJSONArray("files");if(rows.length()!=result.files.size())throw new IOException("Backup manifest mismatch");Set<String> declared=new HashSet<>();
            for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);String name=row.getString("name");File file=result.files.get(name);if(file==null||!declared.add(name)||file.length()!=row.getLong("size")||!hash(file).equals(row.getString("sha256")))throw new IOException("Backup checksum mismatch");long modified=row.optLong("modified",0);if(modified<0)throw new IOException("Invalid save date");result.times.put(name,modified);if(modified>0&&!file.setLastModified(modified))throw new IOException("Cannot preserve save date");}
            for(Map.Entry<String,File> item:result.files.entrySet()){String name=item.getKey();if(name.startsWith("saves/slot"))SlotSaveMetadata.read(item.getValue());else if(name.startsWith("saves/"))SaveCommon.validate(name.substring(6),small(item.getValue()));else{File save=result.files.get("saves/slot"+number(name)+".whs");if(save==null)throw new IOException("Thumbnail has no matching save");checkThumbnail(item.getValue(),SlotSaveMetadata.read(save));}}
            return result;
        }catch(Exception error){result.close();throw error;}
    }
    /** Reserve incoming non-conflicting slot numbers before shifting colliding ones. */
    static TreeMap<Integer,Integer> allocate(Set<Integer> incoming,Set<Integer> occupied){
        if(incoming.size()>COUNT)throw new IllegalArgumentException("Too many saves");TreeMap<Integer,Integer> result=new TreeMap<>();Set<Integer> used=new HashSet<>(occupied);
        for(int slot:new TreeSet<>(incoming)){validSlot(slot);if(!occupied.contains(slot)){result.put(slot,slot);used.add(slot);}}
        for(int source:new TreeSet<>(incoming))if(!result.containsKey(source)){for(int step=1;step<=COUNT;step++){int target=(source+step-1)%COUNT+1;if(!used.contains(target)){result.put(source,target);used.add(target);break;}}}
        Set<Integer> assigned=new HashSet<>(result.values());for(int source:new TreeSet<>(incoming))if(!result.containsKey(source)){int target=assigned.contains(source)?0:source;if(target==0)for(int candidate=1;candidate<=COUNT;candidate++)if(!assigned.contains(candidate)){target=candidate;break;}if(target==0)throw new IllegalStateException("No slot assignment");result.put(source,target);assigned.add(target);}
        return result;
    }
    static Plan plan(Archive archive,File saves,File thumbs)throws Exception {
        Set<Integer> occupied=new HashSet<>();for(int i=1;i<=COUNT;i++)if(new File(saves,"slot"+i+".whs").exists())occupied.add(i);TreeMap<Integer,Integer> slots=allocate(archive.slots(),occupied);List<Integer> overwrite=new ArrayList<>();for(int target:slots.values())if(occupied.contains(target))overwrite.add(target);Collections.sort(overwrite);
        Plan plan=new Plan(archive,saves.getCanonicalFile(),thumbs.getCanonicalFile(),slots,overwrite);
        for(int i=1;i<=COUNT;i++)remember(plan,new File(saves,"slot"+i+".whs"));
        for(Map.Entry<Integer,Integer> slot:slots.entrySet()){int source=slot.getKey(),target=slot.getValue();File save=new File(saves,"slot"+target+".whs"),thumbnail=new File(thumbs,"slot"+target+".thumb");remember(plan,thumbnail);plan.replacements.put(save,archive.files.get("saves/slot"+source+".whs"));plan.replacements.put(thumbnail,archive.files.get("thumbnails/slot"+source+".thumb"));}
        for(String name:SaveCommon.NAMES){File source=archive.files.get("saves/"+name);if(source==null)continue;File target=new File(saves,name);remember(plan,target);byte[] merged=SaveCommon.merge(name,target.isFile()?small(target):null,small(source));File staged=File.createTempFile("merged-", ".whp",archive.root);try(FileOutputStream stream=new FileOutputStream(staged)){stream.write(merged);stream.getFD().sync();}plan.replacements.put(target,staged);}
        return plan;
    }
    private static void remember(Plan plan,File file)throws IOException {if(Files.isSymbolicLink(file.toPath())||file.exists()&&!file.isFile())throw new IOException("Unsafe save target");plan.expected.put(file,file.isFile()?hash(file):"");}
    static Plan deletion(File saves,File thumbs,int slot,long modified,long size,String selectedHash)throws Exception {
        validSlot(slot);File save=new File(saves,"slot"+slot+".whs"),thumbnail=new File(thumbs,"slot"+slot+".thumb");
        if(!save.isFile()||save.lastModified()!=modified||save.length()!=size)throw new IOException("Selected save changed");
        Plan plan=new Plan(null,saves.getCanonicalFile(),thumbs.getCanonicalFile(),new TreeMap<>(),Collections.emptyList());remember(plan,save);remember(plan,thumbnail);
        if(!selectedHash.isEmpty()&&!selectedHash.equals(plan.expected.get(save)))throw new IOException("Selected save checksum changed");
        plan.replacements.put(save,null);plan.replacements.put(thumbnail,null);return plan;
    }
    static void apply(Plan plan,File journal,boolean overwriteApproved,CheckedAction afterFiles)throws Exception {
        if(!overwriteApproved&&!plan.overwrites.isEmpty())throw new IOException("Overwrite approval required");
        recover(plan.saves,plan.thumbs,journal);for(Map.Entry<File,String> item:plan.expected.entrySet()){String current=item.getKey().isFile()?hash(item.getKey()):"";if(!current.equals(item.getValue()))throw new IOException("Save data changed; import again");}
        plan.saves.mkdirs();plan.thumbs.mkdirs();journal.mkdirs();String id=UUID.randomUUID().toString();JSONArray operations=new JSONArray();
        try{for(Map.Entry<File,File> item:plan.replacements.entrySet()){File target=item.getKey();boolean thumb=target.getParentFile().getCanonicalFile().equals(plan.thumbs);operations.put(new JSONObject().put("kind",thumb?"thumb":"save").put("name",target.getName()).put("id",id).put("old",target.exists()).put("new",item.getValue()!=null));}
            // Persist intent before copying, so a killed process cannot strand large .new files.
            JSONObject record=new JSONObject().put("schema",1).put("committed",false).put("operations",operations);writeJournal(journal,record);
            for(Map.Entry<File,File> item:plan.replacements.entrySet())if(item.getValue()!=null){File source=item.getValue(),staged=sibling(item.getKey(),id,"new");try(InputStream in=new FileInputStream(source);FileOutputStream out=new FileOutputStream(staged)){copy(in,out,MAX_SAVE,new byte[65536]);out.getFD().sync();}if(!staged.setLastModified(source.lastModified()))throw new IOException("Cannot preserve imported save date");}
            for(int i=0;i<operations.length();i++){JSONObject row=operations.getJSONObject(i);File target=target(plan.saves,plan.thumbs,row),backup=sibling(target,id,"old"),staged=sibling(target,id,"new");if(row.getBoolean("old"))move(target,backup);if(row.getBoolean("new"))move(staged,target);}
            afterFiles.run();record.put("committed",true);writeJournal(journal,record);recover(plan.saves,plan.thumbs,journal);
        }catch(Exception error){try{if(new File(journal,"transaction.json").isFile())recover(plan.saves,plan.thumbs,journal);else cleanupPrepared(plan.saves,plan.thumbs,operations);}catch(Exception rollback){error.addSuppressed(rollback);}throw error;}
    }
    static void recover(File saves,File thumbs,File journal)throws Exception {
        File file=new File(journal,"transaction.json");if(!file.isFile())return;JSONObject record=new JSONObject(new String(small(file),java.nio.charset.StandardCharsets.UTF_8));if(record.optInt("schema")!=1)throw new IOException("Invalid save transaction");JSONArray rows=record.getJSONArray("operations");if(rows.length()>27)throw new IOException("Invalid transaction size");
        for(int i=rows.length()-1;i>=0;i--){JSONObject row=rows.getJSONObject(i);File target=target(saves,thumbs,row),backup=sibling(target,row.getString("id"),"old"),staged=sibling(target,row.getString("id"),"new");
            if(!record.getBoolean("committed")){if(backup.isFile())move(backup,target);else if(!row.getBoolean("old"))Files.deleteIfExists(target.toPath());}
            Files.deleteIfExists(backup.toPath());Files.deleteIfExists(staged.toPath());}
        Files.delete(file.toPath());
    }
    private static void cleanupPrepared(File saves,File thumbs,JSONArray rows)throws Exception {for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);Files.deleteIfExists(sibling(target(saves,thumbs,row),row.getString("id"),"new").toPath());}}
    private static File target(File saves,File thumbs,JSONObject row)throws Exception {String kind=row.getString("kind"),name=row.getString("name"),id=row.getString("id");if(!id.matches("[a-f0-9-]{36}")||!kind.equals("save")&&!kind.equals("thumb")||limit((kind.equals("thumb")?"thumbnails/":"saves/")+name)<0)throw new IOException("Invalid save transaction path");File parent=(kind.equals("thumb")?thumbs:saves).getCanonicalFile(),file=new File(parent,name).getCanonicalFile();if(!parent.equals(file.getParentFile())||Files.isSymbolicLink(file.toPath()))throw new IOException("Unsafe transaction path");return file;}
    private static File sibling(File file,String id,String suffix){return new File(file.getParentFile(),"."+file.getName()+"-"+id+"."+suffix);}
    private static void writeJournal(File folder,JSONObject record)throws IOException {File temp=new File(folder,"transaction.tmp");try(FileOutputStream out=new FileOutputStream(temp)){out.write(record.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));out.getFD().sync();}move(temp,new File(folder,"transaction.json"));}
    private static void move(File source,File target)throws IOException {try{Files.move(source.toPath(),target.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);}catch(AtomicMoveNotSupportedException unsupported){Files.move(source.toPath(),target.toPath(),StandardCopyOption.REPLACE_EXISTING);}}
    static long limit(String name){if(name.matches("saves/slot(?:[1-9]|1[0-2])\\.whs"))return MAX_SAVE;if(name.matches("thumbnails/slot(?:[1-9]|1[0-2])\\.thumb"))return 1024*1024+4108;if(name.startsWith("saves/")&&SaveCommon.NAMES.contains(name.substring(6)))return 1024*1024+48;return -1;}
    static int number(String name){return Integer.parseInt(name.substring(name.lastIndexOf("slot")+4,name.lastIndexOf('.')));}
    private static void validSlot(int slot){if(slot<1||slot>COUNT)throw new IllegalArgumentException("Invalid slot");}
    static String hash(File file)throws IOException {try{MessageDigest digest=MessageDigest.getInstance("SHA-256");try(InputStream in=new DigestInputStream(new BufferedInputStream(new FileInputStream(file)),digest)){byte[] buffer=new byte[65536];while(in.read(buffer)!=-1){}}return SlotSaveMetadata.hex(digest.digest());}catch(NoSuchAlgorithmException error){throw new IOException(error);}}
    static byte[] small(File file)throws IOException {if(file.length()>1024*1024+48)throw new IOException("Metadata exceeds limit");return Files.readAllBytes(file.toPath());}
    private static long copy(InputStream in,OutputStream out,long maximum,byte[] buffer)throws IOException {long total=0;int count;while((count=in.read(buffer))!=-1){if(count>maximum-total)throw new IOException("Backup data exceeds limit");out.write(buffer,0,count);total+=count;}return total;}
    private static void checkThumbnail(File file,SlotSaveMetadata save)throws IOException,JSONException {if(file.length()>1024*1024+4108)throw new IOException("Thumbnail too large");try(DataInputStream in=new DataInputStream(new FileInputStream(file))){if(in.readInt()!=0x57485431)throw new IOException("Invalid thumbnail");int length=in.readInt();if(length<1||length>4096)throw new IOException("Invalid thumbnail metadata");byte[] bytes=new byte[length];in.readFully(bytes);JSONObject row=new JSONObject(new String(bytes,java.nio.charset.StandardCharsets.UTF_8));int image=in.readInt();if(!row.optString("sha256").equals(save.sha256)||row.optLong("size")!=save.size||image<1||image>1024*1024||file.length()!=12L+length+image)throw new IOException("Thumbnail save binding mismatch");}}
    private static void erase(File root)throws IOException {if(!root.exists())return;try(java.util.stream.Stream<Path> paths=Files.walk(root.toPath())){for(Path path:(Iterable<Path>)paths.sorted(Comparator.reverseOrder())::iterator)Files.deleteIfExists(path);}}
    private SaveArchive(){}
}
