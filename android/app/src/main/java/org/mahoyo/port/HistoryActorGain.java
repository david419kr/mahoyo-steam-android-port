package org.mahoyo.port;

import java.util.Locale;

/** Native110D30/035070 mapping; settings remain independent of saved dialogue. */
final class HistoryActorGain {
    // Port default policy; original fresh-profile defaults were not established.
    static final int DEFAULT_STORED_VOLUME=128;
    static int actorForPath(String path){
        String name=path.substring(path.lastIndexOf('/')+1);String prefix=name.length()<3?"":name.substring(0,3).toUpperCase(Locale.ROOT);
        switch(prefix){
            case "A10":case "A11":return 1;case "A40":return 2;case "A30":return 3;case "B70":return 4;
            case "B10":case "B11":return 5;case "B50":return 6;case "B60":return 7;case "B40":case "B41":return 8;
            case "B30":return 9;case "B20":return 10;case "A20":case "A21":return 11;case "A50":return 12;
            case "B80":case "B81":return 13;case "B90":return 14;default:return 0;
        }
    }
    static int configIndex(int actor){if(actor<0||actor>14)throw new IllegalArgumentException("history-actor");return actor==0?14:actor-1;}
    static int percent(int stored,boolean enabled,boolean requireEnabled){if(stored<0||stored>128)throw new IllegalArgumentException("history-actor-volume");return requireEnabled&&!enabled?0:(stored*100)>>7;}
    static int defaultPercent(int actor){configIndex(actor);return percent(DEFAULT_STORED_VOLUME,true,false);}
}
