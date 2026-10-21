#pragma once
#include "fall_note.h"
#include "focus_line.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mahoyo::particles {
struct Sprite {
    std::string resource;
    std::array<int,4> sourceRect{};
    std::array<float,8> quad{}; // TL, BL, TR, BR, resolved canvas pixels.
    uint8_t opacity8=0;
    int priority=0;
};
struct Family {
    uint64_t revision=0;
    bool active=false,pendingResource=false;
    std::vector<std::string> resources;
    std::vector<Sprite> sprites;
};
struct Frame {Family fallNote,focusLine;};
class Overlays {
    fall_note::Controller fall_;
    focus_line::Controller focus_;
    uint64_t serial_=0,fallRevision_=0,focusRevision_=0;
    bool fallAwaiting_=false;
    uint64_t revision();
public:
    void requestFallNote(int mode){fall_.request(mode);}
    void requestFallNote391(int32_t raw){fall_.request(raw==5?1:2);}
    void requestFocusStart(){focus_.requestStart();}
    void requestFocusStop(){focus_.requestStop();}
    void commit();
    bool acknowledge(uint64_t sceneVersion,const std::string& payload,uint64_t now,bool paused);
    void step(uint64_t now,bool ready,bool paused,const std::function<uint32_t()>& rand15);
    bool pending()const{return fall_.state().pendingMode!=-2||focus_.pending()||fallAwaiting_||focus_.resourcesPending();}
    bool safe()const{return !pending()&&fall_.safe()&&focus_.safe();}
    bool active()const{return fallAwaiting_||fall_.active()||focus_.active();}
    bool needsTimedUpdates()const{return (fall_.needsTick()&&!fallAwaiting_)||(focus_.active()&&!focus_.resourcesPending());}
    int nextWake()const{return needsTimedUpdates()?16:0;}
    fall_note::State fallNoteState()const{return fall_.snapshot();}
    focus_line::State focusLineState()const{return focus_.snapshot();}
    void restore(const fall_note::State&,const focus_line::State&,uint64_t now);
    void clear();
    Frame frame()const;
    std::string json()const;
};
} // namespace mahoyo::particles
