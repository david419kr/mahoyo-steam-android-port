package org.mahoyo.port;

import android.content.Context;
import android.util.AtomicFile;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/** Loose developer data or a stored ZIP64 OBB; only small engine metadata is materialized. */
final class GameResources {
    static final String OBB_NAME="main.1.org.mahoyo.port.obb";
    private final File root,obb;private final Map<String,ObbIndex.Entry> entries;
    private final JSONObject manifest,overrides;
    private final boolean korean;
    private volatile String language;
    private final java.util.concurrent.ConcurrentHashMap<String,String> resolvedNames=new java.util.concurrent.ConcurrentHashMap<>();
    private static final java.util.regex.Pattern LOCALIZED=java.util.regex.Pattern.compile("_(ja|en|zc|zt)(?=[_.])");
    static String obbName(Context context){return "main.1."+context.getPackageName()+".obb";}
    static File obb(Context context){File dir=context.getObbDir();return dir==null?null:new File(dir,obbName(context));}
    static boolean available(Context context,File root){File packed=obb(context);return packed!=null&&packed.isFile()||!new File(root,".obb-ready").exists()&&new File(root,"assets.json").isFile();}
    GameResources(Context context,File root)throws Exception{
        this.root=root.getCanonicalFile();File candidate=obb(context);obb=candidate!=null&&candidate.isFile()?candidate.getCanonicalFile():null;
        byte[] manifestBytes=null;JSONObject validatedManifest=null;
        if(obb!=null){
            try(ZipFile zip=new ZipFile(obb)){
                byte[] index=read(zip,"_obb/index.tsv",16*1024*1024);JSONObject info=new JSONObject(new String(read(zip,"_obb/info.json",65536),StandardCharsets.UTF_8));
                if(!"WOHOBB1".equals(info.getString("format"))||info.getInt("dataVersion")!=1||!sha(index).equals(info.getString("indexSha256")))throw new IOException("OBB identity/index mismatch");
                int minimum=info.optInt("minimumApkVersionCode",1);if(minimum<1||minimum>context.getPackageManager().getPackageInfo(context.getPackageName(),0).getLongVersionCode())throw new IOException("This OBB requires a newer app version");
                // Validate the edition before writing even one engine-cache file.
                manifestBytes=read(zip,"assets.json",64*1024*1024);
                if(!sha(manifestBytes).equals(info.getString("manifestSha256")))throw new IOException("OBB manifest mismatch");
                validatedManifest=new JSONObject(new String(manifestBytes,StandardCharsets.UTF_8));validateEdition(context,validatedManifest,info);
                entries=ObbIndex.parse(index,obb.length());if(info.getInt("entries")!=entries.size()||zip.size()!=entries.size()+2)throw new IOException("OBB entry count mismatch");
                for(ObbIndex.Entry entry:entries.values()){ZipEntry member=zip.getEntry(entry.path);if(member==null||member.getMethod()!=ZipEntry.STORED||member.getSize()!=entry.length||member.getCrc()!=entry.crc)throw new IOException("OBB entry differs from index: "+entry.path);}
                String stamp=sha(index)+"\n"+obb.getCanonicalPath()+"\n"+obb.length()+"\n";File ready=new File(root,".obb-ready");
                File localIndex=new File(root,"meta/obb-index.tsv"),localSource=new File(root,"meta/obb-source.txt");
                boolean cached=ready.isFile()&&ready.length()<=4096&&stamp.equals(new String(Files.readAllBytes(ready.toPath()),StandardCharsets.UTF_8))&&localIndex.isFile()&&localIndex.length()==index.length&&sha(Files.readAllBytes(localIndex.toPath())).equals(info.getString("indexSha256"))&&localSource.isFile()&&localSource.length()<=4096&&(obb.getCanonicalPath()+"\n").equals(new String(Files.readAllBytes(localSource.toPath()),StandardCharsets.UTF_8));
                long bytes=0;for(ObbIndex.Entry entry:entries.values())if(engine(entry.path)){bytes+=entry.length;if(entry.length>64*1024*1024||bytes>256*1024*1024)throw new IOException("OBB metadata budget");File local=local(entry.path);if(!cached||!local.isFile()||local.length()!=entry.length)extract(zip,entry,local);}
                File dimensions=local("meta/image-dimensions.tsv");if(!entries.containsKey("meta/image-dimensions.tsv")&&dimensions.exists()&&!dimensions.delete())throw new IOException("Cannot remove obsolete image dimensions");
                for(String optional:new String[]{"meta/game-edition.txt","meta/language-assets.tsv","meta/ko-image-dimensions.tsv","meta/language-messages.tsv"}){
                    File obsolete=local(optional);if(!entries.containsKey(optional)&&obsolete.exists()&&!obsolete.delete())throw new IOException("Cannot remove obsolete edition metadata");
                }
                if(bytes!=info.getLong("engineBytes"))throw new IOException("OBB metadata total mismatch");
                manifestBytes=Files.readAllBytes(new File(root,"assets.json").toPath());
                if(!sha(manifestBytes).equals(info.getString("manifestSha256")))throw new IOException("OBB manifest mismatch");
                if(!cached){write(localIndex,index);write(localSource,(obb.getCanonicalPath()+"\n").getBytes(StandardCharsets.UTF_8));write(ready,stamp.getBytes(StandardCharsets.UTF_8));}
            }
        }else{if(new File(root,".obb-ready").exists())throw new IOException("Required OBB is missing");entries=null;}
        installExactLayouts(context);
        if(manifestBytes==null)manifestBytes=Files.readAllBytes(new File(root,"assets.json").toPath());JSONObject full=validatedManifest!=null?validatedManifest:new JSONObject(new String(manifestBytes,StandardCharsets.UTF_8));manifest=full.getJSONObject("assets");korean=GameLanguage.korean(context);JSONObject edition=full.optJSONObject("edition");
        if(edition==null){if(korean)throw new IOException("This app requires the Korean edition OBB");}
        else if(!edition.optString("edition","").equals(korean?"korean":"vanilla"))throw new IOException("App/OBB edition mismatch");
        JSONObject allOverrides=full.optJSONObject("overrides");overrides=allOverrides==null?null:allOverrides.optJSONObject("ko");
        if(korean&&(overrides==null||!overrides.has("script_text_en.ctd")))throw new IOException("Korean text resources missing");
        setLanguage(GameLanguage.effective(context));
    }
    private static void validateEdition(Context context,JSONObject full,JSONObject info)throws Exception{
        String expected=GameLanguage.edition(context),declared=info.optString("edition","");
        if(!declared.isEmpty()&&!expected.equals(declared))throw new IOException("App/OBB edition mismatch");
        JSONObject profile=full.optJSONObject("edition");
        if(profile==null){if(GameLanguage.korean(context))throw new IOException("This app requires the Korean edition OBB");return;}
        if(!expected.equals(profile.optString("edition"))||profile.optInt("schema",1)!=1)throw new IOException("Unsupported game edition profile");
        String fingerprint=info.optString("sourceFingerprint","");
        if(!fingerprint.isEmpty()&&!fingerprint.equals(profile.optString("sourceFingerprint")))throw new IOException("OBB source profile mismatch");
        java.util.HashSet<String> available=new java.util.HashSet<>();org.json.JSONArray languages=profile.optJSONArray("languages");
        if(languages!=null)for(int i=0;i<languages.length();++i)available.add(languages.getString(i));
        java.util.HashSet<String> wanted=new java.util.HashSet<>(java.util.Arrays.asList(LanguagePolicy.choices(GameLanguage.korean(context))));wanted.remove("auto");
        if(!available.equals(wanted))throw new IOException("OBB language catalogue does not match this app");
    }
    private void installExactLayouts(Context context)throws IOException{
        byte[] bytes;try(InputStream in=context.getAssets().open("dialogue/exact-layouts.tsv");ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] buffer=new byte[8192];int n;while((n=in.read(buffer))!=-1){if(out.size()>2*1024*1024-n)throw new IOException("Native layout metadata exceeds budget");out.write(buffer,0,n);}bytes=out.toByteArray();}
        File target=local("meta/native-exact-layouts.tsv");if(!target.isFile()||target.length()!=bytes.length||!Arrays.equals(Files.readAllBytes(target.toPath()),bytes))write(target,bytes);
    }
    private File local(String path)throws IOException{if(!ObbIndex.safe(path))throw new IOException("Unsafe asset path");File f=new File(root,path).getCanonicalFile();if(!f.getPath().startsWith(root.getPath()+File.separator))throw new IOException("Asset outside game root");return f;}
    GameAsset file(String name){try{
        String logical=logicalName(name.toLowerCase(Locale.ROOT));String current=language;String selected=resolveName(logical,current);JSONObject record=record(selected,current);String path=record==null?name:record.optString("path",name);File f=local(path);
        if(entries==null||engine(path))return GameAsset.loose(f);
        ObbIndex.Entry entry=entries.get(path);if(entry==null)throw new IOException("Missing OBB resource: "+name);return GameAsset.packed(obb,entry);
    }catch(IOException e){throw new IllegalArgumentException(e);}}
    void setLanguage(String selected){if("auto".equals(selected)||!java.util.Arrays.asList(LanguagePolicy.choices(korean)).contains(selected))throw new IllegalArgumentException("Language not available in this edition");language=selected;resolvedNames.clear();}
    String language(){return language;}
    private JSONObject record(String name,String current){JSONObject row="ko".equals(current)&&overrides!=null?overrides.optJSONObject(name):null;return row!=null?row:manifest.optJSONObject(name);}
    private String logicalName(String name){
        String candidate=name;
        if(name.startsWith("images/")&&name.endsWith(".png"))candidate=name.substring(7,name.length()-4);
        else if(name.startsWith("audio/")&&name.endsWith(".ogg"))candidate=name.substring(6,name.length()-4);
        if(candidate.indexOf('/')<0&&(manifest.has(candidate)||overrides!=null&&overrides.has(candidate)))return candidate;
        return name;
    }
    private String resolveName(String name,String current){return resolvedNames.computeIfAbsent(current+":"+name,ignored->{java.util.regex.Matcher matcher=LOCALIZED.matcher(name);if(matcher.find()){String candidate=name.substring(0,matcher.start())+"_"+LanguagePolicy.suffix(current)+name.substring(matcher.end());if(record(candidate,current)!=null)return candidate;}return name;});}
    Collection<String> imageFiles(){java.util.LinkedHashSet<String> result=new java.util.LinkedHashSet<>();for(java.util.Iterator<String> names=manifest.keys();names.hasNext();){String name=names.next();JSONObject row=manifest.optJSONObject(name);if(row!=null&&"image".equals(row.optString("kind")))result.add("images/"+name+".png");}if("ko".equals(language)&&overrides!=null)for(java.util.Iterator<String> names=overrides.keys();names.hasNext();){String name=names.next();JSONObject row=overrides.optJSONObject(name);if(row!=null&&"image".equals(row.optString("kind")))result.add("images/"+name+".png");}return result;}
    private static boolean engine(String path){return path.equals("assets.json")||path.startsWith("scripts/")||path.startsWith("text/")||path.startsWith("meta/")||path.startsWith("fonts/");}
    private static byte[] read(ZipFile zip,String name,int cap)throws IOException{ZipEntry e=zip.getEntry(name);if(e==null||e.getSize()<0||e.getSize()>cap)throw new IOException("Missing/oversized OBB metadata");try(InputStream in=zip.getInputStream(e);ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1){if(out.size()>cap-n)throw new IOException("OBB metadata cap");out.write(b,0,n);}return out.toByteArray();}}
    private static String sha(byte[] bytes)throws Exception{StringBuilder out=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(bytes))out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();}
    private static void write(File file,byte[] bytes)throws IOException{File parent=file.getParentFile();if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Cannot create metadata directory");AtomicFile target=new AtomicFile(file);FileOutputStream out=null;try{out=target.startWrite();out.write(bytes);target.finishWrite(out);}catch(IOException e){if(out!=null)target.failWrite(out);throw e;}}
    private static void extract(ZipFile zip,ObbIndex.Entry entry,File file)throws IOException{
        File parent=file.getParentFile();if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Cannot create metadata directory");AtomicFile target=new AtomicFile(file);FileOutputStream out=null;
        try(InputStream in=zip.getInputStream(zip.getEntry(entry.path))){out=target.startWrite();CRC32 crc=new CRC32();byte[] b=new byte[65536];long total=0;int n;while((n=in.read(b))!=-1){total+=n;if(total>entry.length)throw new IOException("OBB metadata size mismatch");out.write(b,0,n);crc.update(b,0,n);}if(total!=entry.length||crc.getValue()!=entry.crc)throw new IOException("OBB metadata CRC mismatch");target.finishWrite(out);}catch(IOException e){if(out!=null)target.failWrite(out);throw e;}
    }
}
