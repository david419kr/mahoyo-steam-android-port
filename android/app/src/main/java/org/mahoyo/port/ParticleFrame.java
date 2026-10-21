package org.mahoyo.port;

import org.json.*;
import java.util.*;

/** Core-resolved Native quads; no Java simulation, pivot, rotation or RNG. */
final class ParticleFrame {
    static final int FALL_NOTE_PRIORITY=22576,FOCUS_LINE_PRIORITY=22577;
    static final ParticleFrame EMPTY=new ParticleFrame(Collections.emptyList(),Collections.emptySet());
    final List<Sprite> sprites;final Set<String> resources;final String fallPending,focusPending;
    ParticleFrame(List<Sprite> values,Set<String> required){this(values,required,"0","0");}
    ParticleFrame(List<Sprite> values,Set<String> required,String fallPending,String focusPending){
        for(String token:new String[]{fallPending,focusPending})if(token==null||!token.matches("[0-9]{1,20}"))throw new IllegalArgumentException("Invalid particle pending revision");
        this.fallPending=fallPending;this.focusPending=focusPending;
        ArrayList<Sprite> sorted=new ArrayList<>(values);sorted.sort(Comparator.comparingInt(v->v.priority));
        sprites=Collections.unmodifiableList(sorted);resources=Collections.unmodifiableSet(new LinkedHashSet<>(required));
        for(Sprite sprite:sprites)if(!resources.contains(sprite.resource))throw new IllegalArgumentException("Particle resource not declared");
    }
    static ParticleFrame from(JSONObject value){
        if(value==null)return EMPTY;
        ArrayList<Sprite> sprites=new ArrayList<>();Set<String> resources=new LinkedHashSet<>();
        String fall=family(value.optJSONObject("fallNote"),FALL_NOTE_PRIORITY,sprites,resources);
        String focus=family(value.optJSONObject("focusLine"),FOCUS_LINE_PRIORITY,sprites,resources);
        return new ParticleFrame(sprites,resources,fall,focus);
    }
    String readyAck(long version){return version<0||fallPending.equals("0")&&focusPending.equals("0")?"":version+" "+fallPending+" "+focusPending;}
    private static String family(JSONObject value,int priority,List<Sprite> sprites,Set<String> resources){
        if(value==null)return "0";
        boolean active=value.optBoolean("active",false),pending=value.optBoolean("pendingResource",false);
        if(!active&&!pending)return "0";
        String revision=value.optString("revision","");if(!revision.matches("[0-9]{1,20}")||pending&&revision.equals("0"))throw new IllegalArgumentException("Invalid particle revision");
        JSONArray names=value.optJSONArray("resources");if(names==null||names.length()==0)throw new IllegalArgumentException("Missing particle resources");
        Set<String> familyResources=new LinkedHashSet<>();for(int i=0;i<names.length();i++){String name=names.optString(i,"");if(name.isEmpty())throw new IllegalArgumentException("Empty particle resource");familyResources.add(name);}
        resources.addAll(familyResources);JSONArray values=value.optJSONArray("sprites");if(values==null)return pending?revision:"0";
        for(int i=0;i<values.length();i++){
            JSONObject item=values.optJSONObject(i);if(item==null)throw new IllegalArgumentException("Invalid particle sprite");
            String resource=item.optString("resource","");if(!familyResources.contains(resource))throw new IllegalArgumentException("Undeclared family particle resource");
            sprites.add(new Sprite(resource,array(item.optJSONArray("sourceRect"),4),array(item.optJSONArray("quad"),8),integer(item,"opacity8"),integer(item,"priority"),priority));
        }return pending?revision:"0";
    }
    private static int integer(JSONObject value,String key){double n=value.optDouble(key,Double.NaN);if(!Double.isFinite(n)||n!=Math.floor(n)||n<Integer.MIN_VALUE||n>Integer.MAX_VALUE)throw new IllegalArgumentException("Invalid particle "+key);return (int)n;}
    private static float[] array(JSONArray value,int count){if(value==null||value.length()!=count)throw new IllegalArgumentException("Invalid particle geometry array");float[] result=new float[count];for(int i=0;i<count;i++){result[i]=(float)value.optDouble(i,Double.NaN);if(!Float.isFinite(result[i]))throw new IllegalArgumentException("Nonfinite particle coordinate");}return result;}
    static final class Sprite {
        final String resource;final float[] source,quad;final int opacity8,priority;
        Sprite(String resource,float[] source,float[] quad,int opacity8,int priority,int expectedPriority){
            if(resource==null||resource.isEmpty()||source==null||source.length!=4||quad==null||quad.length!=8||opacity8<0||opacity8>255||priority!=expectedPriority||(priority!=FALL_NOTE_PRIORITY&&priority!=FOCUS_LINE_PRIORITY))throw new IllegalArgumentException("Invalid particle sprite");
            for(float n:source)if(!Float.isFinite(n))throw new IllegalArgumentException("Nonfinite particle source");
            for(float n:quad)if(!Float.isFinite(n))throw new IllegalArgumentException("Nonfinite particle quad");
            if(source[0]<0||source[1]<0||source[2]<=0||source[3]<=0)throw new IllegalArgumentException("Invalid particle source extent");
            this.resource=resource;this.source=source.clone();this.quad=quad.clone();this.opacity8=opacity8;this.priority=priority;
        }
        float alpha(){return opacity8/255f;}
        boolean inPriorityRange(int minimum,int maximum){return priority>=minimum&&priority<=maximum;}
    }
}
