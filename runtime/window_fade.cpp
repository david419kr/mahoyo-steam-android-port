#include "window_fade.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mahoyo::effects {
void WindowFade::requestShow(int32_t durationMs) {
    const bool timed=durationMs>0&&!visible_;
    pending_=WindowFadeRequest{true,timed?0:-1,65536,timed?durationMs:0,0x10000};
}

void WindowFade::requestHide(int32_t durationMs) {
    if(durationMs<0&&!visible_)return;
    const bool timed=durationMs>0&&visible_;
    pending_=WindowFadeRequest{false,timed?65536:-1,0,timed?durationMs:0,0x10001};
}

void WindowFade::requestAutoShow(int32_t durationMs) {
    if(durationMs<0)throw std::invalid_argument("Window auto-show requires a nonnegative duration");
    pending_=WindowFadeRequest{true,0,65536,durationMs,0x10000};
}

bool WindowFade::commit(uint64_t tick,bool ready) {
    if(!pending_||!ready)return false;
    const auto request=*pending_;
    if(tick>std::numeric_limits<uint64_t>::max()-static_cast<uint64_t>(request.durationMs))
        throw std::overflow_error("Window fade end tick overflows");
    sample(tick);
    const int32_t from=request.fromFixed16==-1?alpha_:request.fromFixed16;
    visible_=request.visible;
    pending_.reset();
    if(request.durationMs>0) {
        active_=TimedRequest{request,tick,from};
        alpha_=from;
    } else {
        active_.reset();
        alpha_=request.targetFixed16;
    }
    return true;
}

int32_t WindowFade::sample(uint64_t tick) {
    if(!active_)return alpha_;
    const auto& timed=*active_;
    const uint64_t elapsed=tick<timed.startsAt?0:tick-timed.startsAt;
    if(elapsed>=static_cast<uint64_t>(timed.request.durationMs)) {
        alpha_=timed.request.targetFixed16;
        active_.reset();
        return alpha_;
    }
    // 0913a0 divides in double then narrows before the special float curves.
    const float ratio=static_cast<float>(static_cast<double>(elapsed)/timed.request.durationMs);
    float phase;
    if(timed.request.curve==0x10000) {
        const float half=std::max(ratio-.5f,0.f);
        phase=half+half;
    } else {
        phase=std::min(ratio+ratio,1.f);
    }
    float delta=static_cast<float>(timed.request.targetFixed16-timed.from)*phase;
    const float floor=std::floor(delta),fraction=delta-floor;
    constexpr float epsilon=1.f/16384.f;
    if(fraction>=1.f-epsilon)delta=std::ceil(delta);
    else if(fraction<=epsilon)delta=floor;
    alpha_=timed.from+static_cast<int32_t>(delta);
    return alpha_;
}

void WindowFade::reset(bool visible,int32_t alphaFixed16) {
    if(alphaFixed16<0||alphaFixed16>65536)throw std::invalid_argument("Window alpha is outside fixed16 range");
    visible_=visible;alpha_=alphaFixed16;pending_.reset();active_.reset();
}
} // namespace mahoyo::effects
