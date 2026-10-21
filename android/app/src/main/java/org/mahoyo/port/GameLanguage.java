package org.mahoyo.port;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.os.LocaleList;
import java.util.Arrays;
import java.util.Locale;

final class GameLanguage {
    static boolean korean(Context context){return LanguagePolicy.koreanPackage(context.getPackageName());}
    static String edition(Context context){return korean(context)?"korean":"vanilla";}
    static String preference(Context context){return LanguagePolicy.preference(korean(context),context.getSharedPreferences("display",Context.MODE_PRIVATE).getString("language","auto"));}
    static Locale primarySystemLocale(Context context){
        // Never derive Auto from the Activity's overridden UI locale.
        LocaleList languages;
        if(Build.VERSION.SDK_INT>=33){android.app.LocaleManager manager=context.getSystemService(android.app.LocaleManager.class);languages=manager==null?Resources.getSystem().getConfiguration().getLocales():manager.getSystemLocales();}
        else languages=Resources.getSystem().getConfiguration().getLocales();
        return languages.isEmpty()?Locale.ROOT:languages.get(0);
    }
    static String effective(Context context){return LanguagePolicy.resolve(korean(context),preference(context),primarySystemLocale(context));}
    static boolean save(Context context,String value){
        if(!Arrays.asList(LanguagePolicy.choices(korean(context))).contains(value))throw new IllegalArgumentException("Language not available in this edition");
        return context.getSharedPreferences("display",Context.MODE_PRIVATE).edit().putString("language",value).commit();
    }
    static Context localized(Context context){
        // Override only locale. Copying the whole current configuration pins old
        // dimensions/density/orientation when rotating Settings or folding the device.
        Configuration configuration=new Configuration();configuration.setLocales(new LocaleList(Locale.forLanguageTag(effective(context))));return context.createConfigurationContext(configuration);
    }
    private GameLanguage(){}
}
