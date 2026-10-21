#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace mahoyo::effects {
struct GlobalShakeRequest {
    int32_t amplitudeX=0,amplitudeY=0,periodMs=0,signedCycles=0,delayMs=0;
};
struct GlobalShakeOffset {float x=0,y=0;};

// Caller supplies its pause-aware engine timeline in milliseconds. Returned
// offsets use native internal coordinates, before the caller's layer transform.
class GlobalShake {
public:
    void queue(GlobalShakeRequest request);
    void commit(uint64_t tick);
    GlobalShakeOffset sample(uint64_t tick);
    // commit/sample advance these states; getters do not read a clock.
    bool active() const {return active_.has_value();}
    bool pending() const {return queued_.has_value()||scheduled_.has_value();}
    void reset();
    void cancel() {reset();}
private:
    struct TimedRequest {GlobalShakeRequest request;uint64_t startsAt=0;};
    std::optional<GlobalShakeRequest> queued_;
    std::optional<TimedRequest> scheduled_,active_;
};

// Native metadata table +0x34 bit0x200; this is independent of the layer's ID.
bool isGlobalShakeExcluded(std::string_view name);
} // namespace mahoyo::effects
