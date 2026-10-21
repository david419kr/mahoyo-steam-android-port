package org.mahoyo.port;

//Port policy: acknowledge represented transient UI closure plus an actual
//matching GL commit; this does not identify Native graphics slot43.
final class InputRegionSession {
    private String generation="0",request="0",trigger="0",epoch="0",release="0",sent="";
    private boolean active,enabled,pending,held;
    private long version=-1;
    boolean update(long version,boolean active,boolean enabled,boolean pending,String gen,String request,String trigger,String epoch,String release){
        if(version<this.version)return false;
        if(!this.generation.equals(gen)||!this.trigger.equals(trigger)||!this.epoch.equals(epoch))held=false;
        this.version=version;this.active=active;this.enabled=enabled;this.pending=pending;this.generation=gen;this.request=request;this.trigger=trigger;this.epoch=epoch;this.release=release;return true;
    }
    boolean restricted(){return active;}
    boolean captures(){return active||enabled||pending;}
    boolean needsClose(){return pending;}
    String acknowledge(long renderedVersion,boolean actualCurrentScene,boolean uiClosed,boolean playerReleased,int width,int height){
        if(!actualCurrentScene||renderedVersion!=version||!uiClosed||width<=0||height<=0)return "";
        String type,id;
        if(!release.equals("0")){if(!playerReleased)return "";type="inputJumpReleased";id=release;}
        else if(pending){type="inputUiReady";id=request;}
        else return "";
        String key=type+" "+generation+" "+id+" "+version;
        if(key.equals(sent))return "";sent=key;return key+" "+width+" "+height;
    }
    String pointer(boolean down,boolean inside){
        if(!enabled||pending||trigger.equals("0"))return "";
        if(down){if(held)return "";held=true;}else{if(!held)return "";held=false;}
        return generation+" "+trigger+" "+epoch+" "+(down?1:0)+" "+(inside?1:0);
    }
    String cancel(){held=false;return enabled&&!trigger.equals("0")?generation+" "+trigger+" "+epoch:"";}
}
