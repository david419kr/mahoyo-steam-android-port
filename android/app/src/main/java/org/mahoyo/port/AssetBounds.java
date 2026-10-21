package org.mahoyo.port;
import java.util.*;
/** Safe generated resource paths and overflow-safe HFA byte ranges. */
final class AssetBounds {
    private static final Set<String> FOLDERS=new HashSet<>(Arrays.asList("audio","fonts","images","meta","scripts","text","video"));
    static boolean safe(String path){
        if(path.isEmpty()||path.startsWith("/")||path.indexOf('\\')>=0||path.indexOf(':')>=0)return false;
        for(int i=0;i<path.length();i++)if(path.charAt(i)<32)return false;
        for(String part:path.split("/",-1))if(part.isEmpty()||part.equals(".")||part.equals(".."))return false;
        int slash=path.indexOf('/');return path.equals("assets.json")||slash>0&&FOLDERS.contains(path.substring(0,slash));
    }
    static boolean range(long offset,long length,long total){return offset>=0&&length>=0&&offset<=total&&length<=total-offset;}
}
