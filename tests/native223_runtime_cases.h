// Exact Native223: no operands or side effects. Synthetic assets and private saves only.
static void native223EmptyAndRepeated(){
    for(int repeats:{1,7}){
        Program p;for(int i=0;i<repeats;++i)p.op(0x223);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");
        check(waiting(state,"input")&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"223 accepts an empty operand stack and repeated calls add no wait or idle timer");
        check(state.find("\"transition\":null")!=std::string::npos&&visibleLayers(state)=="[]","223 does not invent a visible object or prepare barrier");
        f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");
        check(currentHostSchema(saved),"223 requires no schema change from WH18");
        InspectHost host;mahoyo::vm::Machine machine(host);machine.loadState(saved.vm);
        check(machine.stack().empty(),"223 returns Continue without pushing native return zero");f.passed=true;
    }
}

static void native223ActualStackAndSentinels(){
    const std::vector<std::vector<int32_t>> inputs{
        {1,1,0,0,0,0,0,1,1,0,1,1},
        {314159,-17,INT32_MIN,INT32_MAX,0,12345,-9876}
    };
    for(const auto& values:inputs){
        Program p;for(auto value:values)p.push(value);for(int i=0;i<3;++i)p.op(0x223);p.dialogue(0);
        for(int i=0;i<2;++i)p.op(0x223);for(size_t i=0;i<values.size();++i){p.push(int(i));p.op(0xe0);}p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");
        check(waiting(state,"input")&&number(state,"nextTickMs")==0,"actual223 stack reaches the next paragraph without a synthetic yield");
        f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");
        InspectHost host;mahoyo::vm::Machine before(host);before.loadState(saved.vm);
        check(before.stack()==values,"all actual twelve operands or distinct signed sentinels remain in their original order");
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");
        check(waiting(state,"input")&&number(state,"nextTickMs")==0,"fresh WH18 Runtime restores the untouched223 operand stack at the same input wait");
        state=f.command("advance");check(waiting(state,"input"),"223 remains immediate after a fresh process-style restore");
        f.command("save","2");auto resumed=mahoyo::save::read(f.root/"savedata/slot2.whs");
        mahoyo::vm::Machine after(host);after.loadState(resumed.vm);
        check(after.stack().empty(),"the following VM stores consume exactly the preserved stack, with no hidden223 result");
        for(size_t i=0;i<values.size();++i)check(after.variable(i)==values[values.size()-1-i],"following VM consumers see the original signed values in LIFO order");
        f.passed=true;
    }
}

static std::string native223Audio(const std::string& state){auto first=state.find("\"audio\":");check(first!=std::string::npos,"223 fixture exposes audio state");first+=8;return state.substr(first,state.find(",\"history\":",first)-first);}

static Program native223StateProgram(bool with223){
    Program p;p.image();gfFilter(p,0,13,0x123456,58,20);effectRequest(p,0,915);p.sound190(5,0,"fake",70,13);
    if(with223)p.op(0x223);p.dialogue(0);
    gfFilter(p,0,7,0x0a0b0c,-12,-35);effectRequest(p,0,915);p.preload(99,"fake");
    if(with223){p.op(0x223);p.op(0x223);}p.commit();p.dialogue(1);
    if(with223)p.op(0x223);p.clone(1,0,"fake");if(with223)p.op(0x223);p.dialogue(0);p.op(0xf4);return p;
}

static void native223PreservesPendingLayersAudioAndRestore(){
    const std::string table="915 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n915 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n";
    const std::string windows="915 0 0 0x0 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x0 -32767 -32767 -32767 -32767 -32767 -32767\n";
    Fixture control(native223StateProgram(false),true,table,"fake 100 100\n","",windows);
    Fixture candidate(native223StateProgram(true),true,table,"fake 100 100\n","",windows);
    control.command("textSpeed","0");candidate.command("textSpeed","0");
    auto a=control.command("action","start"),b=candidate.command("action","start");
    check(audioChannel(a,"se5").find("fake.hw")!=std::string::npos,"223 preservation fixture starts a real synthetic SE channel");
    check(waiting(a,"input")&&waiting(b,"input")&&visibleLayers(a)==visibleLayers(b)&&native223Audio(a)==native223Audio(b),"223 preserves the initialized image, filters, active effect and audio");
    check(number(b,"instructions")==number(a,"instructions")+1,"the candidate actually executes its additional223 command");
    a=control.command("advance");b=candidate.command("advance");
    check(waiting(a,"transitionReady")&&waiting(b,"transitionReady"),"only the explicit200 consumes the pending batch;223 does not prepare it early");
    check(visibleLayers(a)==visibleLayers(b)&&preloads(a)==preloads(b)&&native223Audio(a)==native223Audio(b),"repeated223 preserves pending filter/effect/preload updates and the unrelated audio channel");
    a=acknowledge(control,a);b=acknowledge(candidate,b);
    if(waiting(a,"frame")){a=control.command("tick","1000");b=candidate.command("tick","1000");}
    check(waiting(a,"input")&&waiting(b,"input")&&visibleLayers(a)==visibleLayers(b),"the common explicit prepare has identical visible results with or without223");
    control.command("save","2");candidate.command("save","2");
    auto savedA=mahoyo::save::read(control.root/"savedata/slot2.whs"),savedB=mahoyo::save::read(candidate.root/"savedata/slot2.whs");
    check(currentHostSchema(savedA)&&currentHostSchema(savedB)&&savedA.hostExtra==savedB.hostExtra,"223 adds no WH18 fields and does not modify the serialized host/effect state");
    const auto before=visibleLayers(b);woh_destroy(control.handle);control.handle=woh_create(control.game.string().c_str());control.command("textSpeed","0");a=control.command("load","2");
    woh_destroy(candidate.handle);candidate.handle=woh_create(candidate.game.string().c_str());candidate.command("textSpeed","0");b=candidate.command("load","2");
    check(visibleLayers(b)==before&&visibleLayers(a)==visibleLayers(b)&&native223Audio(b)==native223Audio(a),"fresh WH18 load keeps the layers and follows the same existing audio restore policy with or without223");
    a=control.command("advance");b=candidate.command("advance");
    if(waiting(a,"transitionReady")){a=acknowledge(control,a);b=acknowledge(candidate,b);}
    check(waiting(a,"input")&&waiting(b,"input")&&visibleLayers(a)==visibleLayers(b),"223 does not clear the saved effect base or change the subsequent terminal clone");
    control.passed=true;candidate.passed=true;
}
