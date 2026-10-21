#include "layer_shake.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mahoyo::effects;
static void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
static void close(float actual,float expected,const char* message,float tolerance=2e-6f) {
    check(std::abs(actual-expected)<=tolerance,message);
}
static void zero(LayerShakeOffset value,const char* message) {check(value.x==0&&value.y==0&&!value.active,message);}

static void nativeNumbers() {
    LayerShakeMode2 p{0,5,300,300};LayerShakeGeometry g;
    // Original1455e0 + native SSE2 sinf; see layer270-validation.json.
    const struct {uint64_t t;float y;} captures[]={
        {0,0},{75,2.9166667461395264f},{150,-1.6998873775264656e-7f},
        {225,-.9722222089767456f}
    };
    for(const auto& capture:captures) {
        auto value=sampleLayerShakeMode2(p,capture.t,g);
        check(value.active&&value.x==0,"mode2 stays active with one zero axis");
        close(value.y,capture.y,"native finite mode2 sample");
    }
    close(sampleLayerShakeMode2(p,150,g).y,-1.6998873775264656e-7f,
          "half-phase is rounded before float sine",1e-12f);
    zero(sampleLayerShakeMode2(p,300,g),"finite endpoint returns exact zero and completes");
    zero(sampleLayerShakeMode2(p,301,g),"finite effect cannot reappear after endpoint");
    zero(sampleLayerShakeMode2(p,75,g,true),"cancel clears offset even at nonzero native peak");
    p.durationMs=0;zero(sampleLayerShakeMode2(p,0,g),"zero duration completes without division");
    p.durationMs=-1;
    close(sampleLayerShakeMode2(p,75,g).y,3.8888888359069824f,"negative duration has no decay");
    auto atPeriod=sampleLayerShakeMode2(p,300,g);
    check(atPeriod.active&&atPeriod.x==0&&atPeriod.y==0,"modulo makes each indefinite period boundary exact zero");
    close(sampleLayerShakeMode2(p,375,g).y,sampleLayerShakeMode2(p,75,g).y,"indefinite phase repeats without precision drift");
    p.durationMs=-100;
    close(sampleLayerShakeMode2(p,75,g).y,3.8888888359069824f,"all negative native durations select the indefinite branch");
    p={0,-5,300,300};
    close(sampleLayerShakeMode2(p,75,g).y,-2.9166667461395264f,"signed amplitude reverses the axis");
}

static void independentGeometry() {
    LayerShakeMode2 p{4,5,300,300};LayerShakeGeometry g;
    const auto base=sampleLayerShakeMode2(p,75,g);
    close(base.x,1.875f,"mode2 X uses640/1024 rather than340 scale");
    close(base.y,2.9166667461395264f,"mode2 Y uses448/576");
    const float tx=g.projectionScaleX*30.0f,ty=g.projectionScaleY*30.16666603088379f;
    g.minProjectedX=tx;g.minProjectedY=ty;
    auto at=sampleLayerShakeMode2(p,75,g);
    close(at.x,base.x,"exact X threshold does not boost");close(at.y,base.y,"exact Y threshold does not boost");
    g.minProjectedX=std::nextafter(tx,std::numeric_limits<float>::infinity());
    auto xOnly=sampleLayerShakeMode2(p,75,g);
    close(xOnly.x,3.75f,"one float above X threshold doubles X");close(xOnly.y,base.y,"X boost does not affect Y");
    g.minProjectedX=tx;g.minProjectedY=std::nextafter(ty,std::numeric_limits<float>::infinity());
    auto yOnly=sampleLayerShakeMode2(p,75,g);
    close(yOnly.x,base.x,"Y boost does not affect X");close(yOnly.y,5.833333492279053f,"one float above Y threshold doubles Y");
    g.minProjectedX=tx+1;
    auto both=sampleLayerShakeMode2(p,75,g);
    close(both.x,3.75f,"both geometry axes can boost");close(both.y,5.833333492279053f,"native doubled Y capture");
    g={31,31,1,1};
    both=sampleLayerShakeMode2(p,75,g);
    close(both.x,3.75f,"threshold uses caller projection scale rather than fixed90");
    close(both.y,5.833333492279053f,"threshold uses independent Y projection scale");
    g={0,0,1,1};
    auto baseline=sampleLayerShakeMode2(p,75,g);
    close(baseline.x,base.x,"projection scale changes boost threshold, not internal waveform units");
    close(baseline.y,base.y,"output remains internal before caller design scaling");
}

static void limits() {
    LayerShakeMode2 p{1,2,300,300};LayerShakeGeometry g;
    for(int period:{0,-1,std::numeric_limits<int32_t>::min()}) {
        p.periodMs=period;bool rejected=false;
        try {sampleLayerShakeMode2(p,1,g);}catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"unsupported period cannot reach modulo or silently use a fallback");
    }
    p={std::numeric_limits<int32_t>::min(),1,300,300};bool rejected=false;
    try {sampleLayerShakeMode2(p,1,g);}catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"unreviewed native abs(INT_MIN) wrap remains explicit error");
    p={1,2,300,300};
    for(float scale:{0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        g.projectionScaleX=scale;rejected=false;
        try {sampleLayerShakeMode2(p,1,g);}catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"invalid projection cannot invent geometry branch behavior");
    }
    g={};g.minProjectedY=std::numeric_limits<float>::quiet_NaN();rejected=false;
    try {sampleLayerShakeMode2(p,1,g);}catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"unprojected geometry cannot silently choose the wrong boost");
    zero(sampleLayerShakeMode2(p,1,g,true),"cancel does not require rebuilding discarded geometry");
    zero(sampleLayerShakeMode2({0,0,0,0},0,g),"both-zero factory creates no job even without timing or geometry");
    g={};p={1,2,300,-1};
    check(sampleLayerShakeMode2(p,std::numeric_limits<uint64_t>::max(),g).active,
          "indefinite modulo is safe at large engine elapsed values");
}

static void randomMode0() {
    LayerShakeMode0 p{0,18,180,1500};LayerShakeGeometry g;LayerShakeRandom shake;
    uint32_t seed=1;unsigned draws=0;auto rng=[&](){++draws;seed=seed*214013u+2531011u;return(seed>>16)&32767u;};
    close(shake.sample(p,0,g,rng).y,8.920000076293945f,"native finite random first pair");
    check(draws==2,"zero X still consumes the original X RNG draw");
    g.minProjectedY=200;
    close(shake.sample(p,1,g,rng).y,8.920000076293945f,"geometry and envelope are held before the next random event");
    close(shake.sample(p,179,g,{}).y,8.920000076293945f,"held sample does not require or consume RNG");
    check(draws==2,"same logical timeline cannot consume another random pair");g={};
    close(shake.sample(p,180,g,rng).y,-10.243200302124023f,"native random period boundary and finite decay");
    check(draws==4,"one new pair at the exact boundary");
    close(shake.sample(p,1000,g,rng).y,2.5799999237060547f,"late sampling draws one pair without backfilling missed events");
    check(draws==6,"a large time jump consumes only one pair");
    zero(shake.sample(p,1500,g,rng),"finite random endpoint clears without consuming RNG");
    check(draws==6,"completion cannot consume a discarded random pair");
    zero(shake.sample(p,0,g,rng),"finished random job cannot restart on a repeated old timestamp");
    shake.reset();seed=1;draws=0;p={0,2,34,-1};
    close(shake.sample(p,0,g,rng).y,.3755555748939514f,"native indefinite random first value");
    close(shake.sample(p,33,g,rng).y,.3755555748939514f,"indefinite period holds without decay");
    close(shake.sample(p,34,g,rng).y,-1.1644444465637207f,"native indefinite random second value");
    zero(shake.sample(p,35,g,rng,true),"272 cancellation clears a held indefinite value");
    zero(shake.sample(p,68,g,rng),"canceled job cannot reappear at its next random boundary");
    check(draws==4,"cancellation and later sampling do not advance shared RNG");
    shake.reset();p={0,18,1,5000};auto zeroRng=[](){return 0u;};
    close(shake.sample(p,1000,g,zeroRng).y,14,"long-duration native random envelope has an initial1000ms plateau");
    close(shake.sample(p,3000,g,zeroRng).y,7,"long-duration decay uses duration minus1000");
    shake.reset();p={4,2,1,-1};g.minProjectedX=g.projectionScaleX*30;g.minProjectedY=g.projectionScaleY*30.16666603088379f;
    auto at=shake.sample(p,0,g,zeroRng);close(at.x,2.5f,"exact random X geometry boundary does not double");
    g.minProjectedX=std::nextafter(g.minProjectedX,std::numeric_limits<float>::infinity());
    close(shake.sample(p,1,g,zeroRng).x,5,"strict random geometry threshold doubles only on the next due event");
    shake.reset();bool rejected=false;try{shake.sample({-1,2,34,-1},0,{},rng);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"negative X shared-cache mode remains explicit unsupported");
    rejected=false;try{shake.sample({1,2,0,-1},0,{},rng);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"unreviewed zero-period local random behavior is not approximated");
    shake.reset();rejected=false;try{shake.sample({1,2,34,-1},0,{},[](){return 32768u;});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"RNG contract rejects values outside native15-bit output");
    zero(shake.sample({0,0,0,0},0,{},{}),"zero-amplitude factory shortcut requires neither clock nor RNG");
}

static void sharedNativeCache() {
    LayerShakeRandom a,b,follower,last;LayerShakeCache cache;LayerShakeGeometry plain,boost;boost.minProjectedY=200;
    uint32_t seed=1;int draws=0;auto rng=[&](){++draws;seed=seed*214013u+2531011u;return(seed>>16)&32767u;};
    close(a.sampleNative({0,20,34,1000},0,plain,rng,0,cache).y,-11.4644441605f,"native primary0 first RNG pair");
    close(cache.y,-11.4644441605f,"primary cache is geometry-independent");
    close(b.sampleNative({0,20,34,1000},0,plain,rng,1,cache).y,14.5955553055f,"native primary1 replaces shared pre-geometry cache");
    close(follower.sampleNative({-1,-1,34,1000},0,boost,{},2,cache).y,29.191110611f,"follower applies its own geometry boost to cache");
    close(last.sampleNative({-1,-1,34,1000},0,plain,{},63,cache).y,14.5955553055f,"another follower need not share the geometry boost");
    check(draws==4,"64-target script1 needs only two primary pairs");
    cache.y=7;boost.minProjectedY=0;
    close(follower.sampleNative({-1,-1,34,1000},33,boost,{},2,cache).y,29.191110611f,"held follower ignores changed cache and geometry");
    close(follower.sampleNative({-1,-1,34,1000},34,boost,{},2,cache).y,7,"due follower reads cache without a second finite envelope");
    zero(a.sampleNative({0,20,34,1000},1000,plain,{},0,cache),"primary completion returns exact zero");
    check(cache.x==0&&cache.y==0,"only real primary completion clears shared cache");
    cache={2,-3};LayerShakeRandom alone;
    close(alone.sampleNative({-1,-1,34,-1},0,plain,{},23,cache).y,-3,"follower-only job preserves retained nonzero context cache");
    zero(alone.sampleNative({-1,-1,34,-1},1,plain,{},23,cache,true),"follower cancellation completes without RNG");
    check(cache.x==2&&cache.y==-3,"follower cancellation must not clear primary cache");
}
static void nativeJobClock() {
    LayerShakeClock clock;clock.reset(0,400);check(!clock.sample(0),"delay begins after ready");
    for(uint64_t t:{49,99,149,199,249,299,349,399})check(!clock.sample(t),"Native delay waits through399ms");
    check(clock.sample(449)==0,"delay overshoot is discarded at the first active callback");
    check(clock.sample(449)==0,"repeat sample in one frame cannot advance the clock");
    check(clock.sample(1449)==50,"active callback uses the same50ms cap");
    clock.reset(0,400);check(!clock.sample(0),"new delayed job is not ready immediately");
    for(uint64_t t=1000;t<8000;t+=1000)check(!clock.sample(t),"coarse wall jumps do not skip native delay");
    check(clock.sample(8000)==0,"eight capped callbacks consume400ms delay");
    clock.reset(0,0);check(clock.sample(0)==0&&clock.sample(1000)==50,"zero-delay first frame starts at0");
    clock.rebase(900000);check(clock.sample(900000)==50&&clock.sample(900016)==66,"pause rebase preserves active elapsed");
    bool rejected=false;try{clock.reset(0,-1);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"negative delay is not an unsigned clock wrap");
}
int main() {nativeNumbers();independentGeometry();limits();randomMode0();sharedNativeCache();nativeJobClock();std::cout<<"Layer modes0/2 native math, state, shared cache and scheduler checks passed\n";}
