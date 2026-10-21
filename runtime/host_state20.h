#pragma once
#include "fall_note.h"
#include "focus_line.h"
#include "save_store.h"
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace mahoyo::host_state20 {
// Port continuation policy, not a claim that Native saves its particle pools.
// The Core owns this one RNG. Helpers, parsing and restore must never draw it.
struct State {
    std::set<int> registeredEffects;
    std::set<int> pausedEffects;
    uint32_t randomState=1;
    fall_note::State fallNote;
    focus_line::State focusLine;
};

namespace detail {
inline std::string token(std::istream& in){
    std::string value;if(!(in>>value))throw save::Error("Truncated WH20 state");return value;
}
inline uint32_t u32(std::istream& in){
    const auto text=token(in);uint32_t value=0;
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())throw save::Error("Invalid WH20 unsigned integer");
    return value;
}
inline int32_t i32(std::istream& in){
    const auto text=token(in);int32_t value=0;
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())throw save::Error("Invalid WH20 signed integer");
    return value;
}
inline void tag(std::istream& in,const char* expected){
    if(token(in)!=expected)throw save::Error("Invalid WH20 state tag");
}
inline uint32_t bits(float value){
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,"WH20 requires IEEE754 binary32");
    uint32_t result;std::memcpy(&result,&value,sizeof(result));return result;
}
inline float number(std::istream& in){
    const auto raw=u32(in);float value;std::memcpy(&value,&raw,sizeof(value));
    if(!std::isfinite(value))throw save::Error("Nonfinite WH20 float bits");return value;
}
inline std::set<int> effects(const std::vector<int>& active){
    if(active.size()>64)throw save::Error("Too many WH20 effects");
    std::set<int> result;
    for(int id:active)if(id<0||id>=64||!result.insert(id).second)throw save::Error("Invalid WH20 active effect identity");
    return result;
}
inline std::set<int> identities(std::istream& in,const char* expected){
    tag(in,expected);const auto count=u32(in);if(count>64)throw save::Error("Too many WH20 effect identities");
    std::set<int> result;int previous=-1;
    for(uint32_t n=0;n<count;++n){const auto id=i32(in);if(id<=previous||id>=64)throw save::Error("Invalid WH20 effect identity order");result.insert(id);previous=id;}
    return result;
}
} // namespace detail

inline void validate(const State& state,const std::vector<int>& activeEffects,const std::set<int>& layers){
    const auto active=detail::effects(activeEffects);
    if(layers.size()>64)throw save::Error("Too many WH20 layers");
    for(int id:layers)if(id<0||id>=64)throw save::Error("Invalid WH20 layer identity");
    for(int id:active)if(!layers.count(id))throw save::Error("WH20 active effect has no layer");
    for(int id:state.registeredEffects)if(!layers.count(id))throw save::Error("WH20 registered effect has no layer");
    for(int id:active)if(!state.registeredEffects.count(id))throw save::Error("WH20 active effect is not registered");
    for(int id:state.pausedEffects)if(!active.count(id)||!state.registeredEffects.count(id))throw save::Error("WH20 paused effect is not active/registered");
    // These uncommitted states are currently save-busy in the Core. Restore
    // readiness is separate: stable logical pools need a fresh GPU prepare ACK.
    if(state.fallNote.pendingMode!=-2||state.fallNote.phase==fall_note::Phase::WaitingResources)
        throw save::Error("Unprepared WH20 FallNote state");
    try{fall_note::validate(state.fallNote);focus_line::validate(state.focusLine);}
    catch(const std::exception& e){throw save::Error(std::string("Invalid WH20 pool: ")+e.what());}
}

// Append after BITMAPGEOMETRY1. No existing effect packet or container changes.
// Caller also checks both controllers.safe(): pending FocusLine/GPU state is
// intentionally outside its State POD and must not be silently discarded.
inline void write(std::ostream& destination,const State& state,const std::vector<int>& activeEffects,const std::set<int>& layers){
    validate(state,activeEffects,layers);
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"EFFECTREGISTRY1 "<<state.registeredEffects.size();for(int id:state.registeredEffects)out<<' '<<id;out<<'\n';
    out<<"EFFECTPAUSES1 "<<state.pausedEffects.size();for(int id:state.pausedEffects)out<<' '<<id;out<<'\n';
    out<<"CRT15STATE1 "<<state.randomState<<'\n';
    const auto& f=state.fallNote;
    out<<"FALLNOTE1 "<<f.mode<<' '<<f.pendingMode<<' '<<unsigned(f.phase)<<' '<<f.fadeDurationMs<<' '<<f.fadeElapsedMs<<' '<<f.opacity16<<' '<<fall_note::SlotCount<<'\n';
    for(const auto& s:f.slots)out<<s.type<<' '<<s.ageMs<<' '<<detail::bits(s.x)<<' '<<detail::bits(s.y)<<' '<<detail::bits(s.vx)<<' '<<detail::bits(s.vy)<<' '<<detail::bits(s.ax)<<' '<<detail::bits(s.ay)<<'\n';
    const auto& g=state.focusLine;
    out<<"FOCUSLINE1 "<<g.mode<<' '<<g.phase<<' '<<g.substage<<' '<<g.durationMs<<' '<<g.fadeElapsedMs<<' '<<g.fade0<<' '<<g.canvasWidth<<' '<<g.canvasHeight<<' '<<focus_line::PoolSize<<'\n';
    for(const auto& s:g.slots)out<<s.type<<' '<<s.ageMs<<' '<<detail::bits(s.x)<<' '<<detail::bits(s.y)<<' '<<detail::bits(s.thetaDegrees)<<' '<<s.opacity16<<'\n';
    if(!out)throw save::Error("Cannot encode WH20 state");
    destination<<out.str();if(!destination)throw save::Error("Cannot append WH20 state");
}

// Parse into a fresh value. Caller validates trailing EOF, builds temporary
// controllers, then commits this value together with the candidate VM/scene.
inline State read(std::istream& in,const std::vector<int>& activeEffects,const std::set<int>& layers){
    State state;
    state.registeredEffects=detail::identities(in,"EFFECTREGISTRY1");
    state.pausedEffects=detail::identities(in,"EFFECTPAUSES1");
    detail::tag(in,"CRT15STATE1");state.randomState=detail::u32(in);
    detail::tag(in,"FALLNOTE1");auto& f=state.fallNote;
    f.mode=detail::i32(in);f.pendingMode=detail::i32(in);const auto phase=detail::u32(in);
    if(phase>unsigned(fall_note::Phase::FadingOut))throw save::Error("Invalid WH20 FallNote phase");
    f.phase=static_cast<fall_note::Phase>(phase);f.fadeDurationMs=detail::u32(in);f.fadeElapsedMs=detail::u32(in);f.opacity16=detail::u32(in);
    if(detail::u32(in)!=fall_note::SlotCount)throw save::Error("Invalid WH20 FallNote slot count");
    for(auto& s:f.slots){s.type=detail::i32(in);s.ageMs=detail::u32(in);s.x=detail::number(in);s.y=detail::number(in);s.vx=detail::number(in);s.vy=detail::number(in);s.ax=detail::number(in);s.ay=detail::number(in);}
    detail::tag(in,"FOCUSLINE1");auto& g=state.focusLine;
    g.mode=detail::i32(in);g.phase=detail::i32(in);g.substage=detail::i32(in);g.durationMs=detail::i32(in);g.fadeElapsedMs=detail::i32(in);g.fade0=detail::i32(in);g.canvasWidth=detail::i32(in);g.canvasHeight=detail::i32(in);
    if(detail::u32(in)!=focus_line::PoolSize)throw save::Error("Invalid WH20 FocusLine slot count");
    for(auto& s:g.slots){s.type=detail::i32(in);s.ageMs=detail::i32(in);s.x=detail::number(in);s.y=detail::number(in);s.thetaDegrees=detail::number(in);s.opacity16=detail::i32(in);}
    validate(state,activeEffects,layers);return state;
}
} // namespace mahoyo::host_state20
