#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mahoyo::fall_note {
constexpr std::size_t SlotCount=50;
enum class Phase:uint8_t {Off,WaitingResources,Running,FadingOut};
struct Slot {
    int type=-1;
    uint32_t ageMs=0;
    float x=0,y=0,vx=0,vy=0,ax=0,ay=0;
};
// Port save policy: preserve the logical pool exactly. Native161320 saves the
// global command/fade configuration, not this internal sprite pool.
struct State {
    int mode=-1,pendingMode=-2; // -2=no command; -1=stop; 0/1/2=configure.
    Phase phase=Phase::Off;
    uint32_t fadeDurationMs=0,fadeElapsedMs=0,opacity16=0;
    std::array<Slot,SlotCount> slots{};
};
struct Sprite {
    int slot=0,type=0,frame=0,left=0,top=0,width=0,height=0;
    // Native14F950 simulation units. Resolve x*canvasWidth/640 and
    // y*canvasHeight/448 at the shared Native geometry bridge, not in Java.
    float referenceX=0,referenceY=0;
    uint8_t opacity8=0;
    int priority=22576;
};
using Rand15=std::function<uint32_t()>;
uint8_t opacity8(uint32_t opacity16,uint32_t fade0=0,uint32_t fade1=0);
void validate(const State&);
class Controller {
    State state_{};
    uint64_t last_=0;
    bool anchored_=false;
public:
    const State& state()const{return state_;}
    State snapshot()const{return state_;}
    void restore(const State&); // Validate first; no RNG/GPU/absolute clock.
    void request(int mode); // Native390=0,391(raw5)=1/otherwise2,392=-1.
    void commit(); // Existing scene prepare owns when this is called.
    void resourcesReady(uint64_t now); // Actual prepared resource/GL ACK only.
    void rebase(uint64_t now){last_=now;anchored_=true;}
    void step(uint64_t now,bool ready,bool paused,const Rand15&);
    bool safe()const{return state_.pendingMode==-2&&state_.phase!=Phase::WaitingResources;}
    bool active()const{return state_.phase==Phase::Running||state_.phase==Phase::FadingOut;}
    bool pendingResource()const{return state_.phase==Phase::WaitingResources;}
    bool needsTick()const{return state_.phase==Phase::Running||state_.phase==Phase::FadingOut;}
    std::vector<Sprite> sprites()const;
};
}
