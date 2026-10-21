#include "focus_line.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mahoyo::focus_line {
namespace {
float f(float x){volatile float value=x;return value;}
int32_t opacity(int v){return int32_t(f(f(float(v)/100.f)*65536.f));}
bool zero(const Slot& s){return s.type==-1&&s.ageMs==0&&s.x==0&&s.y==0&&s.thetaDegrees==0&&s.opacity16==0;}
void dimensions(int32_t w,int32_t h){if(w<=0||h<=0||w>65536||h>65536)throw std::runtime_error("Invalid FocusLine native canvas");}
uint32_t random15(const std::function<uint32_t()>& rng){if(!rng)throw std::runtime_error("Missing FocusLine shared RNG");auto n=rng();if(n>32767)throw std::runtime_error("Invalid FocusLine rand15");return n;}
void spawn(Slot& s,int32_t w,int32_t h,const std::function<uint32_t()>& rng){
    s.type=0;s.ageMs=0;
    s.opacity16=opacity(int(random15(rng)%41)+10);
    s.thetaDegrees=f(float(random15(rng)%36000)/100.f);
    const float radius=float(random15(rng)%100+150);
    const float radians=float((double(s.thetaDegrees)*3.14159265358979)/180.0);
    s.x=f(f(std::cos(radians)*radius)+float(w>>1));
    s.y=f(float(h>>1)-f(std::sin(radians)*radius));
}
}
uint8_t alpha8(int32_t opacity16,int32_t fade0,int32_t fade1){
    if(opacity16<0||opacity16>65536||fade0<0||fade0>65536||fade1<0||fade1>65536)throw std::runtime_error("Invalid FocusLine alpha channel");
    const uint32_t a=uint32_t(opacity16)*255u;
    const uint32_t b=uint32_t((uint64_t(65536-fade0)*uint64_t(65536-fade1))>>16);
    return uint8_t((uint64_t(a)*b)>>32);
}
void validate(const State& s){
    if(s.mode!=0||(s.phase!=0&&s.phase!=2&&s.phase!=3)||s.substage<0||s.substage>2||s.fade0<0||s.fade0>65536||s.fadeElapsedMs<0||s.fadeElapsedMs>400)throw std::runtime_error("Invalid FocusLine state");
    if(s.phase==0){
        if(s.substage||s.durationMs||s.fadeElapsedMs||s.fade0!=65536||s.canvasWidth||s.canvasHeight)throw std::runtime_error("Invalid inactive FocusLine state");
        for(const auto& p:s.slots)if(!zero(p))throw std::runtime_error("Inactive FocusLine has particles");
        return;
    }
    dimensions(s.canvasWidth,s.canvasHeight);
    if(s.phase==2){if(s.durationMs||s.fadeElapsedMs>50||(s.substage!=2&&s.fadeElapsedMs)||(s.substage==2&&s.fade0))throw std::runtime_error("Invalid FocusLine start fade");}
    else if(s.durationMs!=400||s.substage>1||s.fadeElapsedMs>=400||(s.substage==1&&s.fade0!=int32_t((uint64_t(s.fadeElapsedMs)<<16)/400)))throw std::runtime_error("Invalid FocusLine stop fade");
    for(const auto& p:s.slots){
        if(p.type==-1){if(!zero(p))throw std::runtime_error("Invalid unspawned FocusLine slot");continue;}
        if(p.type!=0||p.ageMs<0||p.ageMs>=LifeMs||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.thetaDegrees)||p.thetaDegrees<0||p.thetaDegrees>327.67f||std::abs(p.x-float(s.canvasWidth>>1))>250||std::abs(p.y-float(s.canvasHeight>>1))>250)throw std::runtime_error("Invalid FocusLine particle");
        bool found=false;for(int v=10;v<=50;++v)if(p.opacity16==opacity(v)){found=true;break;}
        if(!found)throw std::runtime_error("Invalid FocusLine particle opacity");
    }
}
void Controller::commit(int32_t w,int32_t h){
    if(!pending_)return;
    if(pending_==1){
        dimensions(w,h);
        if(!active()){state_={};state_.canvasWidth=w;state_.canvasHeight=h;ready_=false;}
        else if(w!=state_.canvasWidth||h!=state_.canvasHeight)throw std::runtime_error("FocusLine canvas changed while active");
        state_.phase=2;state_.durationMs=0;
    }else{
        if(!active()){pending_=0;return;}
        state_.phase=3;state_.durationMs=400;
    }
    state_.substage=0;state_.fadeElapsedMs=0;clock_=false;pending_=0;
}
void Controller::resourcesReady(uint64_t now){if(active()&&!ready_){ready_=true;last_=poolLast_=now;clock_=true;}}
void Controller::sample(uint64_t now,bool ready,bool paused,const std::function<uint32_t()>& rng){
    if(!active()||!ready_)return;
    if(clock_&&(now<last_||now<poolLast_))throw std::runtime_error("FocusLine backwards clock");
    if(!ready){last_=poolLast_=now;clock_=true;return;}
    if(!clock_||state_.substage==0){last_=poolLast_=now;state_.substage=1;clock_=true;}
    const uint32_t delta=uint32_t(std::min<uint64_t>(now-last_,50));
    if(state_.substage==1)last_=now;
    // Native fade-off returns while paused before entering the pool updater.
    if(paused){if(state_.phase==2)poolLast_=now;return;}
    if(state_.phase==2){if(state_.substage==1)state_.fadeElapsedMs+=int32_t(delta);state_.fade0=0;state_.substage=2;}
    else{
        state_.fadeElapsedMs+=int32_t(delta);
        if(state_.fadeElapsedMs>=400){clear();return;}
        state_.fade0=int32_t((uint64_t(state_.fadeElapsedMs)<<16)/400);
    }
    const int32_t poolDelta=int32_t(std::min<uint64_t>(now-poolLast_,50));poolLast_=now;
    for(auto& s:state_.slots){
        if(s.type==-1){spawn(s,state_.canvasWidth,state_.canvasHeight,rng);continue;}
        s.ageMs+=poolDelta;
        if(s.ageMs>=LifeMs)spawn(s,state_.canvasWidth,state_.canvasHeight,rng);
    }
}
void Controller::restore(const State& s,uint64_t now){validate(s);state_=s;pending_=0;ready_=false;clock_=true;last_=poolLast_=now;}
void Controller::clear() noexcept {state_={};pending_=0;ready_=false;clock_=false;last_=poolLast_=0;}
std::array<Sprite,PoolSize> Controller::sprites() const {
    std::array<Sprite,PoolSize> out{};
    if(!active()||!ready_)return out;
    for(size_t i=0;i<out.size();++i){const auto& s=state_.slots[i];if(s.type==0)out[i]={s.x,s.y,s.thetaDegrees,alpha8(s.opacity16,state_.fade0)};}
    return out;
}
}
