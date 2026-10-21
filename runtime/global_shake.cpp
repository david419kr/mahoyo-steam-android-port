#include "global_shake.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mahoyo::effects {
namespace {
uint64_t duration(const GlobalShakeRequest& request) {
    const auto cycles=static_cast<int64_t>(request.signedCycles);
    return static_cast<uint64_t>(cycles<0?-cycles:cycles)*request.periodMs;
}
float axisOffset(int32_t amplitude,float sine,float sign,float envelope,float numerator,float denominator) {
    // Preserve the native mulss/divss order instead of pre-multiplying a wave.
    float value=static_cast<float>(amplitude)*sine;
    value*=sign;
    value*=envelope;
    value*=numerator;
    return value/denominator;
}
}

void GlobalShake::queue(GlobalShakeRequest request) {
    if(!request.amplitudeX&&!request.amplitudeY) {queued_=request;return;}
    if(request.periodMs<=0||request.signedCycles==0||request.delayMs<0)
        throw std::invalid_argument("Global shake requires a positive period, nonzero finite cycles and nonnegative delay");
    if(duration(request)>static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
        throw std::invalid_argument("Global shake duration exceeds the native signed32 range");
    queued_=request;
}

void GlobalShake::commit(uint64_t tick) {
    if(queued_&&(queued_->amplitudeX||queued_->amplitudeY)
       &&static_cast<uint64_t>(queued_->delayMs)>std::numeric_limits<uint64_t>::max()-tick)
        throw std::overflow_error("Global shake start tick overflow");
    sample(tick);
    if(!queued_)return;
    const auto request=*queued_;
    queued_.reset();
    // The native factory creates no replacement job when both amplitudes are 0.
    if(!request.amplitudeX&&!request.amplitudeY)return;
    scheduled_=TimedRequest{request,tick+static_cast<uint64_t>(request.delayMs)};
    sample(tick);
}

GlobalShakeOffset GlobalShake::sample(uint64_t tick) {
    if(scheduled_&&tick>=scheduled_->startsAt) {
        active_=scheduled_;
        scheduled_.reset();
    }
    if(!active_||tick<active_->startsAt)return {};
    const auto& request=active_->request;
    const auto elapsed=tick-active_->startsAt;
    if(elapsed>=duration(request)) {
        active_.reset();
        return {};
    }
    constexpr double TwoPi=6.283185307179586;
    const float phase=static_cast<float>((static_cast<double>(elapsed)*TwoPi)/request.periodMs);
    const float sine=std::sin(phase);
    const float ratio=static_cast<float>(elapsed)/static_cast<float>(request.periodMs);
    const auto cycles=static_cast<int64_t>(request.signedCycles);
    const float envelope=1.0f-ratio/static_cast<float>(cycles<0?-cycles:cycles);
    const float sign=cycles<0?-1.0f:1.0f;
    return {axisOffset(request.amplitudeX,sine,sign,envelope,640.0f,480.0f),
            axisOffset(request.amplitudeY,sine,sign,envelope,480.0f,360.0f)};
}

void GlobalShake::reset() {
    queued_.reset();
    scheduled_.reset();
    active_.reset();
}

bool isGlobalShakeExcluded(std::string_view name) {
    const auto separator=name.find_last_of("/\\");
    if(separator!=std::string_view::npos)name.remove_prefix(separator+1);
    const auto extension=name.find_last_of('.');
    if(extension!=std::string_view::npos)name=name.substr(0,extension);
    for(const std::string_view candidate:{"black","blue","green","orange","red","white","yellow"}) {
        if(name.size()!=candidate.size())continue;
        bool same=true;
        for(std::size_t i=0;i<name.size();++i) {
            const char lower=name[i]>='A'&&name[i]<='Z'?name[i]-'A'+'a':name[i];
            if(lower!=candidate[i]) {same=false;break;}
        }
        if(same)return true;
    }
    return false;
}
} // namespace mahoyo::effects
