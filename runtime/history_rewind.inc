// On-demand replay is isolated from user saves and shared progress. Nothing is
// adopted until both the existing execution point and the requested row match.
    bool historySuffixMatches(const Runtime& candidate,size_t end) const {
        if(end>history.size()||candidate.history.size()<end||historySources.size()!=history.size())return false;
        const size_t offset=candidate.history.size()-end;
        for(size_t i=0;i<end;++i){
            const auto& source=historySources[i];const auto& replay=candidate.historySources[offset+i];
            if(!source.raw.empty()){if(source.raw!=replay.raw)return false;}
            else if(history[i]!=candidate.history[offset+i])return false;
            const auto& a=historyMeta.rows[i];const auto& b=candidate.historyMeta.rows[offset+i];
            if(a.known){
                if(a.startsGroup!=b.startsGroup||a.endsGroup!=b.endsGroup||a.voices.size()!=b.voices.size())return false;
                for(size_t j=0;j<a.voices.size();++j){const auto& av=a.voices[j];const auto& bv=b.voices[j];if(av.active!=bv.active||av.backend!=bv.backend||lower(av.name)!=lower(bv.name)||av.variant!=bv.variant)return false;}
            }
        }
        return true;
    }
    struct HistoryReplayDriver {
        Runtime& value;
        uint64_t platform=0;
        std::map<uint64_t,uint64_t> audioEnds;
        explicit HistoryReplayDriver(Runtime& runtime):value(runtime){}
        void advanceTime(uint64_t delta){
            delta=std::max<uint64_t>(1,std::min<uint64_t>(delta,1000));
            value.havePlatformTime=true;value.platformLast=platform;platform+=delta;
            value.sampleWaveClock(platform);value.command("tick",std::to_string(platform));
        }
        void step(bool holdInput=false){
            auto& r=value;
            if(!r.error.empty()||r.wait=="error")throw mahoyo::vm::Error("replay-runtime-error");
            if(r.choices.active())throw mahoyo::vm::Error("replay-needs-unrecorded-choice");
            // Maintain only live one-shot channels. No Android decoder is started.
            std::map<std::string,Audio> current;for(const auto& a:r.audio)current.insert_or_assign(a.channel,a);
            std::set<uint64_t> live;uint64_t nearest=UINT64_MAX;
            for(const auto& pair:current){const auto& a=pair.second;if(a.name.empty()||a.loop)continue;live.insert(a.id);
                auto due=audioEnds.find(a.id);
                if(due==audioEnds.end()){
                    auto metadata=r.audioLoops.find(a.name);
                    if(metadata==r.audioLoops.end()||!metadata->second.sampleRate)throw mahoyo::vm::Error("replay-audio-metadata-missing");
                    auto ms=(uint64_t(metadata->second.sampleCount)*1000+metadata->second.sampleRate-1)/metadata->second.sampleRate;
                    due=audioEnds.emplace(a.id,r.tick+std::max<uint64_t>(1,ms)).first;
                }
                if(due->second<=r.tick){r.command("audioEnded",a.channel+" "+std::to_string(a.id));audioEnds.erase(a.id);return;}
                nearest=std::min(nearest,due->second-r.tick);
            }
            for(auto i=audioEnds.begin();i!=audioEnds.end();)if(!live.count(i->first))i=audioEnds.erase(i);else ++i;
            if(r.particleOverlays.pending()){
                auto frame=r.particleOverlays.frame();uint64_t fall=frame.fallNote.pendingResource?frame.fallNote.revision:0,focus=frame.focusLine.pendingResource?frame.focusLine.revision:0;
                if(fall||focus){r.command("particlesReady",std::to_string(r.version)+" "+std::to_string(fall)+" "+std::to_string(focus));return;}
            }
            if(r.inputRegion.readyPending()){
                r.command("inputUiReady",std::to_string(r.inputRegion.generation())+" "+std::to_string(r.inputRegion.request())+" "+std::to_string(r.version)+" 1920 1080");return;
            }
            if(r.wait=="transitionReady"){
                r.command("transitionReady",r.alphaRules.pending()?r.alphaRules.ack(r.transition.id,r.version):std::to_string(r.transition.id));return;
            }
            if(r.opening.active()&&(r.wait=="movieReady"||r.wait=="movie")){
                auto movie=r.opening.state();auto id=std::to_string(movie.id);
                if(!movie.prepared){r.command("moviePrepared",id+" "+std::to_string(mahoyo::movie::Opening::SkipToMs+10000));return;}
                if(movie.seeking){r.command("movieSeeked",id+" "+std::to_string(movie.seekToMs)+" "+std::to_string(movie.durationMs)+" "+std::to_string(movie.seekSerial));return;}
                r.command("movieSkip",id);movie=r.opening.state();
                if(movie.seeking)return;
                r.command("movieEnded",id+" "+std::to_string(movie.durationMs)+" "+std::to_string(movie.durationMs)+" "+std::to_string(movie.seekSerial));return;
            }
            if(r.intermission.active()){
                const auto modal=*r.intermission.state();
                if(modal.phase==mahoyo::teatime::Phase::Ready)r.command("modalAction",std::to_string(modal.id)+" "+std::to_string(modal.revision)+" next");
                else advanceTime(1000);
                return;
            }
            if(r.mode!="play"||r.wait=="end"||r.wait=="exit")throw mahoyo::vm::Error("replay-left-story");
            if(r.wait=="input"){
                if(!r.voicePlaying()&&!holdInput){r.command("advance","");return;}
                if(holdInput){advanceTime(std::min<uint64_t>(16,nearest));return;}
                advanceTime(nearest);return;
            }
            if(r.wait=="voice"){advanceTime(r.voicePlaying()?nearest:16);return;}
            uint64_t delta=r.deadline>r.tick?r.deadline-r.tick:16;
            if(r.wait=="transition"&&(r.transition.nativeMode==0||r.transition.nativeMode==2))delta=std::max(1,r.transition.nativeStepMs);
            advanceTime(std::min(delta,nearest));
        }
    };
    bool rewindReady() const {
        // Preserve live/pending jobs in the reconstructed Runtime. Active alpha-rule
        // captures still depend on GPU pixels from the former rendering timeline.
        return alphaRules.empty();
    }
    std::unique_ptr<Runtime> rewindHistory(const std::string& payload) {
        std::istringstream request(payload);std::string revision,extra;uint64_t start,count;
        if(mode!="history"||!machine||!(request>>revision>>start>>count)||request>>extra||revision!=std::to_string(historyRevision)||!count||start>=history.size()||count>history.size()-start)
            throw mahoyo::vm::Error("rewind-stale-selection");
        const size_t targetEnd=size_t(start+count);
        if(!historyGroupStart(size_t(start))||(targetEnd<history.size()&&!historyGroupStart(targetEnd))||historySources.size()!=history.size())throw mahoyo::vm::Error("rewind-invalid-group");
        for(size_t i=size_t(start);i<targetEnd;++i)if(!historyMeta.rows[i].known)throw mahoyo::vm::Error("rewind-legacy-metadata-missing");
        const auto vars=machine->sharedVariables();const auto flags=machine->sharedFlags();
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(45);
        const uint64_t instructionLimit=std::min<uint64_t>(100000000,std::max<uint64_t>(1000000,machine->instructionsExecuted()*2+200000));
        auto create=[&](){
            auto path=root.u8string();auto next=std::make_unique<Runtime>(path.c_str(),languageAssets.edition().c_str(),languageAssets.current().c_str(),true);
            if(!next->error.empty())throw mahoyo::vm::Error("replay-metadata-unavailable");
            next->sharedState=sharedState;next->sharedState.variables=vars;next->sharedState.flags=flags;next->sharedLoaded=true;next->sharedWritePending=false;
            next->command("textSpeed","0");
            if(archiveReplay){next->openArchive(archiveReplay,archiveCheckpoint);next->startArchive(archiveReplay);}else next->play();
            return next;
        };
        uint64_t absoluteEnd=0;
        for(int pass=0;pass<2;++pass){
            auto next=create();HistoryReplayDriver driver(*next);bool found=false;uint64_t settleStart=UINT64_MAX;
            for(size_t steps=0;steps<250000;++steps){
                if(std::chrono::steady_clock::now()>until)throw mahoyo::vm::Error("replay-time-budget");
                if(!next->machine||next->machine->instructionsExecuted()>instructionLimit||!next->error.empty())throw mahoyo::vm::Error("replay-execution-diverged");
                if(pass==0){
                    if(next->historyRowsAppended>=history.size()&&machine->sameExecutionPoint(*next->machine)&&historySuffixMatches(*next,history.size())){
                        absoluteEnd=next->historyRowsAppended-history.size()+targetEnd;found=true;break;
                    }
                }else if(next->historyRowsAppended==absoluteEnd&&next->wait!="transitionReady"&&!next->inputRegion.readyPending()&&historySuffixMatches(*next,targetEnd)){
                    if(next->wait!="input")throw mahoyo::vm::Error("replay-target-not-input");
                    if(!next->rewindReady()){
                        if(settleStart==UINT64_MAX)settleStart=next->tick;
                        if(next->tick-settleStart>10000)throw mahoyo::vm::Error("replay-target-needs-render-capture");
                        driver.step(true);next->snapshot();continue;
                    }
                    next->machine->restoreShared(vars,flags);next->sharedState=sharedState;next->sharedState.variables=vars;next->sharedState.flags=flags;next->sharedLoaded=true;next->sharedWritePending=false;
                    next->seenImages=seenImages;next->trophies=trophies;next->trophiesDirty=trophiesDirty;next->pendingFrontendFlags=pendingFrontendFlags;
                    next->activeSlot=activeSlot;next->autoDelay=autoDelay;next->reveal.speed(reveal.speed(),next->tick);next->reveal.finish();next->hidden=false;next->paused=paused;
                    next->audioId=std::max(audioId,next->audioId);for(auto& a:next->audio)a.id=++next->audioId;
                    next->version=version+1;next->transitionId=std::max(transitionId,next->transitionId)+1;next->transition.active=false;
                    next->resetHistorySession();next->historyChanged();next->notice.clear();return next;
                }else if(pass==1&&next->historyRowsAppended>absoluteEnd)throw mahoyo::vm::Error("replay-target-not-stable");
                driver.step();next->snapshot();
            }
            if(pass==0&&!found)throw mahoyo::vm::Error("replay-current-point-not-found");
        }
        throw mahoyo::vm::Error("replay-target-not-found");
    }
