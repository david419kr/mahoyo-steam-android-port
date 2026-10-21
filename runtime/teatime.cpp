#include "teatime.h"
#include <algorithm>
#include <stdexcept>

namespace mahoyo::teatime {
namespace {
constexpr uint32_t FullAlpha=65536;
uint64_t elapsed(uint64_t now,uint64_t start) noexcept { return now>=start?now-start:0; }
void validate(uint64_t id,int selector) {
    if(!id)throw std::invalid_argument("TEATIME ID must be nonzero");
    if(selector<1||selector>13)throw std::invalid_argument("TEATIME selector must be1..13");
}
}

void Controller::enter(uint64_t id,int selector,uint64_t tick) {
    validate(id,selector);
    state_=State{id,1,selector,Phase::Opening,tick};
}
void Controller::restoreReady(uint64_t id,int selector) {
    validate(id,selector);
    state_=State{id,1,selector,Phase::Ready,0};
}
void Controller::reset() noexcept { state_.reset(); }
bool Controller::active() const noexcept { return state_.has_value(); }
const std::optional<State>& Controller::state() const noexcept { return state_; }
void Controller::changePhase(Phase phase,uint64_t now) {
    state_->phase=phase;
    state_->startedAt=now;
    ++state_->revision;
}
Result Controller::tick(uint64_t now) {
    if(!state_)return Result::None;
    if(state_->phase==Phase::Opening&&elapsed(now,state_->startedAt)>=1000)
        changePhase(Phase::Ready,now);
    else if(state_->phase==Phase::Closing&&elapsed(now,state_->startedAt)>=500) {
        reset();
        return Result::ResumeCurrentVm;
    }
    return Result::None;
}
Result Controller::action(uint64_t id,uint64_t revision,const std::string& action,
                          uint64_t now,bool saveAllowed) {
    if(!state_||state_->id!=id||state_->revision!=revision)return Result::None;
    if(action=="save-return") {
        if(state_->phase==Phase::Saving)changePhase(Phase::Ready,now);
        return Result::None;
    }
    if(state_->phase!=Phase::Ready)return Result::None;
    if(action=="save"&&saveAllowed) {
        changePhase(Phase::Saving,now);
        return Result::Save;
    }
    if(action=="next")changePhase(Phase::Closing,now);
    else if(action=="title") {
        reset();
        return Result::Title;
    }
    return Result::None;
}
uint32_t Controller::alphaFixed16(uint64_t now) const noexcept {
    if(!state_||state_->phase==Phase::Saving)return 0;
    if(state_->phase!=Phase::Closing)return FullAlpha;
    const auto t=std::min<uint64_t>(500,elapsed(now,state_->startedAt));
    return FullAlpha-static_cast<uint32_t>(t*FullAlpha/500);
}
uint32_t Controller::buttonAlphaFixed16(int nativeEvent,uint64_t now) const {
    if(nativeEvent<0||nativeEvent>3)throw std::invalid_argument("TEATIME native event must be0..3");
    if(!state_||state_->phase==Phase::Saving)return 0;
    if(state_->phase!=Phase::Opening)return FullAlpha;
    const auto t=elapsed(now,state_->startedAt),delay=uint64_t(nativeEvent)*200;
    const auto progress=t>=delay?std::min<uint64_t>(400,t-delay):0;
    return static_cast<uint32_t>(progress*FullAlpha/400);
}
bool Controller::needsTick() const noexcept {
    return state_&&(state_->phase==Phase::Opening||state_->phase==Phase::Closing);
}
bool Controller::isBlack(int selector) {
    switch(selector) {
    case 1:case 3:case 6:case 7:case 8:case 9:case 13:return true;
    case 2:case 4:case 5:case 10:case 11:case 12:return false;
    default:throw std::invalid_argument("TEATIME selector must be1..13");
    }
}
} // namespace mahoyo::teatime
