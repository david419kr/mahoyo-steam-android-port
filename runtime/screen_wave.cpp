#include "screen_wave.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mahoyo::screen_wave {
void Controller::request(int preset,int fadeMs){
    if(preset>0&&preset!=1&&preset!=9)throw std::invalid_argument("Unsupported native Screen Raster preset");
    if(fadeMs<0)throw std::invalid_argument("Unsupported negative Screen Raster fade duration");
    pending_=Request{preset,fadeMs};
}
void Controller::commit(uint64_t now){
    sample(now);
    if(!pending_)return;
    auto request=*pending_;pending_.reset();
    if(request.preset>0){
        if(nextId_>=std::numeric_limits<uint64_t>::max()-1)throw std::invalid_argument("Screen Raster identity overflow");
        Record record;record.id=++nextId_;record.preset=request.preset;record.fadeMs=request.fadeMs;
        record_=record;drawLast_=now;
    }else if(record_){
        if(!record_->stopping){record_->stopping=true;record_->strengthElapsed=0;}
        //Native repeated stop changes duration without resetting cancelStart.
        record_->fadeMs=request.fadeMs;
    }
    sample(now);
}
void Controller::sample(uint64_t now,bool running){
    if(clock_&&now<last_)throw std::invalid_argument("Screen Raster logical clock moved backwards");
    const uint64_t delta=clock_?std::min<uint64_t>(now-last_,50):0;
    last_=now;clock_=true;
    if(!running||!record_)return;
    auto& r=*record_;const auto duration=uint64_t(r.fadeMs);
    r.strengthElapsed=std::min(duration,r.strengthElapsed+std::min(delta,duration));
    if(r.stopping&&r.strengthElapsed>=duration)record_.reset();
}
float Controller::strength(const Record& r){
    const float p=r.fadeMs>0?std::min(1.f,float(r.strengthElapsed)/float(r.fadeMs)):1.f;
    return r.stopping?1.f-p:p;
}
std::optional<Sample> Controller::draw(uint64_t now,bool running){
    if(!record_)return std::nullopt;
    auto& r=*record_;
    if(running){
        if(!r.drawStarted){r.drawStarted=true;r.phaseMs=0;}
        else{
            if(now<drawLast_)throw std::invalid_argument("Screen Raster draw clock moved backwards");
            r.phaseMs+=uint32_t(now-drawLast_);
        }
    }
    drawLast_=now;
    return Sample{r.id,r.preset,strength(r),r.phaseMs};
}
bool Controller::finite()const{return record_&&(record_->stopping||record_->strengthElapsed<uint64_t(record_->fadeMs));}
uint64_t Controller::remainingWait()const{return record_&&record_->stopping?uint64_t(record_->fadeMs)-record_->strengthElapsed:0;}
void Controller::validate(const Record& r){
    if(!r.id||(r.preset!=1&&r.preset!=9)||r.fadeMs<0||r.strengthElapsed>uint64_t(r.fadeMs)||(!r.drawStarted&&r.phaseMs)||
       (r.stopping&&r.strengthElapsed==uint64_t(r.fadeMs)))throw std::invalid_argument("Invalid saved Screen Raster state");
}
void Controller::restore(const std::optional<Record>& record,uint64_t generation,uint64_t now){
    if(generation==std::numeric_limits<uint64_t>::max())throw std::invalid_argument("Invalid saved Screen Raster identity");
    if(record){validate(*record);if(record->id!=generation)throw std::invalid_argument("Invalid saved Screen Raster generation");}
    record_=record;nextId_=generation;pending_.reset();last_=drawLast_=now;clock_=true;
}
void Controller::reset(){record_.reset();pending_.reset();last_=drawLast_=nextId_=0;clock_=false;}
}
