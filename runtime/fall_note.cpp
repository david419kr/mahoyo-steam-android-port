#include "fall_note.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mahoyo::fall_note {
namespace {
float f(float x){volatile float out=x;return out;}
constexpr int Frames[3]={24,20,16},Widths[3]={563,225,225},Heights[3]={187,112,150};
uint32_t draw(const Rand15& random){
    if(!random)throw std::invalid_argument("FallNote requires shared rand15");
    const auto n=random();if(n>32767)throw std::invalid_argument("FallNote RNG exceeds15 bits");return n;
}
void clear(State& state){state=State{};}
}
uint8_t opacity8(uint32_t alpha,uint32_t fade0,uint32_t fade1){
    if(alpha>65536||fade0>65536||fade1>65536)throw std::invalid_argument("Invalid sprite opacity16");
    const auto inverse=(uint64_t(65536-fade0)*(65536-fade1))>>16;
    return uint8_t((uint64_t(alpha*255u)*inverse)>>32);
}
void validate(const State& s){
    auto fail=[](){throw std::invalid_argument("Invalid saved FallNote state");};
    if(s.mode< -1||s.mode>2||s.pendingMode< -2||s.pendingMode>2||unsigned(s.phase)>unsigned(Phase::FadingOut)||s.opacity16>65536)fail();
    if(s.phase==Phase::Off){if(s.mode!=-1||s.opacity16||s.fadeDurationMs||s.fadeElapsedMs)fail();}
    else if(s.mode<0)fail();
    if(s.phase==Phase::Running&&(s.opacity16!=65536||s.fadeDurationMs||s.fadeElapsedMs))fail();
    if(s.phase==Phase::WaitingResources&&(s.opacity16||s.fadeDurationMs||s.fadeElapsedMs))fail();
    if(s.phase==Phase::FadingOut){
        if(s.fadeDurationMs!=800||s.fadeElapsedMs>=800||s.opacity16!=65536-uint32_t((uint64_t(s.fadeElapsedMs)<<16)/800))fail();
    }
    for(const auto& p:s.slots){
        if(p.type< -1||p.type>2||p.ageMs>=1200||(p.type>=0&&p.ageMs>=uint32_t(Frames[p.type]*50)))fail();
        for(float value:{p.x,p.y,p.vx,p.vy,p.ax,p.ay})if(!std::isfinite(value)||std::abs(value)>1000000.f)fail();
        if(s.phase==Phase::Off&&p.type!=-1)fail();
    }
}
void Controller::restore(const State& s){validate(s);state_=s;anchored_=false;last_=0;}
void Controller::request(int mode){
    if(mode< -1||mode>2)throw std::invalid_argument("Invalid FallNote mode");
    state_.pendingMode=mode;
}
void Controller::commit(){
    const int mode=state_.pendingMode;if(mode==-2)return;state_.pendingMode=-2;
    if(mode<0){
        if(state_.phase==Phase::Off)return;
        if(state_.phase==Phase::WaitingResources){clear(state_);anchored_=false;return;}
        state_.phase=Phase::FadingOut;state_.fadeDurationMs=800;state_.fadeElapsedMs=0;state_.opacity16=65536;anchored_=false;return;
    }
    const auto oldPhase=state_.phase;state_.mode=mode;
    if(oldPhase==Phase::Off||oldPhase==Phase::FadingOut){
        state_.phase=Phase::WaitingResources;state_.opacity16=0;state_.fadeDurationMs=state_.fadeElapsedMs=0;anchored_=false;
    }
}
void Controller::resourcesReady(uint64_t now){
    if(state_.phase!=Phase::WaitingResources)return;
    state_.phase=Phase::Running;state_.opacity16=65536;state_.fadeDurationMs=state_.fadeElapsedMs=0;rebase(now);
}
void Controller::step(uint64_t now,bool ready,bool paused,const Rand15& random){
    if(anchored_&&now<last_)throw std::invalid_argument("FallNote clock moved backwards");
    const auto elapsed=anchored_?std::min<uint64_t>(50,now-last_):0;last_=now;anchored_=true;
    if(!ready||paused||!needsTick())return;
    if(state_.phase==Phase::FadingOut){
        state_.fadeElapsedMs+=uint32_t(elapsed);
        if(state_.fadeElapsedMs>=800){clear(state_);anchored_=false;return;}
        state_.opacity16=65536-uint32_t((uint64_t(state_.fadeElapsedMs)<<16)/800);
    }
    const float probability=state_.mode==2?0.1f:0.05f;
    if(f(float(draw(random))/32767.f)<probability){
        auto at=std::find_if(state_.slots.begin(),state_.slots.end(),[](const Slot& p){return p.type<0;});
        if(at!=state_.slots.end()){
            auto& p=*at;p.type=int(draw(random)%3);p.ageMs=draw(random)%uint32_t(Frames[p.type]*50);
            p.x=float(int(draw(random)%801)-80);p.y=float(int(draw(random)%101)-130);
            p.vx=f(f(float(int(draw(random)%16)-8)*0.005f)-0.4f);
            p.vy=f(f(float(int(draw(random)%16)-8)*0.005f)+4.f);
            p.ax=f(float(int(draw(random)%16)-8)*0.00001f);
            constexpr float ay[3]={0.001f,0.0002f,0.0015f};p.ay=f(float(int(draw(random)%16)-8)*ay[p.type]);
        }
    }
    const float dt=f(float(elapsed*60)/1000.f);
    for(auto& p:state_.slots)if(p.type>=0){
        p.ageMs=(p.ageMs+uint32_t(elapsed))%uint32_t(Frames[p.type]*50);
        p.vy=f(f(dt*p.ay)+p.vy);p.vx=f(f(dt*p.ax)+p.vx);
        p.y=f(f(p.vy*dt)+p.y);p.x=f(f(p.vx*dt)+p.x);
        if(p.y>450.f)p.type=-1;
    }
}
std::vector<Sprite> Controller::sprites()const{
    std::vector<Sprite> out;if(state_.phase!=Phase::Running&&state_.phase!=Phase::FadingOut)return out;
    for(size_t i=0;i<SlotCount;++i){const auto& p=state_.slots[i];if(p.type<0)continue;
        const int frame=int(p.ageMs/50),width=Widths[p.type];
        out.push_back(Sprite{int(i),p.type,frame,frame*width,0,width,Heights[p.type],p.x,p.y,opacity8(state_.opacity16),22576});
    }
    return out;
}
}
