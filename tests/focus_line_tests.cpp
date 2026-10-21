#include "focus_line.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
using namespace mahoyo::focus_line;
static uint32_t bits(float x){uint32_t v;std::memcpy(&v,&x,4);return v;}
static void equal(const State& a,const State& b){
    assert(a.mode==b.mode&&a.phase==b.phase&&a.substage==b.substage&&a.durationMs==b.durationMs&&a.fadeElapsedMs==b.fadeElapsedMs&&a.fade0==b.fade0&&a.canvasWidth==b.canvasWidth&&a.canvasHeight==b.canvasHeight);
    for(int i=0;i<PoolSize;++i){auto x=a.slots[i],y=b.slots[i];assert(x.type==y.type&&x.ageMs==y.ageMs&&bits(x.x)==bits(y.x)&&bits(x.y)==bits(y.y)&&bits(x.thetaDegrees)==bits(y.thetaDegrees)&&x.opacity16==y.opacity16);}
}
template<class F> static void reject(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}assert(caught);}
static void focused(){
    validate(State{});Controller c;int calls=0;auto rng=[&]{++calls;return 0u;};
    c.requestStart();c.commit(640,360);assert(c.resourcesPending()&&!c.safe());
    c.sample(500,true,false,rng);assert(calls==0);c.resourcesReady(500);
    c.sample(500,true,true,rng);assert(calls==0&&c.snapshot().fade0==65536);
    c.sample(900,true,false,rng);assert(calls==60&&c.safe());auto s=c.snapshot();validate(s);
    assert(s.slots[0].x==470&&s.slots[0].y==180&&s.slots[0].opacity16==6553);
    c.resourcesReady(950);c.sample(950,true,false,rng);assert(calls==60&&c.snapshot().slots[0].ageMs==50);
    c.sample(979,true,false,rng);assert(calls==60&&c.snapshot().slots[0].ageMs==79);
    c.sample(980,true,false,rng);assert(calls==120&&c.snapshot().slots[0].ageMs==0);
    c.sample(1980,true,false,rng);assert(calls==120&&c.snapshot().slots[0].ageMs==50);
    c.sample(2100,false,false,rng);c.sample(2101,true,false,rng);assert(c.snapshot().slots[0].ageMs==51);
    c.requestStart();c.commit(640,360);c.sample(2101,true,false,rng);assert(calls==120&&c.snapshot().slots[0].ageMs==51);
    c.requestStop();c.commit(640,360);c.sample(2101,true,false,rng);assert(c.snapshot().fade0==0);
    c.sample(2151,true,false,rng);assert(c.snapshot().fade0==8192&&calls==180);
    auto saved=c.snapshot();validate(saved);auto beforeCalls=calls;
    Controller restored;restored.restore(saved,5000);assert(calls==beforeCalls&&restored.resourcesPending());
    restored.sample(8000,true,false,rng);equal(restored.snapshot(),saved);restored.resourcesReady(8000);
    restored.sample(8000,true,false,rng);equal(restored.snapshot(),saved);
    restored.sample(9000,true,true,rng);equal(restored.snapshot(),saved);
    restored.sample(9001,true,false,rng);assert(restored.snapshot().fadeElapsedMs==51);
    auto stable=restored.snapshot();auto bad=stable;bad.slots[0].x=std::numeric_limits<float>::infinity();
    reject([&]{restored.restore(bad,0);});equal(restored.snapshot(),stable);
    bad=stable;bad.slots[0].opacity16=6554;reject([&]{restored.restore(bad,0);});
    bad=stable;bad.phase=1;reject([&]{restored.restore(bad,0);});
    bad=State{};bad.slots[1].x=1;reject([&]{validate(bad);});
    reject([&]{restored.sample(1,true,false,rng);});
    for(int i=0;i<8;++i)c.sample(2201+i*50,true,false,rng);
    assert(!c.active()&&c.safe());validate(c.snapshot());
    c.requestStart();c.commit(640,360);c.resourcesReady(3000);c.sample(3000,true,false,rng);
    assert(c.snapshot().slots[0].ageMs==0);c.clear();validate(c.snapshot());
    assert(alpha8(6553,0)==25&&alpha8(32768,32768)==63&&alpha8(32768,65536)==0);
    reject([]{alpha8(-1,0);});std::cout<<"FocusLine focused PASS\n";
}
int main(int argc,char** argv){
    if(argc==1){focused();return 0;}
    Controller c;uint32_t seed=1;uint64_t calls=0;
    auto rng=[&]{seed=seed*214013u+2531011u;++calls;return (seed>>16)&32767u;};
    std::string op;uint64_t now;int w=0,h=0,ready,paused;
    while(std::cin>>op){
        if(op=="start"){std::cin>>w>>h;c.requestStart();c.commit(w,h);}
        else if(op=="stop"){c.requestStop();c.commit(w,h);}
        else if(op=="ack"){std::cin>>now;c.resourcesReady(now);}
        else if(op=="restore"){std::cin>>now;c.restore(c.snapshot(),now);}
        else if(op=="tick"){std::cin>>now>>ready>>paused;c.sample(now,ready,paused,rng);}
        else if(op=="alpha"){int a,b,d;std::cin>>a>>b>>d;std::cout<<"A "<<int(alpha8(a,b,d))<<'\n';continue;}
        else return 2;
        auto s=c.snapshot();validate(s);
        std::cout<<"S "<<s.phase<<' '<<s.substage<<' '<<s.fadeElapsedMs<<' '<<s.fade0<<' '<<calls;
        for(const auto& p:s.slots)std::cout<<' '<<p.type<<' '<<p.ageMs<<' '<<bits(p.x)<<' '<<bits(p.y)<<' '<<bits(p.thetaDegrees)<<' '<<p.opacity16;
        std::cout<<'\n';
    }
}
