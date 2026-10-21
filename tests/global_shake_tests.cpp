#include "global_shake.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mahoyo::effects;

static void check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
static void close(float value,float expected,const char* message,float tolerance=2e-6f) {
    check(std::abs(value-expected)<=tolerance,message);
}
static void zero(GlobalShakeOffset value,const char* message) {
    check(value.x==0&&value.y==0,message);
}

static void nativeSamples() {
    // Original144ee0 + original SSE2 sinf captures, checked independently by
    // tools/host_reverse_global_shake.py; no production formula builds goldens.
    struct Golden {int cycles;uint64_t elapsed;float x,y;};
    const Golden goldens[]={
        {1,1,.33337289094924927f,.08334322273731232f},
        {1,50,8,2},
        {1,100,-4.662548178657744e-7f,-1.1656369736101624e-7f},
        {1,150,-2.6666667461395264f,-.6666666865348816f},
        {1,199,-.0016752302180975676f,-.0004188075545243919f},
        {2,1,.33421051502227783f,.08355262875556946f},
        {2,50,9.333333015441895f,2.3333332538604736f},
        {2,150,-6.666666507720947f,-1.6666666269302368f},
        {-3,1,-.33448970317840576f,-.08362242579460144f},
        {-3,50,-9.777777671813965f,-2.444444417953491f},
        {-3,150,8,2},
        {-3,199,.22392265498638153f,.055980660021305084f}
    };
    for(const auto& golden:goldens) {
        GlobalShake shake;shake.queue({8,2,200,golden.cycles,0});shake.commit(1000);
        zero(shake.sample(1000),"native sine starts at zero");
        const auto value=shake.sample(1000+golden.elapsed);
        close(value.x,golden.x,"native float32 X sample");
        close(value.y,golden.y,"native float32 Y sample");
        check(shake.active()&&!shake.pending(),"finite sample remains active without pending request");
        const auto duration=static_cast<uint64_t>(std::abs(golden.cycles))*200;
        zero(shake.sample(1000+duration),"native finite endpoint returns exact zero");
        check(!shake.active(),"finite endpoint releases job");
        zero(shake.sample(1000+duration+10000),"completed pulse cannot restart");
    }
    GlobalShake precision;precision.queue({8,2,200,1,0});precision.commit(0);
    // sinf(float(pi)) has this small negative residue; sin(double(pi)) would
    // hide the original phase quantization and fail this tighter comparison.
    const auto half=precision.sample(100);
    close(half.x,-4.662548178657744e-7f,"phase is rounded to float before sine",1e-12f);
    close(half.y,-1.1656369736101624e-7f,"float sine residue scales both axes",1e-12f);
}

static void queuedAndDelayedReplacement() {
    GlobalShake shake;
    shake.queue({8,2,200,1,0});
    check(shake.pending()&&!shake.active(),"uncommitted request is pending");
    zero(shake.sample(10000),"queued request does not start before commit");
    shake.commit(10000);
    close(shake.sample(10050).x,8,"first pulse starts at commit engine tick");
    shake.queue({3,0,100,1,100});
    close(shake.sample(10050).x,8,"queue alone preserves old pulse");
    shake.commit(10050);
    check(shake.active()&&shake.pending(),"old pulse and delayed replacement coexist");
    close(shake.sample(10075).x,4.71404504776001f,"old pulse continues during new delay");
    zero(shake.sample(10150),"replacement starts at delay endpoint with zero phase");
    check(shake.active()&&!shake.pending(),"replacement consumes scheduled request at start");
    close(shake.sample(10175).x,3,"new pulse replaces old at actual start");
    zero(shake.sample(10250),"new pulse reaches its own endpoint");

    shake.queue({8,2,200,1,0});shake.commit(20000);
    shake.queue({3,0,100,1,500});shake.commit(20050);
    zero(shake.sample(20200),"old pulse can finish while replacement is delayed");
    check(!shake.active()&&shake.pending(),"delay stays pending after old completion");
    close(shake.sample(20575).x,3,"late sample advances from scheduled start, not polling time");
    shake.commit(20575);
    close(shake.sample(20575).x,3,"empty commit does not restart active pulse");

    shake.reset();
    shake.queue({8,2,200,1,0});shake.queue({3,0,100,1,0});shake.commit(30000);
    close(shake.sample(30025).x,3,"last uncommitted request wins");

    shake.reset();
    shake.queue({8,2,200,10,0});shake.commit(40000);
    shake.queue({3,0,100,1,100});shake.commit(40050);
    zero(shake.sample(41000),"missed replacement still supersedes old job and finishes at its scheduled time");
    check(!shake.active()&&!shake.pending(),"late polling cannot resurrect old active or completed replacement");
}

static void zeroAndCancellation() {
    GlobalShake shake;
    // Native factory rejects both-zero amplitudes before inspecting timing.
    shake.queue({0,0,0,0,-10});shake.commit(0);
    check(!shake.active()&&!shake.pending(),"zero amplitudes create no job");
    shake.queue({8,2,200,1,0});shake.commit(100);
    shake.queue({0,0,200,1,0});shake.commit(150);
    close(shake.sample(150).x,8,"zero request does not replace current native job");
    shake.queue({3,0,100,1,100});shake.commit(150);
    shake.queue({0,0,0,0,0});shake.commit(175);
    check(shake.pending(),"zero request also preserves an already scheduled native job");
    close(shake.sample(275).x,3,"preserved delayed job still starts on time");
    shake.queue({8,2,200,1,50});
    shake.cancel();
    check(!shake.active()&&!shake.pending(),"cancel clears active scheduled and queued state");
    zero(shake.sample(100000),"canceled jobs do not reappear");
    shake.queue({0,2,200,1,0});shake.commit(100001);
    const auto singleAxis=shake.sample(100051);
    check(singleAxis.x==0,"one zero amplitude affects only that axis");close(singleAxis.y,2,"other axis remains active");
    shake.reset();
    check(!shake.active()&&!shake.pending(),"reset clears all state");
}

static void validationAndTickLimits() {
    const GlobalShakeRequest invalid[]={
        {1,1,0,1,0},{1,1,-1,1,0},{1,1,100,0,0},{1,1,100,1,-1},
        {1,1,100,std::numeric_limits<int32_t>::min(),0},
        {1,1,std::numeric_limits<int32_t>::max(),2,0}
    };
    for(const auto& request:invalid) {
        GlobalShake shake;shake.queue({8,2,200,1,0});
        bool rejected=false;
        try {shake.queue(request);}catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"unsupported timing and native duration overflow are rejected");
        shake.commit(0);close(shake.sample(50).x,8,"invalid queue preserves previous valid request");
    }
    GlobalShake shake;shake.queue({8,2,200,1,10});
    bool rejected=false;
    try {shake.commit(std::numeric_limits<uint64_t>::max()-9);}catch(const std::overflow_error&) {rejected=true;}
    check(rejected&&shake.pending()&&!shake.active(),"start overflow rejects without consuming pending request");
    shake.commit(std::numeric_limits<uint64_t>::max()-10);
    zero(shake.sample(std::numeric_limits<uint64_t>::max()),"maximum start tick stays finite and has zero phase");
    check(shake.active()&&!shake.pending(),"maximum tick avoids wrapping start time");
    shake.reset();shake.queue({1,0,std::numeric_limits<int32_t>::max(),1,0});shake.commit(0);
    check(shake.active(),"maximum native duration is accepted without end-tick overflow");
    zero(shake.sample(std::numeric_limits<int32_t>::max()),"maximum native duration still completes");
}

static void metadataExclusions() {
    for(const auto name:{"black","blue","green","orange","red","white","yellow",
                         "BLACK.MZP","assets/White.png","C:\\game\\yellow.mzp"})
        check(isGlobalShakeExcluded(name),"native static color-image metadata excludes global shake");
    for(const auto name:{"","img0408.mzp","black_00_20.mzp","blackish.mzp","black.mzp.extra","folder/"})
        check(!isGlobalShakeExcluded(name),"unlisted basename cannot inherit opt-out by prefix");
}

int main() {
    nativeSamples();queuedAndDelayedReplacement();zeroAndCancellation();
    validationAndTickLimits();metadataExclusions();
    std::cout<<"Global shake native samples and scheduling checks passed\n";
}
