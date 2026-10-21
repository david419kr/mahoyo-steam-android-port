#include "teatime.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace mahoyo::teatime;
static int checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F> static void invalid(F call,const char* message){bool threw=false;try{call();}catch(const std::invalid_argument&){threw=true;}check(threw,message);}
static Controller ready(uint64_t id=1,int selector=1,uint64_t start=0){Controller m;m.enter(id,selector,start);m.tick(start+1000);return m;}
static void validationAndThemes(){
    Controller m;
    check(!m.active()&&!m.state()&&!m.needsTick(),"initial state has no scheduled work");
    check(m.tick(1000)==Result::None&&m.alphaFixed16(1000)==0,"inactive clock is inert");
    check(m.action(1,1,"next",1000,true)==Result::None,"inactive input cannot resume the VM");
    invalid([&]{m.enter(0,1,0);},"zero modal ID is invalid");
    invalid([&]{m.enter(1,0,0);},"selector0 is invalid");
    invalid([&]{m.restoreReady(1,14);},"selector14 cannot enter through load");
    invalid([&]{m.restoreReady(0,1);},"restored modal also requires a nonzero ID");
    invalid([&]{m.buttonAlphaFixed16(-1,0);},"negative native event is invalid");
    invalid([&]{m.buttonAlphaFixed16(4,0);},"native event4 is invalid");
    const bool black[]={true,false,true,false,false,true,true,true,true,false,false,false,true};
    for(int selector=1;selector<=13;++selector)check(Controller::isBlack(selector)==black[selector-1],"original selector theme table");
    invalid([]{Controller::isBlack(0);},"invalid theme selector is explicit");
    m.enter(5,8,70);invalid([&]{m.enter(0,1,0);},"invalid replacement is rejected");
    check(m.state()->id==5&&m.state()->selector==8&&m.state()->startedAt==70,"failed enter preserves the current modal");
}
static void openingStaggerAndInputGate(){
    Controller m;m.enter(1,1,100);
    check(m.active()&&m.state()->phase==Phase::Opening&&m.state()->revision==1&&m.needsTick(),"enter starts Opening revision1");
    check(m.alphaFixed16(100)==65536,"opening container is full alpha; only its parts stagger");
    const uint64_t offsets[]={0,1,199,200,399,400};
    const uint32_t expected[]={0,163,32604,32768,65372,65536};
    for(int event=0;event<4;++event){
        const uint64_t start=100+uint64_t(event)*200;
        check(m.buttonAlphaFixed16(event,start?start-1:0)==0,"part cannot appear before its own offset");
        for(size_t i=0;i<6;++i)check(m.buttonAlphaFixed16(event,start+offsets[i])==expected[i],"native400ms fixed16 samples");
    }
    check(m.tick(1099)==Result::None&&m.state()->phase==Phase::Opening,"input stays blocked one millisecond before1000ms");
    check(m.action(1,1,"next",1100,true)==Result::None&&m.state()->phase==Phase::Opening,"input cannot implicitly advance the opening clock");
    check(m.tick(1100)==Result::None&&m.state()->phase==Phase::Ready&&m.state()->revision==2,"only tick releases the1000ms gate and bumps revision");
    check(!m.needsTick()&&m.alphaFixed16(1100)==65536,"Ready has no idle polling deadline");
    for(int event=0;event<4;++event)check(m.buttonAlphaFixed16(event,1100)==65536,"all four native parts are fully visible when Ready");
    const auto state=*m.state();m.tick(std::numeric_limits<uint64_t>::max());
    check(m.state()->revision==state.revision&&m.state()->startedAt==state.startedAt,"idle ticks never mutate Ready state");
}
static void savingAndStaleActions(){
    auto m=ready();const auto revision=m.state()->revision;
    check(m.action(2,revision,"save",1000,true)==Result::None,"stale modal ID is ignored");
    check(m.action(1,revision-1,"save",1000,true)==Result::None,"opening revision cannot target Ready");
    check(m.action(1,revision,"archive",1000,true)==Result::None&&m.state()->revision==revision,"unsupported action never consumes the modal");
    check(m.action(1,revision,"save",1000,false)==Result::None&&m.state()->phase==Phase::Ready,"unavailable Save does not change state");
    check(m.action(1,revision,"save-return",1000,false)==Result::None&&m.state()->revision==revision,"save-return only applies to Saving");
    check(m.action(1,revision,"save",1010,true)==Result::Save,"valid Save emits one host request");
    check(m.state()->phase==Phase::Saving&&m.state()->revision==revision+1&&!m.needsTick(),"Saving is immediately hidden and unscheduled");
    check(m.alphaFixed16(1010)==0&&m.buttonAlphaFixed16(1,1010)==0,"Save hides immediately without a close fade");
    check(m.action(1,revision,"save",1010,true)==Result::None,"duplicate Save revision is stale");
    check(m.action(1,revision+1,"title",2000,true)==Result::None&&m.active(),"Saving ignores Title from a stale UI surface");
    check(m.tick(100000)==Result::None&&m.state()->phase==Phase::Saving,"Saving never completes by elapsed time");
    check(m.action(1,revision,"save-return",100000,false)==Result::None&&m.state()->phase==Phase::Saving,"stale Save return is ignored");
    check(m.action(1,revision+1,"save-return",100000,false)==Result::None,"current Save return needs no new save eligibility");
    check(m.state()->phase==Phase::Ready&&m.state()->revision==revision+2&&!m.needsTick(),"Save returns directly to Ready and bumps revision");
    check(m.alphaFixed16(100000)==65536&&m.buttonAlphaFixed16(1,100000)==65536,"Save return does not replay opening or500ms fade");
    m.action(1,revision+1,"save-return",100000,false);
    check(m.state()->revision==revision+2,"duplicate Save return cannot alter the restored revision");
}
static void closingAndTitle(){
    auto m=ready();const auto revision=m.state()->revision;
    check(m.action(1,revision,"next",1200,true)==Result::None,"Next schedules completion rather than immediately resuming VM");
    check(m.state()->phase==Phase::Closing&&m.state()->revision==revision+1&&m.needsTick(),"Next starts Closing once");
    const uint64_t times[]={1200,1201,1450,1699,1700};
    const uint32_t expected[]={65536,65405,32768,132,0};
    for(size_t i=0;i<5;++i)check(m.alphaFixed16(times[i])==expected[i],"native close subtracts truncated fixed16 progress");
    check(m.buttonAlphaFixed16(1,1450)==65536,"individual button does not apply the parent close fade twice");
    check(m.action(1,revision+1,"title",1400,true)==Result::None,"Closing cannot switch its result through later input");
    check(m.action(1,revision+1,"next",1500,true)==Result::None&&m.state()->startedAt==1200,"repeat Next cannot restart Closing");
    check(m.tick(1699)==Result::None&&m.active(),"Next cannot resume before500ms");
    check(m.tick(1700)==Result::ResumeCurrentVm&&!m.active()&&!m.needsTick(),"Next resumes current VM exactly at500ms");
    check(m.tick(2000)==Result::None&&m.action(1,revision+1,"next",2000,true)==Result::None,"completion is emitted exactly once");
    auto title=ready(2);
    check(title.action(2,title.state()->revision,"title",2000,true)==Result::Title,"Title requests the host's existing title flow");
    check(!title.active()&&!title.needsTick()&&title.alphaFixed16(2000)==0,"Title closes immediately without invented timing");
    check(title.tick(100000)==Result::None,"Title can never produce a delayed ResumeCurrentVm");
}
static void resetRestoreAndLogicalClock(){
    Controller m;m.enter(1,1,5000);
    check(m.tick(5200)==Result::None&&m.buttonAlphaFixed16(0,5200)==32768,"logical200ms opening progress");
    const auto revision=m.state()->revision;
    for(int i=0;i<3;++i)check(m.tick(5200)==Result::None&&m.buttonAlphaFixed16(0,5200)==32768&&m.state()->revision==revision,"paused logical time cannot advance animation");
    check(m.buttonAlphaFixed16(0,4999)==0,"pre-epoch snapshots saturate rather than unsigned-underflow");
    m.reset();check(!m.active()&&m.tick(100000)==Result::None,"reset cancels Opening without VM resume");
    m.restoreReady(2,13);check(m.state()->id==2&&m.state()->selector==13&&m.state()->revision==1&&m.state()->phase==Phase::Ready,"load supplies a fresh ID and stable Ready state");
    check(!m.needsTick()&&m.alphaFixed16(0)==65536&&m.buttonAlphaFixed16(3,0)==65536,"restored Ready has no opening delay");
    check(m.action(1,1,"next",100000,true)==Result::None&&m.state()->phase==Phase::Ready,"pre-load identity cannot act on restored state");
    m.action(2,1,"save",100000,true);m.reset();
    check(m.action(2,2,"save-return",100000,true)==Result::None&&!m.active(),"Save callback after reset cannot resurrect modal");
    m.restoreReady(3,1);m.action(3,1,"next",500,true);m.reset();
    check(m.tick(1000)==Result::None&&!m.active(),"reset cancels Closing without a delayed resume");
    m.enter(4,2,0);m.enter(5,3,10);
    check(m.action(4,1,"next",1010,true)==Result::None&&m.state()->id==5,"new modal identity supersedes previous opening input");
    const auto start=std::numeric_limits<uint64_t>::max()-3000;
    m.enter(6,1,start);m.tick(start+1000);m.action(6,m.state()->revision,"next",start+1500,true);
    check(m.alphaFixed16(start+1999)==132&&m.tick(start+2000)==Result::ResumeCurrentVm,"large logical epoch does not overflow timing arithmetic");
}
int main(){try{validationAndThemes();openingStaggerAndInputGate();savingAndStaleActions();closingAndTitle();resetRestoreAndLogicalClock();std::cout<<"TEATIME: "<<checks<<" state/timing checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
