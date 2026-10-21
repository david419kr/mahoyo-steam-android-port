package org.mahoyo.port;

/** A committed terminal Scene may request Title once, after a fresh engine identity check. */
final class MainHaltReturn {
    static final class Ticket {
        final Object scene;
        final long epoch,version,instructions,pc;
        final String module;
        Ticket(Object scene,long epoch,long version,String module,long instructions,long pc){
            this.scene=scene;this.epoch=epoch;this.version=version;this.module=module;this.instructions=instructions;this.pc=pc;
        }
        boolean matches(long version,String module,long instructions,long pc){
            return this.version==version&&this.module.equals(module)&&this.instructions==instructions&&this.pc==pc;
        }
    }
    private Object current;
    private long epoch;
    private Ticket pending;
    private boolean sent;
    synchronized void observe(Object scene){
        if(current==scene)return;
        current=scene;epoch++;pending=null;sent=false;
    }
    synchronized void invalidate(){epoch++;pending=null;}
    synchronized Ticket claim(Object scene,boolean ready,long version,String module,long instructions,long pc){
        if(!ready||scene==null||scene!=current||pending!=null||sent||version<0||module==null||module.isEmpty()||instructions<0||pc<0)return null;
        pending=new Ticket(scene,epoch,version,module,instructions,pc);return pending;
    }
    synchronized boolean consume(Ticket ticket,boolean terminal,long version,String module,long instructions,long pc){
        if(ticket==null||ticket!=pending||current!=ticket.scene||ticket.epoch!=epoch)return false;
        pending=null;
        if(!terminal||!ticket.matches(version,module,instructions,pc))return false;
        sent=true;return true;
    }
}
