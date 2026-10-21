#include "window_fade.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mahoyo::effects;

static void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}

static void nativeCurves() {
    WindowFade window;window.requestShow(300);window.commit(1000,true);
    // Original complete0913a0 samples (65536 delta), independently emulated by
    // tools/host_reverse_window_fade.py, then native092790 integer truncation.
    const int32_t times[]={0,75,149,150,151,225,299,300};
    const int32_t expected[]={0,0,0,0,436,32768,65099,65536};
    for(unsigned i=0;i<8;++i)check(window.sample(1000+times[i])==expected[i],"native show curve sample");
    check(window.visible()&&!window.active(),"show endpoint is visible and inactive");
    window.requestHide(300);window.commit(2000,true);
    const int32_t hidden[]={65536,32768,437,0,0,0,0,0};
    for(unsigned i=0;i<8;++i)check(window.sample(2000+times[i])==hidden[i],"native hide curve sample");
    check(!window.visible()&&!window.active(),"hide endpoint is hidden and inactive");
}

static void pendingAndReadiness() {
    WindowFade window;window.requestShow(300);
    check(!window.visible()&&!window.active()&&window.pending(),"request is pending without visual changes");
    check(!window.commit(1000,false)&&window.pending(),"not-ready commit retains pending request");
    check(window.commit(5000,true)&&window.sample(5150)==0,"clock begins at ready commit rather than host call");
    check(window.visible()&&window.active(),"logical visible updates while alpha is still zero");
    window.requestHide(300);
    check(window.pending()->fromFixed16==65536,"positive hide starts from full alpha even during show");
    check(!window.commit(5200,false)&&window.sample(5225)==32768,"old active animation continues while new commit waits");
    window.commit(5300,true);
    check(!window.visible()&&window.currentFixed16()==65536,"ready hide replaces active show with explicit start");
    check(window.sample(5375)==32768,"hide runs independently from native visibility flag");
}

static void requestBranches() {
    WindowFade window;window.reset(true,12345);window.requestShow(300);
    check(window.pending()->durationMs==0&&window.pending()->fromFixed16==-1,"already visible show is immediate");
    window.commit(0,true);check(window.currentFixed16()==65536&&!window.active(),"already visible show restores full alpha");
    window.reset(false,12345);window.requestHide(300);
    check(window.pending()->durationMs==0,"already hidden hide is immediate");
    window.commit(0,true);check(window.currentFixed16()==0,"already hidden hide clears residual alpha");
    for(const auto duration:{std::numeric_limits<int32_t>::min(),-1,0}) {
        window.reset(false,12345);window.requestShow(duration);window.commit(0,true);
        check(window.visible()&&window.currentFixed16()==65536&&!window.active(),"nonpositive show is immediate");
        window.requestHide(duration);window.commit(0,true);
        check(!window.visible()&&window.currentFixed16()==0&&!window.active(),"nonpositive hide of visible window is immediate");
    }
    window.reset();window.requestShow(300);window.requestHide(-1);
    check(window.pending()->visible&&window.pending()->durationMs==300,"negative hide when hidden is a complete no-op preserving pending show");
    window.commit(0,true);window.requestHide(0);window.requestAutoShow(300);
    check(window.pending()->visible&&window.pending()->fromFixed16==0&&window.pending()->durationMs==300,
          "140 auto-show can replace pending hide even while current flag is visible");
    window.commit(100,true);check(window.sample(250)==0,"140 auto-show retains special first-half hold");
}

static void resetAndLimits() {
    WindowFade window;window.requestShow(300);window.commit(1000,true);
    check(window.sample(999)==0,"pre-start sample is clamped to start");
    window.reset(true,32768);check(window.visible()&&window.currentFixed16()==32768&&!window.active()&&!window.pending(),"reset clears transient state");
    bool invalid=false;try{window.reset(false,65537);}catch(const std::invalid_argument&){invalid=true;}
    check(invalid&&window.currentFixed16()==32768,"invalid restore alpha preserves state");
    window.requestHide(std::numeric_limits<int32_t>::max());
    bool overflow=false;try{window.commit(std::numeric_limits<uint64_t>::max(),true);}catch(const std::overflow_error&){overflow=true;}
    check(overflow&&window.pending()&&!window.active()&&window.visible(),"overflow leaves current and pending state intact");
    window.reset();window.requestShow(1);window.commit(0,true);check(window.sample(1)==65536,"one-ms endpoint completes exactly");
    window.reset();check(!window.commit(0,true),"commit without request is a no-op");
}

int main() {
    try {nativeCurves();pendingAndReadiness();requestBranches();resetAndLimits();}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"Window fade checks passed\n";
}
