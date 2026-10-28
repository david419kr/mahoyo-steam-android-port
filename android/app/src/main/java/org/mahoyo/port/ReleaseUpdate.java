package org.mahoyo.port;

import java.io.IOException;
import java.net.URI;
import java.util.regex.*;
import org.json.*;

/** Strict public-release metadata and semantic version ordering. */
final class ReleaseUpdate {
    static final String REPOSITORY="david419kr/mahoyo-steam-android-port",API="https://api.github.com/repos/"+REPOSITORY+"/releases/latest";
    final String tag,version,url,page,digest;final long size;
    private ReleaseUpdate(String tag,String version,String url,String page,String digest,long size){this.tag=tag;this.version=version;this.url=url;this.page=page;this.digest=digest;this.size=size;}
    static ReleaseUpdate parse(String json)throws IOException {
        try{JSONObject root=new JSONObject(json);if(root.optBoolean("draft")||root.optBoolean("prerelease"))throw new IOException("Not a stable release");String tag=root.getString("tag_name");Version release=Version.parse(tag);if(!release.suffix.isEmpty())throw new IOException("Prerelease tag");JSONArray assets=root.getJSONArray("assets");ReleaseUpdate found=null;
            for(int i=0;i<assets.length();i++){JSONObject asset=assets.getJSONObject(i);if(!asset.optString("name").equals("mahoyo-unofficial.apk"))continue;if(found!=null)throw new IOException("Duplicate APK asset");String url=asset.getString("browser_download_url");URI uri=URI.create(url);String expected="https://github.com/"+REPOSITORY+"/releases/download/"+tag+"/mahoyo-unofficial.apk";
                if(!url.equals(expected)||!"https".equals(uri.getScheme())||uri.getRawUserInfo()!=null)throw new IOException("Unexpected release download URL");long size=asset.getLong("size");if(size<1||size>256L*1024*1024)throw new IOException("Invalid APK size");String digest=asset.optString("digest","");if(!digest.isEmpty()&&!digest.matches("sha256:[0-9a-f]{64}"))throw new IOException("Invalid APK digest");
                found=new ReleaseUpdate(tag,tag.startsWith("v")?tag.substring(1):tag,url,"https://github.com/"+REPOSITORY+"/releases/tag/"+tag,digest,size);
            }if(found==null)throw new IOException("Release has no APK");return found;
        }catch(JSONException|IllegalArgumentException error){throw new IOException("Invalid release metadata",error);}
    }
    boolean newerThan(String installed)throws IOException {return Version.parse(version).compareTo(Version.parse(installed))>0;}
    static final class Version implements Comparable<Version> {
        final int[] parts;final String suffix;
        Version(int[] parts,String suffix){this.parts=parts;this.suffix=suffix;}
        static Version parse(String text)throws IOException {Matcher m=Pattern.compile("v?(\\d{1,6})\\.(\\d{1,6})\\.(\\d{1,6})(?:-([A-Za-z0-9.-]+))?").matcher(text==null?"":text);if(!m.matches())throw new IOException("Invalid version");return new Version(new int[]{Integer.parseInt(m.group(1)),Integer.parseInt(m.group(2)),Integer.parseInt(m.group(3))},m.group(4)==null?"":m.group(4));}
        public int compareTo(Version other){for(int i=0;i<parts.length;i++){int n=Integer.compare(parts[i],other.parts[i]);if(n!=0)return n;}if(suffix.isEmpty()!=other.suffix.isEmpty())return suffix.isEmpty()?1:-1;return suffix.compareTo(other.suffix);}
    }
}
