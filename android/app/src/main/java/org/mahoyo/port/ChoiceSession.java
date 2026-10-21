package org.mahoyo.port;

/** Commit barrier and once-only input latch; resizing/backgrounding never chooses. */
final class ChoiceSession {
    static final class Request {final ChoiceModel model;final int ordinal;Request(ChoiceModel model,int ordinal){this.model=model;this.ordinal=ordinal;}String payload(){return model.payload(ordinal);}}
    private ChoiceModel current,shown;private Request pending;private boolean blocked;
    boolean state(ChoiceModel value){if(current==null?value==null:value!=null&&current.revision.equals(value.revision))return false;current=value;pending=null;return true;}
    boolean presented(String revision){String expected=current==null?"":current.revision;if(!expected.equals(revision))return false;if(shown==current)return false;shown=current;return true;}
    boolean active(){return current!=null||shown!=null;}
    boolean waiting(){return current!=null;}
    ChoiceModel shown(){return shown;}
    boolean blocked(boolean value){if(blocked==value)return false;blocked=value;return true;}
    boolean enabled(){return !blocked&&current!=null&&shown==current&&pending==null;}
    Request begin(int ordinal){if(!enabled()||ordinal<0||ordinal>=current.items.size()||!current.items.get(ordinal).enabled)return null;return pending=new Request(current,ordinal);}
    boolean complete(Request value){if(pending!=value||current!=value.model)return false;pending=null;return true;}
    void close(){current=shown=null;pending=null;blocked=true;}
}
