#include "movie.h"
#include <stdexcept>

namespace mahoyo::movie {
bool Opening::active() const {return state_.status==Status::Preparing||state_.status==Status::Playing;}
bool Opening::matches(uint64_t id) const {return active()&&id!=0&&id==state_.id;}
Actions Opening::accepted() const {Actions result;result.accepted=true;result.id=state_.id;return result;}
Actions Opening::start(uint64_t id){
    if(!id||id<=state_.id)throw std::invalid_argument("Movie IDs must be increasing nonzero generations");
    Actions actions;actions.accepted=true;actions.id=id;
    if(active()){actions.stopVideo=true;actions.stopVideoId=state_.id;actions.canceled=true;}
    const auto paused=state_.paused;state_=State{};state_.id=id;state_.status=Status::Preparing;state_.paused=paused;
    stage_=Stage::Cue;skip_=false;ended_=false;return actions;
}
Actions Opening::prepared(uint64_t id,uint64_t durationMs){
    if(!matches(id)||state_.prepared)return {};
    auto actions=accepted();state_.prepared=true;state_.durationMs=durationMs;state_.status=Status::Playing;
    actions.prepared=true;evaluate(actions);return actions;
}
Actions Opening::wait(uint64_t id){
    if(!matches(id))return {};
    auto actions=accepted();state_.waiting=true;evaluate(actions);return actions;
}
Actions Opening::position(uint64_t id,uint64_t positionMs,uint64_t acknowledgedSeekSerial){
    if(!matches(id)||!state_.prepared||state_.paused||state_.seeking||acknowledgedSeekSerial!=state_.seekSerial||positionMs<state_.positionMs)return {};
    auto actions=accepted();state_.positionMs=positionMs;evaluate(actions);return actions;
}
Actions Opening::seeked(uint64_t id,uint64_t serial,uint64_t positionMs){
    if(!matches(id)||!state_.prepared||!state_.seeking||serial!=state_.seekSerial)return {};
    auto actions=accepted();state_.seeking=false;state_.positionMs=positionMs;evaluate(actions);return actions;
}
Actions Opening::eof(uint64_t id,uint64_t acknowledgedSeekSerial){
    if(!matches(id)||!state_.prepared||state_.seeking||acknowledgedSeekSerial!=state_.seekSerial)return {};
    auto actions=accepted();ended_=true;evaluate(actions);return actions;
}
Actions Opening::eof(uint64_t id,uint64_t acknowledgedSeekSerial,uint64_t terminalPositionMs){
    if(!matches(id)||!state_.prepared||state_.seeking||acknowledgedSeekSerial!=state_.seekSerial)return {};
    if(terminalPositionMs>state_.positionMs)state_.positionMs=terminalPositionMs;
    // Equivalent to the final playing-position notification before EOF. No
    // audio command runs while paused; only the already-entered193s wait ends.
    if(stage_==Stage::TailBoundary&&state_.positionMs>=SkipToMs)stage_=Stage::Eof;
    return eof(id,acknowledgedSeekSerial);
}
Actions Opening::skip(uint64_t id){
    if(!matches(id))return {};
    auto actions=accepted();skip_=true;evaluate(actions);return actions;
}
Actions Opening::committed(uint64_t id){
    if(!matches(id))return {};
    if(stage_!=Stage::CueCommit&&stage_!=Stage::FadeCommit)return {};
    auto actions=accepted();stage_=stage_==Stage::CueCommit?Stage::TailBoundary:Stage::Eof;evaluate(actions);return actions;
}
Actions Opening::setPaused(bool paused){
    if(state_.paused==paused)return {};
    auto actions=accepted();state_.paused=paused;evaluate(actions);return actions;
}
Actions Opening::stop(){
    if(!active())return {};
    auto actions=accepted();actions.canceled=true;actions.stopVideo=true;actions.stopVideoId=state_.id;
    state_.status=Status::Stopped;state_.waiting=false;state_.seeking=false;skip_=false;ended_=false;return actions;
}
Actions Opening::fail(uint64_t id){
    if(!matches(id))return {};
    auto actions=accepted();actions.failed=true;actions.stopVideo=true;actions.stopVideoId=id;
    state_.status=Status::Failed;state_.waiting=false;state_.seeking=false;skip_=false;ended_=false;return actions;
}
void Opening::finish(Actions& actions){
    actions.completed=true;actions.stopVideo=true;actions.stopVideoId=state_.id;
    state_.status=Status::Finished;state_.waiting=false;state_.seeking=false;
}
void Opening::evaluate(Actions& actions){
    if(!active()||!state_.prepared||!state_.waiting||state_.paused)return;
    //156250 emits the cue before entering the193s branch, even on early skip.
    if(stage_==Stage::Cue){
        if(!ended_&&!skip_&&state_.positionMs<CueAtMs)return;
        stage_=Stage::CueCommit;actions.playSE=true;return;
    }
    if(stage_==Stage::TailBoundary){
        if(ended_){stage_=Stage::FadeCommit;actions.fadeBgm=true;return;}
        if(state_.positionMs>=SkipToMs)stage_=Stage::Eof;
        else if(skip_){
            state_.seeking=true;state_.seekToMs=SkipToMs;++state_.seekSerial;
            stage_=Stage::FadeCommit;actions.needSeek=true;actions.seekToMs=SkipToMs;actions.seekSerial=state_.seekSerial;actions.fadeBgm=true;return;
        }else return;
    }
    if(stage_==Stage::Eof&&ended_&&!state_.seeking)finish(actions);
}
} // namespace mahoyo::movie
