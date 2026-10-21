#include "layer_shake.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mahoyo::effects {
namespace {
float axisOffset(int32_t amplitude,float envelope,float sine,float numerator,float denominator) {
    const auto absolute=amplitude<0?-static_cast<int64_t>(amplitude):static_cast<int64_t>(amplitude);
    float value=static_cast<float>(absolute)*envelope;
    value*=amplitude<0?-1.0f:1.0f;
    value*=sine;
    value*=numerator;
    return value/denominator;
}
}

LayerShakeOffset sampleLayerShakeMode2(const LayerShakeMode2& p,uint64_t elapsed,
                                      const LayerShakeGeometry& g,bool canceled) {
    // The native factory checks zero amplitudes before considering timing.
    if(!p.amplitudeX&&!p.amplitudeY)return {};
    if(p.periodMs<=0||p.amplitudeX==std::numeric_limits<int32_t>::min()
       ||p.amplitudeY==std::numeric_limits<int32_t>::min())
        throw std::invalid_argument("Layer mode2 shake requires a positive period and bounded signed amplitudes");
    if(canceled||(p.durationMs>=0&&elapsed>=static_cast<uint64_t>(p.durationMs)))return {};
    if(!std::isfinite(g.minProjectedX)||!std::isfinite(g.minProjectedY)
       ||!std::isfinite(g.projectionScaleX)||!std::isfinite(g.projectionScaleY)
       ||g.projectionScaleX<=0||g.projectionScaleY<=0)
        throw std::invalid_argument("Layer mode2 shake requires finite projected quad coordinates and positive projection scales");
    const float envelope=p.durationMs<0?1.0f:
        static_cast<float>(static_cast<uint64_t>(p.durationMs)-elapsed)/static_cast<float>(p.durationMs);
    // Original1455e0 converts remainder to float, uses this double pi, converts
    // the half-phase back to float, then doubles it before calling sinf.
    constexpr double NativePi=3.14159265358979;
    const float remainder=static_cast<float>(elapsed%static_cast<uint64_t>(p.periodMs));
    const float period=static_cast<float>(p.periodMs);
    const float halfPhase=static_cast<float>((static_cast<double>(remainder)*NativePi)/period);
    const float sine=std::sin(halfPhase+halfPhase);
    float x=axisOffset(p.amplitudeX,envelope,sine,640.0f,1024.0f);
    float y=axisOffset(p.amplitudeY,envelope,sine,448.0f,576.0f);
    const float thresholdX=g.projectionScaleX*30.0f;
    const float thresholdY=g.projectionScaleY*30.16666603088379f;
    if(g.minProjectedX>thresholdX)x+=x;
    if(g.minProjectedY>thresholdY)y+=y;
    return {x,y,true};
}

LayerShakeOffset sampleLayerShakeMode3(const LayerShakeMode2& p,uint64_t elapsed,
                                      const LayerShakeGeometry& g,bool canceled) {
    if(!p.amplitudeX&&!p.amplitudeY)return {};
    if(p.periodMs<=0||p.amplitudeX==std::numeric_limits<int32_t>::min()
       ||p.amplitudeY==std::numeric_limits<int32_t>::min())
        throw std::invalid_argument("Layer mode3 shake requires a positive period and bounded signed amplitudes");
    if(canceled||(p.durationMs>=0&&elapsed>=static_cast<uint64_t>(p.durationMs)))return {};
    if(!std::isfinite(g.minProjectedX)||!std::isfinite(g.minProjectedY)
       ||!std::isfinite(g.projectionScaleX)||!std::isfinite(g.projectionScaleY)
       ||g.projectionScaleX<=0||g.projectionScaleY<=0)
        throw std::invalid_argument("Layer mode3 shake requires finite projected quad coordinates and positive projection scales");
    constexpr double NativePi=3.14159265358979;
    const float remainder=static_cast<float>(elapsed%static_cast<uint64_t>(p.periodMs));
    const float period=static_cast<float>(p.periodMs);
    const float halfPhase=static_cast<float>((static_cast<double>(remainder)*NativePi)/period);
    //1455E0 native7 quantizes the half-phase before sinf, and never applies
    //native6's finite-duration envelope. Keep the original float boundaries.
    const float phase=halfPhase>=1.5707963705062866f?.7853981852531433f:0.0f;
    const float sine=std::sin(phase+phase);
    float x=axisOffset(p.amplitudeX,1.0f,sine,640.0f,1024.0f);
    float y=axisOffset(p.amplitudeY,1.0f,sine,448.0f,576.0f);
    if(g.minProjectedX>g.projectionScaleX*30.0f)x+=x;
    if(g.minProjectedY>g.projectionScaleY*30.16666603088379f)y+=y;
    return {x,y,true};
}

void LayerShakeRandom::reset() noexcept {nextAt_=0;current_={};ended_=false;}

void LayerShakeClock::reset(uint64_t now,int32_t delay) {
    if(delay<0)throw std::invalid_argument("Layer shake delay must be nonnegative");
    last_=now;delay_=uint64_t(delay);delayElapsed_=elapsed_=0;started_=false;
}
std::optional<uint64_t> LayerShakeClock::sample(uint64_t now) {
    const auto delta=now>=last_?std::min<uint64_t>(50,now-last_):0;
    last_=now;
    if(!started_){delayElapsed_+=delta;if(delayElapsed_<delay_)return std::nullopt;started_=true;elapsed_=0;}
    else elapsed_+=delta;
    return elapsed_;
}

LayerShakeOffset LayerShakeRandom::sample(const LayerShakeMode0& p,uint64_t elapsed,
                                         const LayerShakeGeometry& g,
                                         const std::function<uint32_t()>& random,bool canceled) {
    if(p.amplitudeX<0)throw std::invalid_argument("Unsupported local mode0 shared-X source");
    LayerShakeCache unused;
    return sampleNative(p,elapsed,g,random,2,unused,canceled);
}

LayerShakeOffset LayerShakeRandom::sampleNative(const LayerShakeMode0& p,uint64_t elapsed,
                                         const LayerShakeGeometry& g,
                                         const std::function<uint32_t()>& random,int layerId,
                                         LayerShakeCache& shared,bool canceled,bool reducedByThree,bool suppressFiniteDecay) {
    if(layerId<0||layerId>=64)throw std::invalid_argument("Layer shake index outside0..63");
    if(!p.amplitudeX&&!p.amplitudeY)return {};
    if(ended_)return {};
    if(canceled||(p.durationMs>=0&&elapsed>=static_cast<uint64_t>(p.durationMs))){ended_=true;current_={};if(layerId<2)shared={};return {};}
    if(p.periodMs<=0)throw std::invalid_argument("Layer mode0 requires a positive period");
    if(elapsed<nextAt_)return current_;
    if(!std::isfinite(g.minProjectedX)||!std::isfinite(g.minProjectedY)
       ||!std::isfinite(g.projectionScaleX)||!std::isfinite(g.projectionScaleY)
       ||g.projectionScaleX<=0||g.projectionScaleY<=0)
        throw std::invalid_argument("Layer mode0 shake requires finite quad coordinates and positive projection scales");
    const auto nextDelta=static_cast<uint64_t>(p.periodMs)-elapsed%static_cast<uint64_t>(p.periodMs);
    if(nextDelta>std::numeric_limits<uint64_t>::max()-elapsed)throw std::overflow_error("Layer mode0 next tick overflow");
    auto denominator=[](float extent){
        const double value=extent+1.0f;
        if(!std::isfinite(value)||value<INT32_MIN||value>INT32_MAX||int32_t(value)==0)
            throw std::invalid_argument("Layer mode0 amplitude exceeds the native random modulus range");
        return int32_t(value);
    };
    auto draw=[&](){
        if(!random)throw std::invalid_argument("Layer mode0 shake requires the shared native RNG");
        const uint32_t value=random();if(value>32767)throw std::invalid_argument("Layer mode0 RNG value exceeds15-bit range");return int32_t(value);
    };
    float x=shared.x,y=shared.y;
    if(layerId<2||p.amplitudeX>=0){
      const float extentX=static_cast<float>(p.amplitudeX)*62.5f;
      const float extentY=static_cast<float>(p.amplitudeY)*77.77778f;
      const int32_t modulusX=denominator(extentX),modulusY=denominator(extentY);
      x=(extentX-static_cast<float>((draw()%modulusX)*2))/100.0f;
      y=(extentY-static_cast<float>((draw()%modulusY)*2))/100.0f;
      //Native3 (script5) keeps amplitude until the ordinary finite endpoint.
      if(p.durationMs>0&&!suppressFiniteDecay){
        const float remaining=static_cast<float>(static_cast<uint64_t>(p.durationMs)-elapsed);
        const float denominatorMs=p.durationMs<2001?static_cast<float>(p.durationMs):
            std::fmax(static_cast<float>(p.durationMs-1000),static_cast<float>(p.durationMs)*.5f);
        const float envelope=std::fmin(1.0f,remaining/denominatorMs);
        x*=envelope;y*=envelope;
      }
      //Native4 (script6) divides after random/envelope, before cache/geometry.
      if(reducedByThree){x/=3.0f;y/=3.0f;}
      if(layerId<2)shared={x,y};
    }
    if(g.minProjectedX>g.projectionScaleX*30.0f)x+=x;
    if(g.minProjectedY>g.projectionScaleY*30.16666603088379f)y+=y;
    current_={x,y,true};nextAt_=elapsed+nextDelta;return current_;
}
void LayerMotion274::reset() noexcept {
    packedX_=packedY_=packedPeriods_=0;lastToken_=0;started_=ended_=false;current_={};
}
LayerShakeOffset LayerMotion274::sample(uint64_t token,const std::function<uint32_t()>& random,bool canceled) {
    if(canceled){ended_=true;current_={};return {};}
    if(ended_)return {};
    if(started_&&lastToken_==token)return current_;
    auto draw=[&](){if(!random)throw std::invalid_argument("274 requires the shared native RNG");
        auto value=random();if(value>32767)throw std::invalid_argument("274 RNG exceeds15-bit range");return float(value);};
    int px=packedPeriods_%100+60,py=packedPeriods_/100+60;
    int ax=packedX_/1000,ay=packedY_/1000;
    int rx=std::max(0,std::abs(packedX_)%1000-1),ry=std::max(0,std::abs(packedY_)%1000-1);
    int sx,sy;
    if(!started_){sx=draw()/32767.0f>=.5f?-1:1;sy=draw()/32767.0f>=.5f?-1:1;rx=ry=0;}
    else{sx=ax>0?1:-1;sy=ay>0?1:-1;}
    auto range=[&](float scale){float value=draw()/-32767.0f;value*=scale;return int(scale)-int(value);};
    if(rx<1){px=range(60.0f);rx=px;ax=range(5.0f)*sx;}
    if(ry<1){py=range(60.0f);ry=py;ay=range(5.0f)*sy;}
    auto sine=[](int remaining,int period){float phase=float(remaining)*6.2831854820251465f;phase/=float(period);return std::sin(phase);};
    const float sineY=sine(ry,py),absoluteY=std::abs(sineY);
    const bool update=absoluteY<.5f?ry%4==0:absoluteY<.8f?ry%3==0:absoluteY<.96f?ry%2==0:true;
    if(update){float x=sine(rx,px)*float(ax);x*=640.0f;x*=.0009765625f;
        float y=float(ay)*.3f;y*=sineY;y*=448.0f;y/=576.0f;current_={x,y,true};}
    else current_.active=true;
    packedX_=(std::abs(ax)*1000+rx)*sx;packedY_=(std::abs(ay)*1000+ry)*sy;packedPeriods_=py*100+px-6060;
    started_=true;lastToken_=token;return current_;
}
} // namespace mahoyo::effects
