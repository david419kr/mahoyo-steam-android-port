#pragma once
#include <array>
#include <cstdint>
#include <functional>

namespace mahoyo::focus_line {
constexpr int PoolSize=20,LifeMs=80,Priority=22577;
struct Slot {
    int32_t type=-1,ageMs=0;
    float x=0,y=0,thetaDegrees=0;
    int32_t opacity16=0;
};
// WH20 is a port persistence policy; Native's pool is not serialized here.
// Absolute clocks, GPU handles and the caller's shared RNG are not part of it.
struct State {
    int32_t mode=0,phase=0,substage=0,durationMs=0,fadeElapsedMs=0,fade0=65536;
    int32_t canvasWidth=0,canvasHeight=0;
    std::array<Slot,PoolSize> slots{};
};
struct Sprite {float x,y,thetaDegrees;uint8_t opacity8;};
uint8_t alpha8(int32_t opacity16,int32_t fade0,int32_t fade1=0);
void validate(const State& state);
class Controller {
public:
    void requestStart() noexcept {pending_=1;}
    void requestStop() noexcept {pending_=-1;}
    bool pending() const noexcept {return pending_!=0;}
    bool active() const noexcept {return state_.phase!=0;}
    bool resourcesPending() const noexcept {return active()&&!ready_;}
    bool safe() const noexcept {return !pending()&&!resourcesPending();}
    void commit(int32_t nativeCanvasWidth,int32_t nativeCanvasHeight);
    void resourcesReady(uint64_t now);
    void sample(uint64_t now,bool ready,bool paused,const std::function<uint32_t()>& rand15);
    State snapshot() const noexcept {return state_;}
    void restore(const State& state,uint64_t now);
    void clear() noexcept;
    std::array<Sprite,PoolSize> sprites() const;
private:
    State state_{};
    int pending_=0;
    bool ready_=false,clock_=false;
    uint64_t last_=0,poolLast_=0;
};
}
