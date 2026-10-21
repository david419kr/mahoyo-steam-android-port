package org.mahoyo.port;

import android.graphics.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

/** Materialize original visual assets only from the owner's installed Steam files. */
final class SteamDerivedAssets {
    private static final String[] TABLES={"archive-catalog.tsv","bitmap-recipes.json","checkpoint-map.tsv","effects-properties.tsv","effects-window.tsv","effects.tsv","gallery-catalog.tsv","image-layout.tsv","image-origins.tsv","mask-map.tsv","music-catalog.tsv","music-map.tsv","native-exact-layouts.tsv","native-image-resources.tsv","wik-choice-layout.tsv"};
    static void prepare(File source,File root,Map<String,HfaIndex.Entry> entries,File patch)throws Exception{
        File meta=new File(root,"meta");SteamPe pe=null;
        File receipt=new File(meta,".steam-metadata-v1");boolean tables=!receipt.isFile();for(String name:TABLES)if(!new File(meta,name).isFile())tables=true;
        if(tables){
            pe=executable(source);pe.decodeMetadataCode();Map<String,List<String>> rows=messages(pe);
            HfaIndex.Entry archive=entries.get("archive.chs");if(archive==null)throw new IOException("Missing original archive script");byte[] script=NativeAssetCodec.lenzu(archive.read(1024*1024));
            if(!SteamResources.sha(script).equals("67bff02a80cb6e75f67285085f1a15b129730adea5eaf338121af3028ada23a0"))throw new IOException("Unsupported original archive script");
            Map<String,byte[]> generated=new SteamMetadata(pe).generate(script,rows);for(String name:TABLES){byte[] data=generated.get(name);if(data==null)throw new IOException("Incomplete original metadata");SteamResources.write(new File(meta,name),data);}
            SteamResources.write(receipt,SteamMetadata.NATIVE_SHA.getBytes(StandardCharsets.US_ASCII));
        }
        for(int mode=3;mode<=82;mode++){
            if(mode>3&&mode<8)continue;File out=new File(meta,"color"+mode+"-rgb.bin");
            if(!out.isFile()||out.length()!=768){if(pe==null)pe=executable(source);SteamResources.write(out,pe.palette(mode));}
        }
        for(int side:new int[]{20,64,80,128,256,512})for(String kind:new String[]{"normal","inv"}){
            String stem=String.format(Locale.ROOT,"%03d",side);File out=new File(meta,"window-atlas-"+kind+"-"+stem+".png");
            if(out.isFile()&&out.length()>0)continue;
            HfaIndex.Entry entry=entries.get("alphagradation"+(kind.equals("inv")?"_inv":"")+stem+".cbg");if(entry==null)throw new IOException("Missing original window mask");
            Bitmap image=NativeAssetCodec.bitmap(entry.read(16*1024*1024),null,new BitmapFactory.Options(),null);
            if(image==null)throw new IOException("Cannot decode original window mask");
            try(ByteArrayOutputStream bytes=new ByteArrayOutputStream()){if(!image.compress(Bitmap.CompressFormat.PNG,100,bytes))throw new IOException("Cannot cache original window mask");SteamResources.write(out,bytes.toByteArray());}finally{image.recycle();}
        }
        File messages=new File(meta,"language-messages.tsv");if(!messages.isFile()){
            if(pe==null)pe=executable(source);Map<String,List<String>> rows=messages(pe);
            String[] languages={"ja","en","zh-Hans","zh-Hant"};StringBuilder table=new StringBuilder("WOHMESSAGES1\n");
            for(String key:new TreeSet<>(rows.keySet()))for(int i=0;i<languages.length;i++)table.append(key).append(' ').append(languages[i]).append(" \"").append(rows.get(key).get(i+1).replace("\\","\\\\").replace("\"","\\\"")).append("\"\n");
            String text=table.toString();if(patch!=null)text=KoreanPatch.menuTable(Files.readAllBytes(new File(patch,"mahoyo_kr.dll").toPath()),text);SteamResources.write(messages,text.getBytes(StandardCharsets.UTF_8));
        }
    }
    private static Map<String,List<String>> messages(SteamPe pe)throws Exception{
        String csv=new String(NativeAssetCodec.lenzu(pe.resource("TEXT",5)),StandardCharsets.UTF_8);if(csv.startsWith("\ufeff"))csv=csv.substring(1);Map<String,List<String>> rows=KoreanPatch.csv(csv);if(rows.size()!=375)throw new IOException("Invalid original menu catalogue");return rows;
    }
    private static SteamPe executable(File source)throws Exception{return new SteamPe(new File(source,"WoH.exe"),SteamPe.SOURCE_SHA);}
    private SteamDerivedAssets(){}
}
