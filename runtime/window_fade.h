#pragma once
#include <cstdint>
#include <optional>

namespace mahoyo::effects {
struct WindowFadeRequest {
    bool visible=false;
    int32_t fromFixed16=-1,targetFixed16=0,durationMs=0,curve=0x10001;
};

// Visibility is the native logical flag. During a hide animation it is false
// while currentFixed16 can still be nonzero; rendering must use the alpha.
class WindowFade {
public:
    void requestShow(int32_t durationMs);
    void requestHide(int32_t durationMs);
    // The caller applies 140's text-mode/pending-state guards before calling.
    void requestAutoShow(int32_t durationMs);
    // Call with the pause-aware engine millisecond clock, after scene readiness.
    bool commit(uint64_t tick,bool ready);
    int32_t sample(uint64_t tick);
    bool visible() const {return visible_;}
    int32_t currentFixed16() const {return alpha_;}
    int32_t targetFixed16() const {return active_?active_->request.targetFixed16:alpha_;}
    bool active() const {return active_.has_value();}
    const std::optional<WindowFadeRequest>& pending() const {return pending_;}
    void reset(bool visible=false,int32_t alphaFixed16=0);
private:
    struct TimedRequest {WindowFadeRequest request;uint64_t startsAt;int32_t from;};
    bool visible_=false;
    int32_t alpha_=0;
    std::optional<WindowFadeRequest> pending_;
    std::optional<TimedRequest> active_;
};
} // namespace mahoyo::effects
