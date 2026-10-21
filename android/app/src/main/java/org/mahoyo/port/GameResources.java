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
        this.root=root.getCanonicalFile();steam=new SteamResources(context,this.root);korean=steam.korean();
        setLanguage(LanguagePolicy.resolve(korean,context.getSharedPreferences("display",Context.MODE_PRIVATE).getString("language","auto"),GameLanguage.primarySystemLocale(context)));
    }
    GameAsset file(String name){try{return steam.file(name,language);}catch(IOException error){throw new IllegalArgumentException(error);}}
    void setLanguage(String selected){if("auto".equals(selected)||!Arrays.asList(LanguagePolicy.choices(korean)).contains(selected))throw new IllegalArgumentException("Language not available in this edition");language=selected;}
    String language(){return language;}
    Collection<String> imageFiles(){return steam.imageFiles();}
}
