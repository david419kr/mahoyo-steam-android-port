    std::string inputRegionJson()const {
        const auto& r=inputRegion.stable();std::ostringstream out;
        out<<"{\"active\":"<<(r.active?"true":"false")<<",\"generation\":"<<quote(std::to_string(inputRegion.generation()))
           <<",\"request\":"<<quote(std::to_string(inputRegion.request()))<<",\"readyPending\":"<<(inputRegion.readyPending()?"true":"false")
           <<",\"trigger\":"<<quote(std::to_string(inputRegion.trigger()))<<",\"epoch\":"<<quote(std::to_string(inputRegion.epoch()))
           <<",\"enabled\":"<<(r.currentEnabled?"true":"false")<<",\"elapsedMs\":"<<inputRegion.elapsed()<<",\"progressLeft\":"<<inputRegion.progressLeft()
           <<",\"jumpRelease\":"<<quote(std::to_string(inputJumpRelease))<<",\"policy\":\"android-transient-close\"}";return out.str();
    }
    void finishInputJump(){
        if(!inputRegion.jumpQueued()||paused||!machine)return;
        machine->validateExternalTarget(inputRegion.jumpTarget());
        if(wait=="transitionReady")return; //The actual prepare/render acknowledgement still owns pending scene work.
        if(opening.active()){
            opening.stop();inputJumpRelease=mahoyo::input_region::identity();wait="inputJumpRelease";havePlatformTime=false;return;
        }
        if(inputJumpRelease)return; //Wait for the real Android player release and rendered movie removal.
        if(choices.active()||intermission.active()||mode!="play"||
           (wait!="input"&&wait!="frame"&&wait!="animation"&&wait!="budget"&&wait!="transition"&&wait!="movie"&&wait!="movieReady"&&wait!="inputJumpRelease"&& !wait.empty()))
            throw mahoyo::vm::Error("Unreviewed input typeE blocking-job cancellation context");
        //Cancel only the VM blocking wait. Scene/effect/audio state, operand
        //stack, local scopes and call frames are retained by externalJump.
        inputJumpPreviousPc=machine->externalJump(inputRegion.jumpTarget());
        inputRegion.jumped();wait.clear();transition.active=false;pulseWaitLayer.reset();waitingScreenWave=false;
        automatic=skipping=false;havePlatformTime=false;pump();
    }
    void inputCommand(const std::string& type,const std::string& payload){
        std::istringstream row(payload);uint64_t gen,id,epoch;std::string extra;
        if(type=="inputUiReady"||type=="inputJumpReleased"){
            uint64_t rendered;int w,h;
            if(!(row>>gen>>id>>rendered>>w>>h)||row>>extra||w<=0||h<=0||w>32768||h>32768)throw mahoyo::vm::Error("Invalid input UI acknowledgement");
            if(paused||rendered!=version||gen!=inputRegion.generation())return;
            if(type=="inputJumpReleased"){
                if(!inputJumpRelease||id!=inputJumpRelease||!inputRegion.jumpQueued())return;
                inputJumpRelease=0;finishInputJump();return;
            }
            int kind=inputRegion.readyKind();
            if(!inputRegion.ready(gen,id))return;
            if(kind==1&&wait=="inputRegionReady")resume();
            else if(kind==2)inputRegion.commit();
        }else{
            if(!(row>>gen>>id>>epoch))throw mahoyo::vm::Error("Invalid input pointer identity");
            if(type=="inputCancel"){
                if(row>>extra)throw mahoyo::vm::Error("Invalid input cancellation");
                if(inputRegion.matches(gen,id,epoch))inputRegion.physicalReset();return;
            }
            int down,inside;if(!(row>>down>>inside)||row>>extra||(down!=0&&down!=1)||(inside!=0&&inside!=1))throw mahoyo::vm::Error("Invalid input pointer state");
            if(!paused&&mode=="play"&&error.empty())inputRegion.pointer(gen,id,epoch,down,inside,tick);
        }
    }
