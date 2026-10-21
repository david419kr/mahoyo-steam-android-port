#pragma once
#include <cstdint>
#include <functional>
#include <optional>

namespace mahoyo::effects {
struct LayerShakeMode2 {
    int32_t amplitudeX=0,amplitudeY=0,periodMs=0,durationMs=0;
};
struct LayerShakeGeometry {
    float minProjectedX=0,minProjectedY=0;
    float projectionScaleX=3.0f,projectionScaleY=2.9779410362243652f;
};
struct LayerShakeOffset {float x=0,y=0;bool active=false;};
struct LayerShakeCache {float x=0,y=0;};

//141040 default contextScale1. Ready begins this clock; each scheduler delta is
//capped at50ms in both delay and playback, and delay overshoot is discarded.
class LayerShakeClock {
public:
    void reset(uint64_t now,int32_t delayMs);
    std::optional<uint64_t> sample(uint64_t now);
    void rebase(uint64_t now) noexcept {last_=now;}
private:
    uint64_t last_=0,delay_=0,delayElapsed_=0,elapsed_=0;
    bool started_=false;
};

// Script mode2 / native mode6 only. Elapsed excludes270's separate delayMs.
// The caller supplies the native projected quad with global offset but without
// this local shake; this helper does not reconstruct depth or parent geometry.
// Results are native internal offsets; integration currently supplies Z0 quads.
LayerShakeOffset sampleLayerShakeMode2(const LayerShakeMode2& parameters,uint64_t elapsedMs,
                                      const LayerShakeGeometry& geometry,bool canceled=false);

// Selected-layer script mode3 / native mode7. Same geometry and clock as mode2,
// but a half-period square step with constant amplitude, including finite jobs.
LayerShakeOffset sampleLayerShakeMode3(const LayerShakeMode2& parameters,uint64_t elapsedMs,
                                      const LayerShakeGeometry& geometry,bool canceled=false);

using LayerShakeMode0=LayerShakeMode2;
//One object per selected-layer mode0 job; parameters stay fixed until reset.
//RNG is the caller's shared native15-bit stream, called twice only on due updates.
class LayerShakeRandom {
public:
    LayerShakeOffset sample(const LayerShakeMode0& parameters,uint64_t elapsedMs,
                            const LayerShakeGeometry& geometry,
                            const std::function<uint32_t()>& rand15,bool canceled=false);
    LayerShakeOffset sampleNative(const LayerShakeMode0& parameters,uint64_t elapsedMs,
                            const LayerShakeGeometry& geometry,
                            const std::function<uint32_t()>& rand15,int layerId,
                            LayerShakeCache& shared,bool canceled=false,bool reducedByThree=false,bool suppressFiniteDecay=false);
    void reset() noexcept;
private:
    uint64_t nextAt_=0;
    LayerShakeOffset current_{};
    bool ended_=false;
};
// Opcode274 / native5: fixed indefinite, period-zero frame-count motion.
// frameToken deduplicates repeated Core sampling of one scheduler tick; it is
// not elapsed time. This branch never consults projected geometry or the cache.
class LayerMotion274 {
public:
    LayerShakeOffset sample(uint64_t frameToken,const std::function<uint32_t()>& rand15,bool canceled=false);
    void reset() noexcept;
private:
    int32_t packedX_=0,packedY_=0,packedPeriods_=0;
    uint64_t lastToken_=0;
    bool started_=false,ended_=false;
    LayerShakeOffset current_{};
};
} // namespace mahoyo::effects
