#include "screen_wave.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using mahoyo::screen_wave::Controller;
static int checks;
static void check(bool value,const char* message){checks++;if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F f){try{f();}catch(const std::invalid_argument&){checks++;return;}throw std::runtime_error("Expected an explicit Screen Raster rejection");}
int main(){try{
    Controller c;c.request(9,100);check(c.pending()&&!c.active()&&!c.safe(),"request stays pending until GPU-ready commit");
    c.sample(900000);c.commit(900000);auto first=c.draw(900000);check(first&&first->id==1&&first->phaseMs==0&&first->strength==0,"loading time cannot consume strength or first-draw phase");
    c.sample(901000);auto coarse=c.draw(901000);check(coarse->strength==.5f&&coarse->phaseMs==1000,"Native job elapsed cap50 is independent from the full draw-clock delta");
    c.sample(901016);check(std::abs(c.draw(901016)->strength-.66f)<1e-6,"native50cap progress resumes with a normal16ms frame");
    c.sample(902000,false);auto held=c.draw(902000,false);check(held->phaseMs==1016&&std::abs(held->strength-.66f)<1e-6,"pause rebases both anchors without changing values");
    c.sample(902016);auto resumed=c.draw(902016);check(resumed->phaseMs==1032&&std::abs(resumed->strength-.82f)<1e-6,"resume excludes paused time");
    c.sample(902034);check(c.safe()&&c.remainingWait()==0&&c.draw(902034)->strength==1,"persistent normal wave is saveable and skipped by wait64 after its fade");
    auto saved=c.record();Controller restored;restored.restore(saved,c.generation(),9000000);check(restored.draw(9000000)->phaseMs==1050,"fresh restore preserves phase rather than consuming platform time");
    restored.sample(9000016);check(restored.draw(9000016)->phaseMs==1066,"first real frame advances restored phase normally");
    c.request(0,100);c.commit(902034);check(!c.safe()&&c.remainingWait()==100&&c.draw(902034)->strength==1,"stop starts a separate strength1-to0 timeline");
    c.sample(902084);check(c.draw(902084)->strength==.5f&&c.remainingWait()==50,"stop fade reaches half in50ms");
    c.request(0,200);c.commit(902084);check(c.draw(902084)->strength==.75f&&c.remainingWait()==150,"repeated stop changes duration but preserves its cancel elapsed");
    for(int i=1;i<=3;++i)c.sample(902084+i*50);check(!c.active()&&c.safe()&&c.generation()==1,"stop completion deletes the global object and releases idle polling");
    Controller empty;empty.restore(c.record(),c.generation(),10000000);empty.request(9,0);empty.commit(10000000);check(empty.draw(10000000)->id==2,"inactive save preserves the next portable renderer identity");
    auto originalId=empty.draw(10000000)->id;empty.request(9,0);check(empty.draw(10000000)->id==originalId,"replacement is not visible before commit");empty.commit(10000000);check(empty.draw(10000000)->id==3,"positive replacement starts a new object/phase after commit");
    auto malformed=empty.record();malformed->preset=8;rejects([&]{empty.restore(malformed,3,10000000);});check(empty.draw(10000000)->id==3,"malformed restore preserves the live object");
    malformed=empty.record();malformed->phaseMs=1;malformed->drawStarted=false;rejects([&]{empty.restore(malformed,3,10000000);});
    rejects([&]{empty.restore(empty.record(),4,10000000);});rejects([&]{empty.restore(std::nullopt,std::numeric_limits<uint64_t>::max(),0);});
    for(int preset:{2,8,10,51})rejects([&]{empty.request(preset,0);});rejects([&]{empty.request(9,-1);});
    Controller wrap;auto record=empty.record();record->id=1;record->drawStarted=true;record->phaseMs=UINT32_MAX-5;wrap.restore(record,1,100);check(wrap.draw(116)->phaseMs==10,"draw phase preserves the native uint32 wrap");
    wrap.reset();check(!wrap.active()&&!wrap.pending()&&wrap.generation()==0,"new owner resets all Screen Raster state");

    Controller one;one.request(1,100);one.sample(500000);check(one.pending()&&!one.active(),"preset1 loading stays pending");one.commit(500000);
    check(one.record()->preset==1&&one.draw(500000)->preset==1&&one.draw(500000)->strength==0,"preset1 preserved in packet and JSON sample");
    one.sample(501000);check(one.draw(501000)->strength==.5f&&one.draw(501000)->phaseMs==1000,"preset1 cap and draw clock stay independent");
    Controller reload;reload.restore(one.record(),one.generation(),100);check(reload.record()->preset==1&&reload.draw(100)->phaseMs==1000&&reload.draw(100)->strength==.5f,"fresh preset1 record roundtrip");
    reload.sample(1100,false);check(reload.draw(1100,false)->phaseMs==1000&&reload.draw(1100,false)->strength==.5f,"preset1 pause preserves both clocks");
    reload.sample(1116);check(reload.draw(1116)->phaseMs==1016&&std::abs(reload.draw(1116)->strength-.66f)<1e-6,"preset1 resume rebases both clocks");
    auto before=reload.record();reload.request(9,0);check(reload.record()->preset==1&&reload.record()->id==before->id,"1-to9 pending keeps old visual record");reload.commit(1116);
    check(reload.record()->preset==9&&reload.draw(1116)->preset==9&&reload.draw(1116)->phaseMs==0&&reload.generation()==2,"1-to9 starts new preset identity and phase");
    reload.request(1,0);reload.commit(1116);check(reload.record()->preset==1&&reload.draw(1116)->strength==1&&reload.generation()==3,"9-to1 keeps requested preset");
    for(int bad:{0,2,8,10,51,52}){auto invalid=reload.record();invalid->preset=bad;rejects([&]{reload.restore(invalid,3,1116);});check(reload.record()->preset==1&&reload.generation()==3,"invalid preset restore is atomic");}
    reload.request(1,0);rejects([&]{reload.request(2,0);});check(reload.pending()&&reload.record()->preset==1,"invalid request preserves pending valid request");reload.commit(1116);check(reload.generation()==4&&reload.draw(1116)->preset==1,"valid pending request survives rejection");
    reload.request(0,100);reload.commit(1116);check(reload.remainingWait()==100&&reload.draw(1116)->strength==1,"preset1 stop uses existing cancel path");reload.sample(1166);check(reload.draw(1166)->strength==.5f,"preset1 stop halfway");reload.request(0,200);reload.commit(1166);check(reload.draw(1166)->strength==.75f&&reload.remainingWait()==150,"preset1 repeated stop does not restart elapsed");
    for(int i=1;i<=3;++i)reload.sample(1166+i*50);check(!reload.active()&&reload.safe()&&reload.generation()==4,"preset1 stop completes and releases polling");
    Controller immediate;immediate.request(1,0);immediate.commit(0);check(immediate.safe()&&immediate.draw(0)->strength==1,"actual [-1,1,0] starts steady at first commit");immediate.request(0,0);immediate.commit(0);check(!immediate.active()&&immediate.safe(),"actual [-1,0,0] immediately removes screen raster");
    Controller oneWrap;auto wrapRecord=one.record();wrapRecord->drawStarted=true;wrapRecord->phaseMs=UINT32_MAX-5;oneWrap.restore(wrapRecord,1,100);check(oneWrap.draw(116)->phaseMs==10&&oneWrap.draw(116)->preset==1,"preset1 signed uint32 phase preserved");
    std::cout<<"Screen Raster1/9 checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
