package org.mahoyo.port;

import org.json.JSONArray;
import org.json.JSONObject;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.charset.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/** Reads verified patch data only. The Windows DLL is never loaded or executed. */
final class KoreanPatch {
    interface Progress { void update(long done, long total) throws IOException; }

    static void unpack(InputStream input, File destination, JSONArray profile, Progress progress) throws Exception {
        Map<String,JSONObject> wanted = new HashMap<>();
        long total = 0;
        for (int i=0; i<profile.length(); i++) {
            JSONObject row = profile.getJSONObject(i);
            String name = row.getString("name").toLowerCase(Locale.ROOT);
            if (!name.matches("mahoyo_(kr\\.dll|kr[123]\\.dat)") || wanted.put(name,row)!=null) throw new IOException("Invalid patch profile");
            total += row.getLong("bytes");
        }
        if (wanted.size()!=4) throw new IOException("Incomplete patch profile");
        if (!destination.isDirectory() && !destination.mkdirs()) throw new IOException("Cannot create patch directory");
        Set<String> found = new HashSet<>();
        long done=0, scanned=0, reported=0;
        int count=0;
        byte[] buffer = new byte[1024*1024];
        try (ZipInputStream zip = new ZipInputStream(input, StandardCharsets.UTF_8)) {
            ZipEntry entry;
            while ((entry=zip.getNextEntry())!=null) {
                if (++count>100000) throw new IOException("Patch ZIP entry budget");
                // Only our own leaf names become output paths, regardless of the ZIP's folder tree.
                String path=entry.getName().replace('\\','/');
                String leaf=path.substring(path.lastIndexOf('/')+1).toLowerCase(Locale.ROOT);
                JSONObject row=wanted.get(leaf);
                if (entry.isDirectory()) { zip.closeEntry(); continue; }
                long limit=row==null?4L*1024*1024*1024:row.getLong("bytes");
                if (row!=null && entry.getSize()>=0 && entry.getSize()!=limit) throw new IOException("Unsupported patch file size: "+leaf);
                MessageDigest hash=row==null?null:MessageDigest.getInstance("SHA-256");
                boolean duplicate=found.contains(leaf);
                File target=row==null?null:new File(destination,row.getString("name"));
                try (OutputStream out=row==null||duplicate?new OutputStream(){public void write(int b){} public void write(byte[] b,int o,int n){}}:new FileOutputStream(target)) {
                    int n; long size=0;
                    while ((n=zip.read(buffer))!=-1) {
                        if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Patch import canceled");
                        size+=n; scanned+=n;
                        if (size>limit || scanned>Math.max(8L*1024*1024*1024,total*3)) throw new IOException("Patch ZIP expansion budget");
                        if (hash!=null) { hash.update(buffer,0,n); if(!duplicate) done+=n; }
                        out.write(buffer,0,n);
                        if (done-reported>=8*1024*1024) { reported=done; progress.update(done,total); }
                    }
                    if (row!=null && (size!=limit || !hex(hash.digest()).equals(row.getString("sha256")))) throw new IOException("Unsupported or corrupt patch file: "+leaf);
                }
                if (row!=null) found.add(leaf);
                zip.closeEntry();
            }
        }
        if (!found.equals(wanted.keySet())) throw new IOException("Korean patch 1.2 ZIP is missing required files");
        progress.update(total,total);
    }

    static LinkedHashMap<String,HfaIndex.Entry> members(File patch, Set<String> originals) throws IOException {
        LinkedHashMap<String,HfaIndex.Entry> result=new LinkedHashMap<>();
        for (int number=1; number<=3; number++) {
            for (HfaIndex.Entry entry:HfaIndex.read(new File(patch,"mahoyo_KR"+number+".dat")).values()) {
                String logical=entry.name;
                if (logical.endsWith(".new")) {
                    String stem=logical.substring(0,logical.length()-4), match=null;
                    for (String ext:new String[]{".cbg",".mzp",".ctd",".ccit"}) if(originals.contains(stem+ext)) {
                        if(match!=null) throw new IOException("Ambiguous patch alias: "+logical);
                        match=stem+ext;
                    }
                    if(match==null && stem.matches("font_en_(?:italic_|font3_)?[0-9]+")) match=stem+".mzp";
                    if(match==null) throw new IOException("Unknown patch alias: "+logical);
                    logical=match;
                }
                if(result.put(logical,entry)!=null) throw new IOException("Duplicate Korean resource: "+logical);
            }
        }
        if(!result.containsKey("script_text_en.ctd")) throw new IOException("Korean text table missing");
        return result;
    }

    static String menuTable(byte[] dll, String original) throws IOException {
        byte[] marker="\ufeffSYSMES_CONF_000,".getBytes(StandardCharsets.UTF_8);
        int start=-1;
        for(int i=0;i<=dll.length-marker.length;i++) {
            int j=0;while(j<marker.length && dll[i+j]==marker[j])j++;
            if(j==marker.length) { if(start>=0)throw new IOException("Ambiguous Korean menu table"); start=i; }
        }
        if(start<0)throw new IOException("Korean menu table missing");
        int end=start;while(end<dll.length && dll[end]!=0)end++;
        if(end==dll.length || end-start>1024*1024)throw new IOException("Korean menu table bounds");
        String csv;
        try{csv=StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(dll,start+3,end-start-3)).toString();}
        catch(CharacterCodingException e){throw new IOException("Korean menu encoding",e);}
        Map<String,List<String>> rows=csv(csv);
        Set<String> base=new HashSet<>();
        for(String line:original.split("\n"))if(line.startsWith("SYSMES_"))base.add(line.substring(0,line.indexOf(' ')));
        if(rows.size()!=375 || !rows.keySet().equals(base))throw new IOException("Korean menu catalogue differs");
        StringBuilder output=new StringBuilder(original);
        if(!original.endsWith("\n"))output.append('\n');
        for(String key:new TreeSet<>(rows.keySet()))output.append(key).append(" ko \"").append(rows.get(key).get(2).replace("\\","\\\\").replace("\"","\\\"")).append("\"\n");
        return output.toString();
    }

    private static Map<String,List<String>> csv(String value)throws IOException {
        Map<String,List<String>> result=new LinkedHashMap<>();List<String> row=new ArrayList<>();StringBuilder field=new StringBuilder();boolean quoted=false;
        for(int i=0;i<=value.length();i++) {
            char c=i==value.length()?'\n':value.charAt(i);
            if(c=='"') { if(quoted && i+1<value.length() && value.charAt(i+1)=='"'){field.append('"');i++;}else quoted=!quoted; }
            else if(!quoted && (c==',' || c=='\n' || c=='\r')) {
                row.add(field.toString());field.setLength(0);
                if(c!=',') {
                    if(c=='\r' && i+1<value.length() && value.charAt(i+1)=='\n')i++;
                    if(row.size()!=1 || !row.get(0).isEmpty()) {
                        if(row.size()!=5 || !row.get(0).startsWith("SYSMES_") || result.put(row.get(0),row)!=null)throw new IOException("Invalid Korean menu row");
                    }
                    row=new ArrayList<>();
                }
            }else field.append(c);
            if(field.length()>4096)throw new IOException("Korean menu field budget");
        }
        if(quoted)throw new IOException("Unclosed Korean menu field");
        return result;
    }
    private static String hex(byte[] bytes){StringBuilder text=new StringBuilder();for(byte b:bytes)text.append(String.format(Locale.ROOT,"%02x",b&255));return text.toString();}
    private KoreanPatch(){}
}
