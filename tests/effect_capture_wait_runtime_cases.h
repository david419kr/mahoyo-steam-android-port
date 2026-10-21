//Synthetic CHS/metadata only. Native ordering proof is documented separately.
static const std::string captureWaitEffects=
    "205 0 0 0 64 -32767 -32767 -100001 0 0 0 1000\n"
    "205 1000 50 50 0 -32767 -32767 -100001 0 0 0 1000\n";
static const std::string captureWaitWindows=
    "205 0 692 0x4144080 1 2 -32767 -32767 -32767 -32767\n"
    "205 1 693 0x4144080 -32767 -32767 -32767 -32767 -32767 -32767\n";
static void captureThenEffect(Program& p,int id=15){p.push(205);p.push(id);p.op(0x279);for(int n:{0,0,-1,205,id})p.push(n);p.op(0x278);}
static void capturedEffectPreparesBeforeFirstWait(){
    Program p;p.image(1000,"fake",15,{{870,0,889,1080,1,75}});p.image(1000,"fake",7);p.dialogue(0);
    captureThenEffect(p);wait110(p,2800,0);
    p.layerShake(7,5,5,200,600,400,2);p.globalShake(12,12,200,4,0);wait110(p,200,0);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,captureWaitEffects,"","",captureWaitWindows);f.command("textSpeed","0");
    auto state=f.command("action","start");check(waiting(state,"input"),"capture regression begins at a stable existing picture");
    state=f.command("advance");check(waiting(state,"transitionReady")&&!boolean(state,"canSave"),"279 alone makes the first110 prepare before its2800ms wait");
    const auto capturedLayer=layerObject(state,15);const auto instructions=number(state,"instructions");
    check(number(capturedLayer,"alpha")>.98&&number(capturedLayer,"width")==889,"first prepare captures old live alpha255/window rather than new278 alpha64");
    state=f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"first prepare cannot save pending scene work");
    state=f.command("tick","999999");check(waiting(state,"transitionReady")&&number(state,"instructions")==instructions&&layerObject(state,15)==capturedLayer,"late graphics preparation cannot consume wait/effect time or recapture old live state");
    state=f.command("transitionReady","99999");check(waiting(state,"transitionReady"),"stale prepare acknowledgement cannot start capture wait");
    state=acknowledge(f,state);check(waiting(state,"frame"),"valid first acknowledgement starts the native relative2800ms wait");
    f.command("tick","1000");state=f.command("tick","3783");check(waiting(state,"frame"),"first110 still waits at2799ms after graphics acknowledgement");
    state=f.command("tick","3784");check(waiting(state,"transitionReady"),"first110 completes at2800ms and following270/340 prepares without stale capture failure");
    state=acknowledge(f,state);f.command("tick","4000");state=f.command("tick","4183");check(waiting(state,"frame"),"second110 keeps its independent200ms deadline");
    state=f.command("tick","4184");check(waiting(state,"input"),"279/278/1102800/270/340/110200 reaches the next dialogue");
    state=f.pulseTick("5184");check(boolean(state,"canSave"),"consumed capture leaves no persistent savebusy after finite shake jobs finish");
    f.command("save","2");const auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");
    const std::string extra(saved.hostExtra.begin(),saved.hostExtra.end());
    check(extra.find("EFFECTFRAMES1 1\n692 50 50 255 870 0 889 1080\n")!=std::string::npos,"both waits retain exactly the old-live captured raw XY/alpha/window override");
    check(currentHostSchema(saved)&&extra.find("WAITBASELINE1 ")!=std::string::npos&&extra.find("EFFECTBASES1 ")!=std::string::npos,"capture correction preserves WH18 baseline and D0 footers");
    const auto before=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());
    state=f.command("load","2");check(visibleLayers(state)==before&&boolean(state,"canSave"),"fresh process restores captured keys with no queued capture or savebusy");f.passed=true;
}
static void capturedEffectZeroAndShortWaitPrepare(){
    for(int duration:{0,200}){
        Program p;p.image(1000,"fake",15,{{870,0,889,1080,1,75}});captureThenEffect(p);wait110(p,duration,0);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true,captureWaitEffects,"","",captureWaitWindows);f.command("textSpeed","0");
        auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(layerObject(state,15),"alpha")>.98,"even zero-duration110 commits capture before yielding to preparation");
        state=acknowledge(f,state);
        if(duration){f.command("tick","1000");state=f.command("tick","1184");}
        check(waiting(state,"input")&&boolean(state,"canSave"),"zero/short110 consumes its capture and reaches a saveable dialogue");
        f.command("save","2");const auto doc=mahoyo::save::read(f.root/"savedata/slot2.whs");const std::string extra(doc.hostExtra.begin(),doc.hostExtra.end());
        check(extra.find("EFFECTFRAMES1 1\n692 50 50 255 870 0 889 1080\n")!=std::string::npos,"zero/short prepare uses the old live picture rather than the provisional new effect");f.passed=true;
    }
}
static void capturedEffectUnknownWaitRemainsStrict(){
    //111 is intentionally outside the newly proved normal110 prepare path.
    Program p;p.image(1000,"fake",15);p.dialogue(0);captureThenEffect(p);p.push(0);p.push(10);p.op(0x111);p.commit();p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,captureWaitEffects,"","",captureWaitWindows);f.command("textSpeed","0");f.command("action","start");
    auto state=f.command("advance");check(waiting(state,"frame")&&!boolean(state,"canSave"),"unreviewed111 cannot make a queued capture saveable");
    state=woh_command(f.handle,"tick","1000");
    check(waiting(state,"error")&&state.find("Unsupported effect capture across an asynchronous wait")!=std::string::npos,"actual tick-crossing queued capture is still rejected instead of guessed or silently discarded");
    check(!fs::exists(f.root/"savedata/slot2.whs"),"strict stale-capture failure creates no checkpoint");f.passed=true;
}
