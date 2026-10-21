package org.mahoyo.port;

import java.util.Arrays;
import java.util.Locale;

/** Platform-independent edition, preference and primary-system-locale policy. */
final class LanguagePolicy {
    static final String AUTO="auto";
    static String[] choices(boolean korean){return korean?new String[]{AUTO,"ko","ja","zh-Hans","zh-Hant"}:new String[]{AUTO,"ja","en","zh-Hans","zh-Hant"};}
    static String[] labels(boolean korean){return korean?new String[]{"Auto","한","日","简","繁"}:new String[]{"Auto","日","En","简","繁"};}
    static boolean koreanPackage(String packageName){
        if("org.mahoyo.port.kr".equals(packageName))return true;
        if("org.mahoyo.port".equals(packageName)||"org.mahoyo.port.unofficial".equals(packageName))return false;
        throw new IllegalArgumentException("Unknown game edition: "+packageName);
    }
    static String preference(boolean korean,String requested){return Arrays.asList(choices(korean)).contains(requested)?requested:AUTO;}
    static String resolve(boolean korean,String requested,Locale primary){
        String selected=preference(korean,requested);
        if(!AUTO.equals(selected))return selected;
        String language=primary==null?"":primary.getLanguage();
        if("zh".equals(language)){
            String script=primary.getScript(),region=primary.getCountry();
            selected="Hant".equalsIgnoreCase(script)?"zh-Hant":"Hans".equalsIgnoreCase(script)?"zh-Hans":
                "TW".equalsIgnoreCase(region)||"HK".equalsIgnoreCase(region)||"MO".equalsIgnoreCase(region)?"zh-Hant":"zh-Hans";
        }else selected=language;
        return !AUTO.equals(selected)&&Arrays.asList(choices(korean)).contains(selected)?selected:korean?"ko":"en";
    }
    static int slot(String language){switch(language){case "ja":return 0;case "en":case "ko":return 1;case "zh-Hans":return 2;case "zh-Hant":return 3;default:throw new IllegalArgumentException("Unsupported game language: "+language);}}
    static String suffix(String language){return new String[]{"ja","en","zc","zt"}[slot(language)];}
    private LanguagePolicy(){}
}
