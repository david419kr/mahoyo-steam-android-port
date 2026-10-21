//Synthetic Native112/110 timeline and portable WH18 regressions.
#include "../runtime/wait_clock.h"
#include "effect_bases_save_cases.h"
static void wait110(Program& p,int duration,int third=1){p.push(third);p.push(1);p.push(duration);p.op(0x110);}
static std::string baselineFooter(const mahoyo::save::Document& document){std::string extra(document.hostExtra.begin(),document.hostExtra.end());auto at=extra.find("WAITBASELINE1 ");check(at!=std::string::npos,"WH18 contains wait baseline footer");return extra.substr(at,extra.find('\n',at)-at+1);}
static mahoyo::save::Document withBaseline(mahoyo::save::Document document,const std::string& footer){std::string extra(document.hostExtra.begin(),document.hostExtra.end());const auto bases=extra.substr(extra.find("EFFECTBASES1 "));extra.resize(extra.find("WAITBASELINE1 "));extra+=footer+bases;document.hostExtra.assign(extra.begin(),extra.end());return document;}
static void waitClockMathAndNativeBoundaries(){
    stoppedEffectBaseSaveAtomicAndLegacy();
    using mahoyo::wait_clock::Clock;
    Clock c;check(c.age(0)==0&&!c.pending(),"fresh manager starts with zero age");
    c.request();c.request();check(c.pending()&&c.age(100)==100,"112 requests are idempotent and do not move time");
    const auto snapshot=c.age(100);c.prepared(snapshot,125);check(!c.pending()&&c.age(125)==25,"baseline uses cached prepare time rather than late completion");
    c.prepared(c.age(130),130);check(c.age(130)==30,"normal preparation without112 does not reset baseline");
    for(int flag:{0,2,-1,INT32_MIN,INT32_MAX})check(Clock::remaining(400,flag,399)==400,"only exactly1 applies native elapsed compensation");
    check(Clock::remaining(400,1,399)==1&&Clock::remaining(400,1,400)==0&&Clock::remaining(400,1,401)==0&&Clock::remaining(INT32_MIN,1,0)==0,"110 clamps negative duration and saturates remaining at zero");
    check(Clock::remaining(INT32_MAX,1,0)==uint64_t(INT32_MAX)&&Clock::remaining(1,1,UINT64_MAX)==0,"unsigned native duration comparison has no signed wrap");
    c.restore(Clock::MaxAge,false,77);check(c.age(77)==Clock::MaxAge,"maximum portable age is exact");
    for(uint64_t now:{uint64_t(76),uint64_t(78)}){bool rejected=false;try{c.age(now);}catch(const std::exception&){rejected=true;}check(rejected,"clock reversal and overflow fail explicitly");}
    c.restore(37,true,1000);std::ostringstream out;c.write(out,1007);std::istringstream in(out.str());auto copy=Clock::read(in,9000000);check(copy.pending()&&copy.age(9000000)==44,"portable age never imports an OS timestamp");
    for(const char* row:{"WAITBASELINE1 2 0","WAITBASELINE1 -1 0","WAITBASELINE1 0 -1","WAITBASELINE1 0 +1","WAITBASELINE1 0 1.0","WAITBASELINE1 0 9223372036854775808","WAITBASELINE1 0 18446744073709551616","WAITBASELINE1 0 xyz"}){bool rejected=false;try{std::istringstream bad(row);Clock::read(bad,0);}catch(const std::exception&){rejected=true;}check(rejected,"malformed baseline values cannot coerce to a valid clock");}
}
static void native112CutAndCompensated110(){
    Program p;p.image(1000,"BLACK");wait110(p,600,0);p.push(2468);p.op(0x112);p.op(0x112);p.push(0);p.op(0x251);p.commit();wait110(p,400,1);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"frame"),"preceding600ms relative wait starts normally");
    f.command("tick","1000");state=f.command("tick","1584");check(waiting(state,"transitionReady")&&number(layerObject(state,0),"z")==2468,"112 consumes and pushes no words, and does not yield before the actual cut");
    const auto instructions=number(state,"instructions");state=f.command("tick","999999");check(waiting(state,"transitionReady")&&number(state,"instructions")==instructions,"GPU resource preparation cannot consume timeline wait");
    state=acknowledge(f,state);check(waiting(state,"frame"),"cut first retains its native port16ms frame");
    state=f.command("tick","1000000");check(waiting(state,"frame"),"110 flag1 subtracts the16ms since cut baseline");
    state=f.command("tick","1000383");check(waiting(state,"frame"),"compensated wait remains blocked at399ms baseline age");
    state=f.command("tick","1000384");check(waiting(state,"input"),"actual112/cut/110 sequence resumes at400ms, not416ms");
    f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");check(baselineFooter(saved)=="WAITBASELINE1 0 400\n","post-cut manager age persists independently of previous600ms");f.passed=true;
}
static void native110ThirdFlagsAndPreparePause(){
    for(int mode:{0,1,2,-1}){
        Program p;p.op(0x112);p.commit();wait110(p,400,mode);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));state=f.command("tick","1000");check(waiting(state,"frame"),"110 starts after the cut");
        state=f.command("tick",mode==1?"1383":"1399");check(waiting(state,"frame"),"flag-specific wait holds until its exact deadline");
        state=f.command("tick",mode==1?"1384":"1400");check(waiting(state,"input"),"third0/2/-1 remain relative while1 uses baseline");f.passed=true;
    }
    Program p;p.op(0x112);wait110(p,400);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"110 itself prepares a pending112 reset without a200");
    state=f.command("transitionReady","99999");check(waiting(state,"transitionReady"),"stale prepare acknowledgement cannot consume reset");state=acknowledge(f,state);
    f.command("tick","1000");f.command("pause");state=f.command("tick","999999");check(waiting(state,"frame")&&number(state,"nextTickMs")==0,"pause cannot advance baseline or wait");f.command("resume");f.command("tick","1000000");state=f.command("tick","1000367");check(waiting(state,"frame"),"resume excludes paused wall time and holds at399ms");state=f.command("tick","1000368");check(waiting(state,"input"),"resumed400ms timeline completes once");f.passed=true;
    for(int duration:{INT32_MIN,-1,0}){Program q;wait110(q,duration);q.dialogue(0);q.op(0xf4);Fixture g(std::move(q));g.command("textSpeed","0");check(waiting(g.command("action","start"),"input"),"nonpositive duration without pending preparation continues immediately");g.passed=true;}
}
static void native112SaveRestoreAndMalformedAtomic(){
    //141 clears glyphs, not the manager baseline. A112 before a glyph-only
    //frame can therefore remain pending at the still-visible input checkpoint.
    Program p;p.dialogue(0);p.op(0x112);p.op(0x141);p.dialogue(1);wait110(p,400);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");check(currentHostSchema(good),"new saves use explicit WH18 schema");
    //A saved pending request is native-owned state, not a requirement for a
    //new opcode to be executed before the next prepare.
    good=withBaseline(good,"WAITBASELINE1 1 123\n");mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos,"fresh Runtime accepts pending reset and relative age");
    f.command("save","2");check(baselineFooter(mahoyo::save::read(f.root/"savedata/slot2.whs"))=="WAITBASELINE1 1 123\n","fresh restore and immediate save preserve pending without consuming it");
    const auto visible=visibleLayers(state),beforeText=state.substr(state.find("\"text\":"),state.find(",\"actions\":")-state.find("\"text\":"));const auto instructions=number(state,"instructions");
    for(const std::string& footer:{std::string(""),std::string("WAITBASELINE1 2 0\n"),std::string("WAITBASELINE1 0 -1\n"),std::string("WAITBASELINE1 0 9223372036854775808\n"),std::string("WAITBASELINE1 0 0\ntrailing\n")}){
        mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",withBaseline(good,footer));state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&number(state,"instructions")==instructions&&visibleLayers(state)==visible&&state.find(beforeText)!=std::string::npos,"invalid WH18 footer is rejected before VM/text/layers mutation");
    }
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",host17Fixture(good));state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos,"genuine WH17 retains history and loads without baseline extension");f.command("save","2");check(baselineFooter(mahoyo::save::read(f.root/"savedata/slot2.whs"))=="WAITBASELINE1 0 0\n","WH17 migration uses zero/false, never guessed timestamps");
    f.passed=true;
}
