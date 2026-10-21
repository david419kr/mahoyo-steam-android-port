#pragma once
#include <cstdint>

namespace mahoyo::movie {
enum class Status { Idle, Preparing, Playing, Finished, Stopped, Failed };
struct Cue {
    const char* path="se01004.hw";
    int slot=2,backend=1,volumeStart=0,volumeEnd=64,pan=64;
    bool loop=true;
    uint64_t fadeMs=15000;
};
struct Actions {
    bool accepted=false,prepared=false,completed=false,failed=false,canceled=false;
    bool stopVideo=false,playSE=false,fadeBgm=false,needSeek=false;
    uint64_t id=0,stopVideoId=0,seekSerial=0,seekToMs=0,bgmFadeMs=3000;
    Cue cue;
};
struct State {
    Status status=Status::Idle;
    uint64_t id=0,positionMs=0,durationMs=0,seekSerial=0,seekToMs=0;
    bool prepared=false,waiting=false,paused=false,seeking=false;
};

// This is the original opening's cue/skip job, not a playback clock. Position
// comes only from the media backend; paused wall time never advances it.
class Opening {
public:
    static constexpr uint64_t CueAtMs=187500,SkipToMs=193000;
    const State& state() const { return state_; }
    bool active() const;
    Actions start(uint64_t id);
    Actions prepared(uint64_t id,uint64_t durationMs);
    // Enter the zero-pop1c1 job after1c0 and the script's timed commands.
    Actions wait(uint64_t id);
    Actions position(uint64_t id,uint64_t positionMs,uint64_t acknowledgedSeekSerial=0);
    Actions seeked(uint64_t id,uint64_t seekSerial,uint64_t positionMs);
    Actions eof(uint64_t id,uint64_t acknowledgedSeekSerial=0);
    // Preserve the backend's final position even if lifecycle pause suppressed
    // its last periodic position event. This is not elapsed wall time.
    Actions eof(uint64_t id,uint64_t acknowledgedSeekSerial,uint64_t terminalPositionMs);
    Actions skip(uint64_t id);
    // Acknowledge that playSE/fadeBgm commands have been committed to the host's
    // queue. This does not wait for their15s/3s audio fades to finish.
    Actions committed(uint64_t id);
    Actions setPaused(bool paused);
    Actions stop();
    Actions fail(uint64_t id);
private:
    enum class Stage { Cue, CueCommit, TailBoundary, FadeCommit, Eof };
    State state_;
    Stage stage_=Stage::Cue;
    bool skip_=false,ended_=false;
    bool matches(uint64_t id) const;
    Actions accepted() const;
    void evaluate(Actions& actions);
    void finish(Actions& actions);
};
} // namespace mahoyo::movie
