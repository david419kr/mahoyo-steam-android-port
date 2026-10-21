package org.mahoyo.port;

import android.content.Context;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONObject;

/** Shared five-language UI catalogue; no system-locale lookup in frame/row drawing. */
final class UiText {
    private static final Map<String,String[]> messages=new HashMap<>(),aliases=new HashMap<>();
    private static volatile boolean loaded;
    private static volatile String language="en";
    static synchronized boolean refresh(Context context){
        if(!loaded){
            try(InputStream input=context.getAssets().open("ui-text.json")){
                ByteArrayOutputStream output=new ByteArrayOutputStream();byte[] buffer=new byte[8192];int count;
                while((count=input.read(buffer))!=-1){if(output.size()>2*1024*1024-count)throw new IllegalStateException("UI catalogue budget");output.write(buffer,0,count);}
                byte[] bytes=output.toByteArray();
                JSONObject root=new JSONObject(new String(bytes,java.nio.charset.StandardCharsets.UTF_8));if(root.getInt("schema")!=1)throw new IllegalStateException("Unknown UI catalogue");
                JSONArray rows=root.getJSONArray("messages");
                for(int i=0;i<rows.length();i++){
                    JSONArray row=rows.getJSONArray(i);if(row.length()!=7)throw new IllegalStateException("Invalid UI catalogue row");
                    String id=row.getString(0);String[] values=new String[5];for(int n=0;n<5;n++)values[n]=row.getString(n+2);
                    if(messages.put(id,values)!=null)throw new IllegalStateException("Duplicate UI key: "+id);
                    JSONArray sources=row.getJSONArray(1);for(int n=0;n<sources.length();n++){String source=sources.getString(n);String[] previous=aliases.put(source,values);if(previous!=null)throw new IllegalStateException("Duplicate UI source: "+source);}
                }
                loaded=true;
            }catch(Exception error){messages.clear();aliases.clear();throw new IllegalStateException("Cannot read UI language catalogue",error);}
        }
        String next=GameLanguage.effective(context);boolean changed=!next.equals(language);language=next;return changed;
    }
    static String language(){return language;}
    static String text(String source){if(!loaded)throw new IllegalStateException("UI catalogue has not been loaded");String[] row=aliases.get(source);return row==null?source:row[column()];}
    private static int column(){switch(language){case "ja":return 1;case "ko":return 2;case "zh-Hans":return 3;case "zh-Hant":return 4;default:return 0;}}
    static String text(Context context,String source){if(!loaded)refresh(context);return text(source);}
    static String key(Context context,String id){if(!loaded)refresh(context);String[] row=messages.get(id);if(row==null)throw new IllegalArgumentException("Unknown UI text key: "+id);return row[column()];}
    static String format(Context context,String id,Object...values){String template=key(context,id);return String.format(Locale.forLanguageTag(language),template,values);}
    static void refreshTree(android.view.View view){
        if(view instanceof android.widget.TextView){android.widget.TextView label=(android.widget.TextView)view;label.setTextLocale(Locale.forLanguageTag(language));String source=label.getText().toString();String translated=knownTranslation(source);if(!source.equals(translated))label.setText(translated);}
        CharSequence description=view.getContentDescription();if(description!=null)view.setContentDescription(knownTranslation(description.toString()));
        if(view instanceof android.view.ViewGroup){android.view.ViewGroup group=(android.view.ViewGroup)view;for(int i=0;i<group.getChildCount();++i)refreshTree(group.getChildAt(i));}
    }
    private static String knownTranslation(String source){String[] row=aliases.get(source);if(row!=null)return row[column()];for(String[] values:messages.values())for(String value:values)if(value.equals(source))return values[column()];return source;}
    private UiText(){}
}
