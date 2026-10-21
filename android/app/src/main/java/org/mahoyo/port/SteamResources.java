package org.mahoyo.port;
import android.content.Context;
import android.util.AtomicFile;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.function.Consumer;
/** Original HFA media stay packed; only scripts and bounded engine metadata are materialized. */
final class SteamResources {
 static final String MARKER=".steam-ready";
 private final File root,source;private final LinkedHashMap<String,HfaIndex.Entry> entries;private final Map<String,String[]> recipes=new HashMap<>();
 private final JSONObject manifest;private final LinkedHashMap<String,HfaIndex.Entry> koreanEntries;private final boolean korean;
 SteamResources(Context context,File root)throws Exception{
  this.root=root.getCanonicalFile();JSONObject ready=new JSONObject(new String(Files.readAllBytes(new File(root,MARKER).toPath()),StandardCharsets.UTF_8));if(ready.getInt("schema")!=1)throw new IOException("Unsupported Steam installation record");source=new File(ready.getString("source")).getCanonicalFile();entries=HfaIndex.directory(source);manifest=new JSONObject(new String(Files.readAllBytes(new File(root,"assets.json").toPath()),StandardCharsets.UTF_8));
  korean=ready.has("patch");koreanEntries=korean?KoreanPatch.members(new File(ready.getString("patch")),entries.keySet()):new LinkedHashMap<>();
  SteamDerivedAssets.prepare(source,root,entries,korean?new File(ready.getString("patch")):null);
  JSONObject profile=new JSONObject(new String(read(new FileInputStream(new File(root,"meta/bitmap-recipes.json")),4*1024*1024),StandardCharsets.UTF_8));JSONArray rows=profile.getJSONArray("records");for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);recipes.put(row.getString("key"),new String[]{row.getString("primary"),row.isNull("alias")?"":row.getString("alias")});}
 }
 boolean korean(){return korean;}
 File source(){return source;}
 static boolean available(File root){return new File(root,MARKER).isFile();}
 static boolean sourceAvailable(Context context,File root)throws Exception{
  JSONObject ready=new JSONObject(new String(read(new FileInputStream(new File(root,MARKER)),4096),StandardCharsets.UTF_8));File source=new File(ready.getString("source"));JSONObject profile=assetJson(context,"steam-profile/source-profile.json",1024*1024);JSONArray files=profile.getJSONArray("archives");
  for(int i=0;i<files.length();i++){JSONObject row=files.getJSONObject(i);File file=new File(source,row.getString("name"));if(!file.isFile()||file.length()!=row.getLong("bytes"))return false;}
  if(ready.has("patch")){File patch=new File(ready.getString("patch"));files=profile.getJSONObject("koreanPatch").getJSONArray("files");for(int i=0;i<files.length();i++){JSONObject row=files.getJSONObject(i);File file=new File(patch,row.getString("name"));if(!file.isFile()||file.length()!=row.getLong("bytes"))return false;}}
  return true;
 }
 JSONObject manifest(){return manifest;}
 Collection<String> imageFiles(){List<String> names=new ArrayList<>();for(String name:entries.keySet())if(name.endsWith(".mzp")||name.endsWith(".cbg"))names.add("images/"+name+".png");return names;}
 private String localize(String name,String language){java.util.regex.Matcher m=java.util.regex.Pattern.compile("_(ja|en|zc|zt)(?=[_.])").matcher(name);if(m.find()){String candidate=name.substring(0,m.start())+"_"+LanguagePolicy.suffix(language)+name.substring(m.end());if(entries.containsKey(candidate)||language.equals("ko")&&koreanEntries.containsKey(candidate))return candidate;}return name;}
 private HfaIndex.Entry entry(String name,String language){HfaIndex.Entry override=language.equals("ko")?koreanEntries.get(name):null;return override!=null?override:entries.get(name);}
 GameAsset file(String requested,String language)throws IOException{
  String name=requested.toLowerCase(Locale.ROOT);
  if(name.startsWith("images/")&&name.endsWith(".png"))name=name.substring(7,name.length()-4);
  else if(name.startsWith("audio/")&&name.endsWith(".ogg"))name=name.substring(6,name.length()-4);
  else if(name.startsWith("video/"))name=name.substring(6);
  if(name.startsWith("ko/"))name=name.substring(3);
  name=localize(name,language);HfaIndex.Entry entry=entry(name,language);
  if(entry==null){if(!AssetBounds.safe(requested))throw new IOException("Unsafe Steam resource path");File local=new File(root,requested).getCanonicalFile();if(!local.getPath().startsWith(root.getPath()+File.separator)||!local.isFile())throw new IOException("Missing Steam resource: "+requested);return GameAsset.loose(local);}
  if(name.endsWith(".mzp")){String[] recipe=recipes.get(name);if(recipe!=null){HfaIndex.Entry primary=entry(recipe[0],language);if(primary==null)throw new IOException("Native primary image missing");if(!recipe[1].isEmpty()){HfaIndex.Entry patch=entry(recipe[1],language);if(patch==null)throw new IOException("Native patch image missing");return GameAsset.composite(raw(primary),raw(patch));}return raw(primary);}}
  if(name.endsWith(".hw")){byte[] header=entry.prefix(68);if(HfaIndex.unsigned(header,0)!=64||header[4]!='h'||header[5]!='w'||header[64]!='O'||header[65]!='g'||header[66]!='g'||header[67]!='S'||HfaIndex.unsigned(header,8)!=entry.length-64)throw new IOException("Invalid HW audio wrapper");return raw(entry.slice(64,entry.length-64));}
  return raw(entry);
 }
 private static GameAsset raw(HfaIndex.Entry entry){return GameAsset.range(entry.file,entry.offset,entry.length);}
 static byte[] read(InputStream in,int cap)throws IOException{try(InputStream stream=in;ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] buffer=new byte[65536];int n;while((n=stream.read(buffer))!=-1){if(out.size()>cap-n)throw new IOException("Steam metadata budget");out.write(buffer,0,n);}return out.toByteArray();}}
 static JSONObject assetJson(Context context,String name,int cap)throws Exception{return new JSONObject(new String(read(context.getAssets().open(name),cap),StandardCharsets.UTF_8));}
 static String sha(byte[] bytes)throws Exception{StringBuilder text=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(bytes))text.append(String.format(Locale.ROOT,"%02x",b&255));return text.toString();}
 static void write(File file,byte[] bytes)throws IOException{File parent=file.getParentFile();if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Cannot create engine data directory");AtomicFile value=new AtomicFile(file);FileOutputStream out=null;try{out=value.startWrite();out.write(bytes);value.finishWrite(out);}catch(IOException e){if(out!=null)value.failWrite(out);throw e;}}
 static String localPath(String name){if(name.endsWith(".chs"))return "scripts/"+name;if(name.endsWith(".ctd"))return "text/"+name;if(name.endsWith(".ccit"))return "fonts/"+name+".bin";return "";}
 static void prepare(Context context,File source,File output,Consumer<String> progress)throws Exception{
  prepare(context,source,output,null,progress);
 }
 static void prepare(Context context,File source,File output,File patch,Consumer<String> progress)throws Exception{
  source=source.getCanonicalFile();output=output.getCanonicalFile();if(output.equals(source)||output.getPath().startsWith(source.getPath()+File.separator))throw new IOException("Engine output must be outside Steam source");if(!output.isDirectory()&&!output.mkdirs())throw new IOException("Cannot create engine directory");
  LinkedHashMap<String,HfaIndex.Entry> entries=HfaIndex.directory(source);JSONObject profile=assetJson(context,"steam-profile/source-profile.json",1024*1024);
  if(progress!=null)progress.accept("Preparing original engine metadata");
  SteamDerivedAssets.prepare(source,output,entries,null);
  JSONObject assets=new JSONObject();int done=0;long engineBytes=0;
  for(Map.Entry<String,HfaIndex.Entry> item:entries.entrySet()){
   if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Installation canceled");
   String name=item.getKey(),path=localPath(name);HfaIndex.Entry entry=item.getValue();JSONObject row=new JSONObject();
   if(!path.isEmpty()){
    byte[] encoded=entry.read(64*1024*1024),decoded=encoded.length>=15&&encoded[0]=='L'&&encoded[1]=='e'&&encoded[2]=='n'?NativeAssetCodec.lenzu(encoded):encoded;engineBytes+=decoded.length;if(engineBytes>256L*1024*1024)throw new IOException("Engine metadata total budget");write(new File(output,path),decoded);row.put("kind",name.endsWith(".chs")?"script":name.endsWith(".ctd")?"text":"font-map");row.put("path",path);
   }else if(name.endsWith(".mzp")||name.endsWith(".cbg")){row.put("kind","image");row.put("path","images/"+name+".png");}
   else if(name.endsWith(".hw")){row.put("kind","audio");row.put("path","audio/"+name+".ogg");}
   else if(name.endsWith(".mp4")){row.put("kind","video");row.put("path","video/"+name);}
   else continue;
   assets.put(name,row);if((++done)%200==0&&progress!=null)progress.accept("Preparing game data: "+done);
  }
  Map<String,int[]> dimensions=new TreeMap<>();for(String name:entries.keySet())if(name.endsWith(".mzp")||name.endsWith(".cbg"))dimensions.put(name,HfaImageInfo.read(entries.get(name)));
  JSONObject recipes=new JSONObject(new String(read(new FileInputStream(new File(output,"meta/bitmap-recipes.json")),4*1024*1024),StandardCharsets.UTF_8));JSONArray rows=recipes.getJSONArray("records");StringBuilder geometry=new StringBuilder();int pairs=0;
  for(int i=0;i<rows.length();i++){JSONObject row=rows.getJSONObject(i);String name=row.getString("key"),primary=row.getString("primary");if(!dimensions.containsKey(name)||!dimensions.containsKey(primary))continue;if(!row.isNull("alias")){int[] old=dimensions.get(name),next=HfaImageInfo.read(entries.get(primary));geometry.append('"').append(name).append("\" ").append(old[0]).append(' ').append(old[1]).append(' ').append(next[0]).append(' ').append(next[1]).append('\n');pairs++;}dimensions.put(name,HfaImageInfo.read(entries.get(primary)));}
  String authority="abef1143c161fc1573f7c6478b9f0024cb32a766e72a5da087b54a3a8b9e0f2f";write(new File(output,"meta/native-composite-geometry.tsv"),("WOHCOMPOSITEGEOMETRY1 "+SteamMetadata.NATIVE_SHA+" "+authority+" "+pairs+"\n"+geometry).getBytes(StandardCharsets.UTF_8));
  StringBuilder sizes=new StringBuilder("WOH_IMAGE_DIMENSIONS1\n");for(Map.Entry<String,int[]> item:dimensions.entrySet())sizes.append(item.getKey()).append('\t').append(item.getValue()[0]).append('\t').append(item.getValue()[1]).append('\n');write(new File(output,"meta/image-dimensions.tsv"),sizes.toString().getBytes(StandardCharsets.UTF_8));
  StringBuilder loops=new StringBuilder("#name sampleCount sampleRate channels wrapperLoop loopStart loopEnd\n");for(Map.Entry<String,HfaIndex.Entry> item:entries.entrySet())if(item.getKey().endsWith(".hw")){byte[] h=item.getValue().prefix(64);if(HfaIndex.unsigned(h,0)!=64||HfaIndex.unsigned(h,8)!=item.getValue().length-64)throw new IOException("Invalid audio source header");loops.append(item.getKey());for(int offset:new int[]{12,16,20,24,28,32})loops.append('\t').append(HfaIndex.unsigned(h,offset));loops.append('\n');}write(new File(output,"meta/audio-loop-map.tsv"),loops.toString().getBytes(StandardCharsets.UTF_8));
  write(new File(output,"meta/game-edition.txt"),"WOHEDITION1\nvanilla\n".getBytes(StandardCharsets.UTF_8));JSONObject manifest=new JSONObject().put("schema_version",1).put("design_size",new JSONArray().put(1920).put(1080)).put("assets",assets);write(new File(output,"assets.json"),manifest.toString().getBytes(StandardCharsets.UTF_8));
  JSONObject ready=new JSONObject().put("schema",1).put("source",source.getPath()).put("buildId",profile.getJSONObject("steam").getInt("buildId"));
  if(patch!=null){prepareKorean(output,patch,entries,rows,manifest);ready.put("patch",patch.getCanonicalPath());}
  write(new File(output,MARKER),ready.toString().getBytes(StandardCharsets.UTF_8));if(progress!=null)progress.accept("Game data ready");
 }
 private static void prepareKorean(File output,File patch,LinkedHashMap<String,HfaIndex.Entry> original,JSONArray recipes,JSONObject manifest)throws Exception{
  LinkedHashMap<String,HfaIndex.Entry> overlay=KoreanPatch.members(patch,original.keySet()),effective=new LinkedHashMap<>(original);effective.putAll(overlay);
  JSONObject records=new JSONObject();Map<String,int[]> sizes=new TreeMap<>();long engineBytes=0;
  for(Map.Entry<String,HfaIndex.Entry> item:overlay.entrySet()){
   if(Thread.currentThread().isInterrupted())throw new InterruptedIOException("Patch import canceled");
   String name=item.getKey(),path=localPath(name);JSONObject row=new JSONObject();
   if(!path.isEmpty()){
    path=path.substring(0,path.indexOf('/')+1)+"ko/"+path.substring(path.indexOf('/')+1);byte[] encoded=item.getValue().read(64*1024*1024),decoded=encoded.length>=15&&encoded[0]=='L'&&encoded[1]=='e'&&encoded[2]=='n'?NativeAssetCodec.lenzu(encoded):encoded;engineBytes+=decoded.length;if(engineBytes>256L*1024*1024)throw new IOException("Patch metadata budget");write(new File(output,path),decoded);row.put("path",path);
   }else if(name.endsWith(".mzp")||name.endsWith(".cbg")){sizes.put(name,HfaImageInfo.read(item.getValue()));row.put("path","images/ko/"+name+".png");}
   else if(name.endsWith(".hw"))row.put("path","audio/ko/"+name+".ogg");
   else if(name.endsWith(".mp4"))row.put("path","video/ko/"+name);
   else throw new IOException("Unsupported Korean resource: "+name);
   records.put(name,row);
  }
  // Substitute both inputs before native primary/patch composition, including untranslated aliases.
  for(int i=0;i<recipes.length();i++){
   JSONObject recipe=recipes.getJSONObject(i);String key=recipe.getString("key"),primary=recipe.getString("primary"),alias=recipe.optString("alias","");
   if(overlay.containsKey(primary)||overlay.containsKey(alias)||overlay.containsKey(key)){
    HfaIndex.Entry base=effective.get(primary);if(base==null)throw new IOException("Korean primary image missing");sizes.put(key,HfaImageInfo.read(base));records.put(key,new JSONObject().put("path","images/ko/"+key+".png"));
   }
  }
  StringBuilder index=new StringBuilder("WOHLANGASSETS1\n"),dimensions=new StringBuilder("WOH_IMAGE_DIMENSIONS1\n");
  Set<String> names=new TreeSet<>();for(Iterator<String> it=records.keys();it.hasNext();)names.add(it.next());
  for(String name:names){int[] size=sizes.get(name);int w=size==null?0:size[0],h=size==null?0:size[1];index.append("ko\t").append(name).append('\t').append(records.getJSONObject(name).getString("path")).append('\t').append(w).append('\t').append(h).append('\n');if(size!=null)dimensions.append(name).append(' ').append(w).append(' ').append(h).append('\n');}
  write(new File(output,"meta/language-assets.tsv"),index.toString().getBytes(StandardCharsets.UTF_8));write(new File(output,"meta/ko-image-dimensions.tsv"),dimensions.toString().getBytes(StandardCharsets.UTF_8));
  File messages=new File(output,"meta/language-messages.tsv");String base=new String(Files.readAllBytes(messages.toPath()),StandardCharsets.UTF_8);byte[] dll=Files.readAllBytes(new File(patch,"mahoyo_kr.dll").toPath());write(messages,KoreanPatch.menuTable(dll,base).getBytes(StandardCharsets.UTF_8));
  write(new File(output,"meta/game-edition.txt"),"WOHEDITION1\nkorean\n".getBytes(StandardCharsets.UTF_8));manifest.put("overrides",new JSONObject().put("ko",records));write(new File(output,"assets.json"),manifest.toString().getBytes(StandardCharsets.UTF_8));
 }
}
