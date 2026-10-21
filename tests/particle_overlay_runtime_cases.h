#pragma once
#include "../runtime/particle_overlay.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace particle_overlay_cases {
inline void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>inline void reject(F fn){try{fn();}catch(const std::exception&){return;}throw std::runtime_error("Invalid particle state accepted");}
inline void run(){
    using mahoyo::particles::Overlays;
    Overlays p;int draws=0;auto random=[&](){++draws;return 0u;};
    check(p.safe()&&!p.active()&&!p.pending()&&!p.nextWake(),"Particle legacy/off idle");
    p.requestFallNote(0);p.requestFocusStart();check(!p.safe()&&p.pending()&&!p.active(),"Uncommitted particles are save-busy without graphics");
    p.step(500,true,false,random);check(draws==0,"Pending handlers cannot start simulation");
    p.commit();auto before=p.frame();
    check(before.fallNote.pendingResource&&before.focusLine.pendingResource&&before.fallNote.resources.size()==3&&before.focusLine.resources.size()==1,"Native prepare declares exact family resources");
    check(before.fallNote.sprites.empty()&&before.focusLine.sprites.empty()&&!p.safe()&&!p.nextWake(),"Ready barrier has no sprites, RNG or idle render loop");
    p.step(1000,true,false,random);check(draws==0,"No pre-ACK RNG");
    const auto payload=std::string("7 ")+std::to_string(before.fallNote.revision)+' '+std::to_string(before.focusLine.revision);
    for(const auto& bad:{"-1 1 2","7 -1 2","7 1 18446744073709551616","7 01 2","7 1 2 extra","6 1 2"})check(!p.acknowledge(7,bad,1000,false),"Malformed/stale particle ACK accepted");
    check(!p.acknowledge(7,payload,1000,true),"Paused particle ACK accepted");
    check(p.acknowledge(7,payload,1000,false)&&draws==0&&p.safe()&&p.nextWake()==16,"ACK only prepares/rebases, without RNG");
    const auto prepared=p.json();check(!p.acknowledge(7,payload,999999,false)&&p.json()==prepared,"Duplicate ACK must not rebase or restart");
    p.step(1016,true,true,random);check(draws==0,"Core pause freezes both pools");
    p.step(1032,true,false,random);auto visible=p.frame();
    check(draws==69&&visible.fallNote.sprites.size()==1&&visible.focusLine.sprites.size()==20,"Shared rand15 FallNote before FocusLine");
    check(visible.fallNote.sprites.front().priority==22576&&visible.focusLine.sprites.front().priority==22577,"Native overlay priorities");
    const auto& focus=visible.focusLine.sprites.front();
    check(focus.sourceRect==std::array<int,4>{{0,0,960,2}}&&focus.quad==std::array<float,8>{{1110,540,1110,542,2070,540,2070,542}},"Focus source bitmap geometry has no extra projection3");
    const auto fall=p.fallNoteState();const auto line=p.focusLineState();const int beforeRestore=draws;const auto oldRevision=visible.fallNote.revision;
    p.restore(fall,line,5000);check(draws==beforeRestore&&!p.safe()&&p.pending()&&!p.nextWake(),"Restored stable pool needs new ready ACK, not resimulation");
    auto restored=p.frame();check(restored.fallNote.revision!=oldRevision&&restored.fallNote.sprites.empty(),"Restore uses fresh pending resource identity");
    const auto saved=p.json();auto invalid=line;invalid.slots[0].x=std::numeric_limits<float>::quiet_NaN();reject([&](){p.restore(fall,invalid,0);});check(saved==p.json(),"Particle restore must be atomic");
    const auto restoredAck=std::string("8 ")+std::to_string(restored.fallNote.revision)+' '+std::to_string(restored.focusLine.revision);
    check(p.acknowledge(8,restoredAck,5000,false),"Restored pool ACK");check(p.fallNoteState().slots[0].x==fall.slots[0].x&&p.focusLineState().slots[0].ageMs==line.slots[0].ageMs&&draws==beforeRestore,"Restore/ACK preserve pool and shared RNG");
    p.requestFallNote391(5);p.commit();check(p.fallNoteState().mode==1,"391 raw5 mode1");p.requestFallNote391(-1);p.commit();check(p.fallNoteState().mode==2,"391 other scalar mode2");
    p.requestFallNote(-1);p.requestFocusStop();p.commit();for(uint64_t t=5000;t<=5900;t+=50)p.step(t,true,false,random);
    check(!p.active()&&p.safe()&&!p.nextWake()&&p.frame().fallNote.resources.empty(),"Both finite stop fades return to true idle");
    auto f=mahoyo::fall_note::State{};f.mode=0;f.phase=mahoyo::fall_note::Phase::Running;f.opacity16=65536;f.slots[0]={0,0,100,112,0,0,0,0};p.restore(f,{},10000);
    auto a=p.frame();check(p.acknowledge(9,"9 "+std::to_string(a.fallNote.revision)+" 0",10000,false),"Fall-only restore ACK");
    check(p.frame().fallNote.sprites[0].quad==std::array<float,8>{{300,270,300,457,863,270,863,457}},"FallNote anchor scales, bitmap cell remains563x187");
    p.clear();check(p.safe()&&!p.pending()&&!p.active()&&p.json().find("FallNote0.mzp")==std::string::npos,"Title/newgame/legacy-load cleanup");
}
} // namespace particle_overlay_cases
