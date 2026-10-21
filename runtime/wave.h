#pragma once
#include <cstdint>
#include <map>
#include <vector>

namespace mahoyo::wave {
struct Request {int layer=0,preset=0,fadeMs=0;};
struct Record {
    int layer=0,preset=0,fadeMs=0;
    uint64_t strengthElapsed=0;
    bool stopping=false,drawStarted=false;
    uint32_t phaseMs=0;
};
struct Sample {int preset;float strength;uint32_t phaseMs;};

/** Native kind2 lifecycle; reviewed local presets2..8 and10..48 (not local9).
 * Requests49..51 complete without a raster after forcing oldkind2 jobs.
 * Render handles and absolute platform clocks stay out. */
class Controller {
public:
    //>=64 consumes/ignores at the host boundary; negative positive is unreviewed.
    void request(int layer,int preset,int fadeMs);
    const std::map<int,Request>& pending() const {return pending_;}
    void commit(uint64_t now);
    void sample(uint64_t now,bool running=true);
    //Caller invokes once per emitted layer sample; repeated same-tick reads do
    //not advance. A hidden sample rebases the anchor without advancing phase.
    Sample draw(int layer,uint64_t now,bool visible,bool externalClock=false);
    //External monotonic clock may advance a retained RT's phase without a
    //draw, timer, strength update or snapshot. First draw still starts at0.
    void advancePhase(int layer,uint64_t elapsed);
    bool contains(int layer) const {return active_.count(layer)!=0;}
    bool active() const {return !active_.empty();}
    bool safe() const;
    bool finite() const;
    uint64_t remainingWait(int layer) const; //normal persistent kind2 is skipped.
    void remove(int layer,bool clearPending=true);
    void reset();
    std::vector<Record> records() const;
    void restore(const std::vector<Record>& records,uint64_t now);
private:
    struct State {Record record;uint64_t drawLast=0;};
    std::map<int,Request> pending_;
    std::map<int,State> active_;
    uint64_t last_=0;
    bool clock_=false;
    static void validate(const Record& record);
    static float strength(const Record& record);
};
}
