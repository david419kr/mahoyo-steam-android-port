#include "wave.h"
#include <algorithm>
#include <stdexcept>

namespace mahoyo::wave {
void Controller::request(int layer,int preset,int fadeMs) {
    if(layer>=64)return;
    if(fadeMs<0)throw std::invalid_argument("Unsupported negative wave fade duration");
    if(preset>0&&(preset<2||preset>51||preset==9))throw std::invalid_argument("Unsupported native wave preset");
    if(layer<0) {
        if(preset>0)throw std::invalid_argument("Unsupported global positive wave request");
        for(int id=0;id<64;++id)pending_.insert_or_assign(id,Request{id,preset,fadeMs});
    } else pending_.insert_or_assign(layer,Request{layer,preset,fadeMs});
}
void Controller::commit(uint64_t now) {
    sample(now);
    //141C60 scans only list210. New jobs are in list250 until the batch ends.
    //Any positive request force290-clears OLD jobs, never its new siblings.
    const bool replace=std::any_of(pending_.begin(),pending_.end(),[](const auto& p){return p.second.preset>0;});
    if(replace)active_.clear();
    else for(const auto& pair:pending_)for(auto& old:active_) {
        auto& r=old.second.record;
        if(!r.stopping){r.stopping=true;r.strengthElapsed=0;}
        //A repeated stop changes b8 but does not reset e0(cancelStart).
        r.fadeMs=pair.second.fadeMs;
    }
    //49..51 force old jobs above, then native config fails and no raster is installed.
    for(const auto& pair:pending_)if(pair.second.preset>0&&pair.second.preset<=48) {
        Record record;record.layer=pair.first;record.preset=pair.second.preset;record.fadeMs=pair.second.fadeMs;
        active_.insert_or_assign(pair.first,State{record,now});
    }
    pending_.clear();sample(now);
}
void Controller::sample(uint64_t now,bool running) {
    if(clock_&&now<last_)throw std::invalid_argument("Wave logical clock moved backwards");
    uint64_t delta=clock_?now-last_:0;last_=now;clock_=true;
    if(!running)return;
    for(auto it=active_.begin();it!=active_.end();) {
        auto& r=it->second.record;
        //Elapsed is capped: it is a strength phase, not a process timestamp.
        const auto duration=uint64_t(r.fadeMs);
        r.strengthElapsed=std::min(duration,r.strengthElapsed+std::min(delta,duration));
        if(r.stopping&&r.strengthElapsed>=duration)it=active_.erase(it);else ++it;
    }
}
float Controller::strength(const Record& r) {
    const float p=r.fadeMs>0?std::min(1.f,float(r.strengthElapsed)/float(r.fadeMs)):1.f;
    return r.stopping?1.f-p:p;
}
Sample Controller::draw(int layer,uint64_t now,bool visible,bool externalClock) {
    auto& state=active_.at(layer);auto& r=state.record;
    if(visible) {
        if(!r.drawStarted){r.drawStarted=true;r.phaseMs=0;}
        else if(!externalClock) {
            if(now<state.drawLast)throw std::invalid_argument("Wave draw clock moved backwards");
            r.phaseMs+=uint32_t(now-state.drawLast);
        }
    }
    state.drawLast=now;
    return {r.preset,strength(r),r.phaseMs};
}
void Controller::advancePhase(int layer,uint64_t elapsed) {
    auto found=active_.find(layer);
    if(found!=active_.end()&&found->second.record.drawStarted)found->second.record.phaseMs+=uint32_t(elapsed);
}
bool Controller::finite() const {
    return std::any_of(active_.begin(),active_.end(),[](const auto& pair){const auto& r=pair.second.record;return r.stopping||r.strengthElapsed<uint64_t(r.fadeMs);});
}
bool Controller::safe() const {return pending_.empty()&&!finite();}
uint64_t Controller::remainingWait(int layer) const {
    uint64_t remaining=0;
    for(const auto& pair:active_)if((layer<0||pair.first==layer)&&pair.second.record.stopping) {
        const auto& r=pair.second.record;remaining=std::max(remaining,uint64_t(r.fadeMs)-r.strengthElapsed);
    }
    return remaining;
}
void Controller::remove(int layer,bool clearPending) {
    if(layer<0){active_.clear();if(clearPending)pending_.clear();}
    else{active_.erase(layer);if(clearPending)pending_.erase(layer);}
}
void Controller::reset(){active_.clear();pending_.clear();last_=0;clock_=false;}
std::vector<Record> Controller::records() const {
    std::vector<Record> result;for(const auto& pair:active_)result.push_back(pair.second.record);return result;
}
void Controller::validate(const Record& r) {
    if(r.layer<0||r.layer>=64||(r.preset<2||r.preset>48||r.preset==9)||r.fadeMs<0||r.strengthElapsed>uint64_t(r.fadeMs)||
       (!r.drawStarted&&r.phaseMs)|| (r.stopping&&r.strengthElapsed==uint64_t(r.fadeMs)))throw std::invalid_argument("Invalid saved native wave state");
}
void Controller::restore(const std::vector<Record>& records,uint64_t now) {
    if(records.size()>64)throw std::invalid_argument("Invalid saved wave count");
    std::map<int,State> candidate;
    for(const auto& record:records){validate(record);if(!candidate.emplace(record.layer,State{record,now}).second)throw std::invalid_argument("Duplicate saved wave layer");}
    active_.swap(candidate);pending_.clear();last_=now;clock_=true;
}
}
