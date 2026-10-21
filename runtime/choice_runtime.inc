//Runtime adapter for the reviewed Native choice profiles; no save footer.
    const std::vector<mahoyo::choices::Layout>& choiceLayout(){
        if(!wikChoiceLayout){auto bytes=readBytes(root/"meta"/"wik-choice-layout.tsv");auto rows=mahoyo::choices::parseWikLayout(std::string(bytes.begin(),bytes.end()));
            for(const auto& row:rows)for(const auto& name:{row.mono,row.color}){auto size=imageSize(name);if(size[0]!=row.sourceWidth||size[1]!=row.sourceHeight)throw mahoyo::vm::Error("WIK choice prepared dimensions mismatch: "+name);}
            wikChoiceLayout=std::move(rows);
        }return *wikChoiceLayout;
    }
    mahoyo::choices::Request choiceRequest(Machine& vm){
        if(vm.stack().size()<4)throw mahoyo::vm::Error("Choice operand underflow");
        mahoyo::choices::Request request{vm.peek(0),vm.peek(1),vm.peek(2),vm.peek(3)};mahoyo::choices::Controller::validateRequest(request);
        if(choices.active())throw mahoyo::vm::Error("Unsupported pending choice collision");
        vm.variable(0); //Strict observed destination[2,0] resolves to local0.
        return request;
    }
    Flow beginChoice(Machine& vm,bool wik){
        const auto request=choiceRequest(vm);
        if(wik){const auto& rows=choiceLayout();choices.beginWik(request,rows);}
        else{std::vector<std::string> labels;labels.reserve(choices.queued().size());for(const auto& item:choices.queued())labels.push_back(mahoyo::text::stripDisplayControls(mahoyo::text::resolveReferences(item.raw,table).text));choices.beginText(request,labels);}
        for(int n=0;n<4;++n)vm.pop();
        //155820/15CD20 both run the160410 common scene-ready adapter.
        //164 adds152(0)-style pendinghide; mode/auto-show guard is preserved.
        if(wik)windowFade.requestHide(0);
        commitEffectCaptures();prepareNoise();
        transition=Transition{++transitionId,tick,0,true,"[]","[]",1};transition.waitAfterReady="choice";transition.deadlineAfterReady=tick;wait="transitionReady";
        return Flow::Yield;
    }
    void selectChoice(const std::string& payload){
        std::istringstream in(payload);std::string revision,ordinalText,extra;
        if(!(in>>revision>>ordinalText)||(in>>extra)||ordinalText.empty())return;
        uint64_t ordinal=0;for(char c:ordinalText){if(c<'0'||c>'9'||ordinal>uint64_t(INT32_MAX)/10)return;ordinal=ordinal*10+uint64_t(c-'0');if(ordinal>INT32_MAX)return;}
        auto selected=choices.select(revision,int(ordinal),machine&&mode=="play"&&wait=="choice"&&!paused&&error.empty()&&!opening.active());if(!selected)return;
        //Only the observed[2,0] destination is admitted. A successful write
        //precedes Native normalcleanup; a later VM error does not undo it.
        machine->setVariable(size_t(selected->index),selected->value,false);choices.complete(selected->revision);commitWindow();resume();
    }
    std::string choiceJson()const{
        if(mode!="play"||!choices.state()||!choices.state()->ready)return "null";const auto& state=*choices.state();bool wik=state.profile==mahoyo::choices::Profile::Wik164;
        std::ostringstream out;out<<"{\"revision\":"<<quote(std::to_string(state.revision))<<",\"profile\":"<<quote(wik?"wik164":"text161")<<",\"initialFocus\":-1";
        if(wik)out<<",\"coordinateUnits\":\"parent-canvas-pixels\",\"canvasWidth\":1920,\"canvasHeight\":1080,\"monoState\":0,\"colorState\":1";
        out<<",\"items\":[";bool first=true;for(const auto& item:state.options){if(!first)out<<',';first=false;out<<"{\"ordinal\":"<<item.ordinal<<",\"label\":"<<quote(item.label)<<",\"enabled\":"<<(item.enabled?"true":"false");
            if(item.layout){const auto& l=*item.layout;out<<",\"image\":"<<quote(l.mono)<<",\"colorImage\":"<<quote(l.color)<<",\"x\":"<<l.x<<",\"y\":"<<l.y<<",\"width\":"<<l.width<<",\"height\":"<<l.height<<",\"priority\":"<<l.priority<<",\"hitMode\":"<<l.hitMode<<",\"anchorX\":"<<l.anchorX<<",\"anchorY\":"<<l.anchorY<<",\"sourceWidth\":"<<l.sourceWidth<<",\"sourceHeight\":"<<l.sourceHeight;}
            out<<'}';
        }return out.str()+"]}";
    }
