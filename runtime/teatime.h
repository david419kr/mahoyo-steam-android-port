#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace mahoyo::teatime {
enum class Phase { Opening, Ready, Saving, Closing };
enum class Result { None, Save, Title, ResumeCurrentVm };
struct State {
    uint64_t id=0,revision=0;
    int selector=0;
    Phase phase=Phase::Opening;
    uint64_t startedAt=0;
};

// The host supplies a fresh modal ID and a paused-aware logical clock. This
// controller never reads wall time, advances the VM or owns save/UI state.
class Controller {
public:
    void enter(uint64_t id,int selector,uint64_t tick);
    void restoreReady(uint64_t id,int selector);
    void reset() noexcept;
    bool active() const noexcept;
    const std::optional<State>& state() const noexcept;
    // Call before delivering input; Opening->Ready changes the revision.
    Result tick(uint64_t now);
    Result action(uint64_t id,uint64_t revision,const std::string& action,
                  uint64_t now,bool saveAllowed);
    // Modal alpha handles Closing. Individual buttons only stagger Opening;
    // multiply these once, rather than applying the Closing fade twice.
    uint32_t alphaFixed16(uint64_t now) const noexcept;
    uint32_t buttonAlphaFixed16(int nativeEvent,uint64_t now) const;
    bool needsTick() const noexcept;
    static bool isBlack(int selector);
private:
    std::optional<State> state_;
    void changePhase(Phase phase,uint64_t now);
};
} // namespace mahoyo::teatime
