package org.mahoyo.port;
import android.content.Context;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
/** Installed Steam resources only; original media remain in HFA archives. */
final class GameResources {
    private final File root;private final SteamResources steam;private final boolean korean;private volatile String language;
    static boolean available(Context context,File root){return SteamResources.available(root);}
    GameResources(Context context,File root)throws Exception{
        this.root=root.getCanonicalFile();steam=new SteamResources(context,this.root);korean=steam.korean();installExactLayouts(context);
        setLanguage(LanguagePolicy.resolve(korean,context.getSharedPreferences("display",Context.MODE_PRIVATE).getString("language","auto"),GameLanguage.primarySystemLocale(context)));
    }
    private void installExactLayouts(Context context)throws IOException{
        byte[] bytes;try(InputStream in=context.getAssets().open("dialogue/exact-layouts.tsv");ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] buffer=new byte[8192];int n;while((n=in.read(buffer))!=-1){if(out.size()>2*1024*1024-n)throw new IOException("Native layout metadata exceeds budget");out.write(buffer,0,n);}bytes=out.toByteArray();}
        File target=local("meta/native-exact-layouts.tsv");if(!target.isFile()||target.length()!=bytes.length||!Arrays.equals(Files.readAllBytes(target.toPath()),bytes))SteamResources.write(target,bytes);
    }
    private File local(String path)throws IOException{if(!AssetBounds.safe(path))throw new IOException("Unsafe asset path");File f=new File(root,path).getCanonicalFile();if(!f.getPath().startsWith(root.getPath()+File.separator))throw new IOException("Asset outside game root");return f;}
    GameAsset file(String name){try{return steam.file(name,language);}catch(IOException error){throw new IllegalArgumentException(error);}}
    void setLanguage(String selected){if("auto".equals(selected)||!Arrays.asList(LanguagePolicy.choices(korean)).contains(selected))throw new IllegalArgumentException("Language not available in this edition");language=selected;}
    String language(){return language;}
    Collection<String> imageFiles(){return steam.imageFiles();}
}
