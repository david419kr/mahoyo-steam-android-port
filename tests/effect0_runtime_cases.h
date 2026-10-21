// Synthetic counterpart to the pinned original278 cancellation proof.
static void effectRequest(Program& p,int layer,int clip){for(int n:{0,0,0,clip,layer})p.push(n);p.op(0x278);}
static const std::string effectZeroScaleTable=
    "1 0 -32767 -32767 -32767 1000 -32767 -100001 0 0 1 1000\n"
    "1 1000 -32767 -32767 -32767 2000 -32767 -100001 0 0 1 1000\n"
    "2 0 -32767 -32767 -32767 1000 -32767 -100001 0 0 1 1000\n"
    "2 1000 -32767 -32767 -32767 3000 -32767 -100001 0 0 1 1000\n";

static void nativeEffectZeroPrepareAndCurrentValue(){
    Program p;p.image(1000,"fake",26);effectRequest(p,26,1);p.dialogue(0);
    effectRequest(p,26,0);p.push(2);p.op(0x10e);
    for(int n:{0,1,100})p.push(n);p.op(0x110);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,effectZeroScaleTable);f.command("textSpeed","0");
    auto state=f.command("action","start");f.command("tick","1000");state=f.command("tick","1484");
    check(number(layerObject(state,26),"w")==150,"active effect reaches an intermediate width before zero request");
    state=f.command("advance");check(waiting(state,"input")&&!boolean(state,"canSave"),"278zero consumes no yield but unprepared cancellation cannot be saved");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"pending effect cancellation is never flattened into a slot");
    state=f.command("tick","1734");const auto frozen=number(layerObject(state,26),"w");
    check(frozen==175,"old job keeps running until an actual prepared-scene boundary");
    state=f.command("advance");check(waiting(state,"transitionReady")&&number(layerObject(state,26),"w")==frozen,"normal110 prepares cancellation without applying its terminal key");
    state=f.command("tick","9000");check(number(layerObject(state,26),"w")==frozen,"graphics preparation cannot advance cancelled effect");
    state=acknowledge(f,state);f.command("tick","9000");state=f.command("tick","9084");
    check(waiting(state,"input")&&number(layerObject(state,26),"w")==frozen&&number(state,"nextTickMs")==0,"prepared stop freezes current width and releases idle scheduling");
    f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");check(currentHostSchema(document),"effect cancellation requires no WH17 footer change");
    const auto before=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");
    check(visibleLayers(state)==before&&number(state,"nextTickMs")==0,"fresh WH17 load retains frozen fields without reviving the cancelled loop");f.passed=true;
}

static void nativeEffectZeroKeepsIndependentJobs(){
    Program p;p.image(1000,"fake",26);p.image(1000,"fake",27);effectRequest(p,26,1);effectRequest(p,27,1);p.dialogue(0);
    effectRequest(p,26,0);for(int n:{0,400,128,INT32_MIN,26})p.push(n);p.op(0x245);
    p.position(26,80,60,400);p.sound190(5,0,"fake",70,13);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,effectZeroScaleTable);f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");auto state=f.command("tick","1484");
    state=f.command("advance");const auto start=layerObject(state,26);check(number(start,"w")==150,"140 also commits effect-only cancellation at current width");
    const auto sound=audioChannel(state,"se5");state=f.command("tick","1684");auto changed=layerObject(state,26);
    check(number(changed,"w")==150&&number(layerObject(state,27),"w")==170,"cancel removes only selected layer effect while the other loop advances");
    check(number(changed,"alpha")<number(start,"alpha")&&number(changed,"x")!=number(start,"x"),"independent same-layer alpha and movement jobs continue after zero");
    check(audioChannel(state,"se5")==sound&&!boolean(state,"canSave"),"audio survives and independent finite jobs retain their save guard");
    state=f.command("tick","1884");check(number(layerObject(state,26),"w")==150&&std::abs(number(layerObject(state,26),"alpha")-128./255)<1e-8&&boolean(state,"canSave"),"only independent400ms endpoints settle; stopped effect never overwrites them");
    const auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");
    check(visibleLayers(state)==before,"WH17 preserves both stopped graphics and unrelated active effect");f.passed=true;
}

static void nativeEffectZeroBoundsAndLatestRequest(){
    for(int layer:{-1,64,INT32_MAX})for(int clip:{0,-1,8373}){
        Program p;p.image(1000,"fake",9);p.push(777);effectRequest(p,layer,clip);p.push(9);p.op(0x251);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");
        check(number(layerObject(state,9),"z")==777&&number(state,"nextTickMs")==0&&boolean(state,"canSave"),"unsigned invalid layer consumes exactly five operands before checking clip resources");f.passed=true;
    }
    for(int clip:{0,-1,INT32_MIN}){
        Program p;effectRequest(p,26,clip);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");
        check(visibleLayers(state)=="[]"&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"nonpositive clip on an empty slot creates neither phantom graphics nor effect0 metadata dependency");f.passed=true;
    }
    Program p;p.image(1000,"fake",26);effectRequest(p,26,1);p.dialogue(0);effectRequest(p,26,0);effectRequest(p,26,2);p.dialogue(1);effectRequest(p,26,0);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true,effectZeroScaleTable);f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");f.command("tick","1484");
    auto state=f.command("advance");check(number(layerObject(state,26),"w")==100&&boolean(state,"canSave"),"positive278 after zero replaces the pending clear rather than losing its new job");
    state=f.command("tick","1684");check(number(layerObject(state,26),"w")==140,"last positive request advances on its own baseline and timeline");
    state=f.command("advance");state=f.command("tick","9000");check(number(layerObject(state,26),"w")==140&&number(state,"nextTickMs")==0,"a later zero cancels the replacement without terminal sampling");f.passed=true;
    Program rejected;rejected.image();effectRequest(rejected,0,8373);rejected.dialogue(0);Fixture invalid(std::move(rejected),true);
    state=woh_command(invalid.handle,"action","start");check(state.find("Unsupported native effect ID8373")!=std::string::npos||state.find("Unsupported native effect ID 8373")!=std::string::npos,"zero support does not relax unsupported positive IDs");invalid.passed=true;
    for(int mode:{-1,1}){Program p;for(int n:{0,0,mode,0,26})p.push(n);p.op(0x278);p.dialogue(0);Fixture f(std::move(p));state=woh_command(f.handle,"action","start");check(state.find("Unsupported effect cancellation text-window mode")!=std::string::npos,"new zero support never silently drops a native auto-show request");f.passed=true;}
}

static void nativeEffectZeroResetAndCaptureIndependence(){
    Program p;p.image(1000,"fake",26);effectRequest(p,26,1);p.dialogue(0);effectRequest(p,26,0);p.push(2);p.op(0x10e);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,effectZeroScaleTable);f.command("textSpeed","0");auto state=f.command("action","start");f.command("tick","1000");state=f.command("tick","1484");const auto saved=visibleLayers(state);f.command("save","2");
    state=f.command("advance");check(!boolean(state,"canSave"),"reset fixture really has a pending cancellation");state=f.command("load","2");check(boolean(state,"canSave")&&visibleLayers(state)==saved,"load clears a transient request while restoring the slot effect unchanged");
    state=f.command("tick","2000");state=f.command("tick","2184");check(number(layerObject(state,26),"w")!=150&&number(state,"nextTickMs")==16,"loaded loop continues through wrap rather than inheriting the stale pending cancellation");
    f.command("advance");f.command("title");state=f.command("action","start");check(boolean(state,"canSave")&&number(layerObject(state,26),"w")==100,"title/newgame discard only transient cancellation state and start clean");f.passed=true;

    Program q;q.image(1000,"fake",0,{{870,0,889,1080,1,75}});q.image(1000,"fake",26);effectRequest(q,26,1);q.dialogue(0);
    effectRequest(q,26,0);q.push(205);q.push(0);q.op(0x279);effectRequest(q,0,205);q.commit();q.dialogue(1);q.op(0xf4);
    std::string table=effectZeroScaleTable+"205 0 0 0 64 -32767 -32767 -100001 0 0 0 1000\n205 1000 50 50 0 -32767 -32767 -100001 0 0 0 1000\n";
    const std::string windows=
        "1 0 0 0x80000000 -32767 -32767 -32767 -32767 -32767 -32767\n"
        "1 1 1 0x80000000 -32767 -32767 -32767 -32767 -32767 -32767\n"
        "2 0 2 0x80000000 -32767 -32767 -32767 -32767 -32767 -32767\n"
        "2 1 3 0x80000000 -32767 -32767 -32767 -32767 -32767 -32767\n"
        "205 0 692 0x4144080 1 2 -32767 -32767 -32767 -32767\n205 1 693 0x4144080 -32767 -32767 -32767 -32767 -32767 -32767\n";
    Fixture captures(std::move(q),true,table,"","",windows);captures.command("textSpeed","0");captures.command("action","start");auto next=captures.command("advance");check(waiting(next,"transitionReady"),"zero and279 share the actual200 prepare boundary");
    next=acknowledge(captures,next);next=captures.command("tick","1000");captures.command("save","2");const auto doc=mahoyo::save::read(captures.root/"savedata/slot2.whs");const std::string extra(doc.hostExtra.begin(),doc.hostExtra.end());
    check(extra.find("EFFECTFRAMES1 1\n692 50 50 255 870 0 889 1080\n")!=std::string::npos,"effect0 cancellation leaves independent279 captured window/frame metadata intact");captures.passed=true;
}

static void nativeEffectZeroCanonicalCloneAndNewPlayback(){
    const std::string table=effectZeroScaleTable+
        "3 0 -32767 -32767 -32767 1000 -32767 -100001 0 0 0 1000\n"
        "3 1000 -32767 -32767 -32767 3000 -32767 -100001 0 0 0 1000\n"
        "5 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 1 1000\n"
        "5 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 1 1000\n";
    for(int clip:{1,3}){
        Program p;p.image(1000,"fake",26);effectRequest(p,26,clip);p.dialogue(0);effectRequest(p,26,0);p.dialogue(1);
        p.clone(27,26,"fake");p.dialogue(0);effectRequest(p,26,5);p.dialogue(1);p.clone(28,26,"fake");p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true,table,"fake 100 100\n");f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");auto state=f.command("tick","1484");const auto live=number(layerObject(state,26),"w");
        state=f.command("advance");check(number(layerObject(state,26),"w")==live&&number(state,"nextTickMs")==0,"finite and looping cancellation both retain live graphics and become idle");
        f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(saved.hostExtra.begin(),saved.hostExtra.end());check(extra.find("EFFECTBASES1 1\n26 ")!=std::string::npos,"cancelled D0 packet is retained separately from the live WH18 layer");
        const auto before=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before&&number(state,"nextTickMs")==0,"fresh WH18 restores frozen graphics without scheduling the retained base");
        state=f.command("advance");double canonical=clip==1?100:300;
        check(number(layerObject(state,27),"w")==canonical&&number(layerObject(state,26),"w")==live,"clone uses pre-effect D0 for loop cancellation and terminal D0 for finite cancellation");
        state=f.command("advance");check(number(layerObject(state,26),"w")==live&&number(state,"nextTickMs")==16,"new positive effect samples current graphics independently of its inherited D0");
        f.command("save","2");saved=mahoyo::save::read(f.root/"savedata/slot2.whs");extra.assign(saved.hostExtra.begin(),saved.hostExtra.end());check(extra.find("EFFECTBASES1 0\n")!=std::string::npos,"new playback takes ownership of the canonical packet without a duplicate stopped entry");
        state=f.command("advance");check(number(layerObject(state,28),"w")==canonical,"new278 attachment base inherits the cancelled D0 rather than frozen graphics");f.passed=true;
    }
}

static void nativeEffectZeroCanonicalPendingAndCleanup(){
    const std::string table="3 0 -32767 -32767 -32767 1000 -32767 0 0 0 0 1000\n3 1000 -32767 -32767 -32767 3000 -32767 3500 0 0 0 1000\n";
    for(bool editsBeforeZero:{false,true}){
        Program p;p.image(1000,"fake",26);effectRequest(p,26,3);p.dialogue(0);
        if(!editsBeforeZero)effectRequest(p,26,0);p.axisScale(26,2500,1000);p.rotate(26,2500);if(editsBeforeZero)effectRequest(p,26,0);p.dialogue(1);
        p.clone(27,26,"fake");p.dialogue(0);p.push(0);p.push(26);p.op(0x251);p.alpha(26,64);p.origin(26,3,4);p.dialogue(1);p.clone(28,26,"fake");p.dialogue(0);
        p.push(26);p.op(0x241);p.image(1000,"fake",26);p.clone(29,26,"fake");p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p),true,table,"fake 100 100\n");f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");f.command("tick","1484");auto state=f.command("advance");
        state=f.command("advance");auto copied=layerObject(state,27);check(number(copied,"w")==250&&number(copied,"rotation")==24.9993896,"explicit pending scale/rotation apply after the finite terminal base regardless of zero call order");
        state=f.command("advance");state=f.command("advance");copied=layerObject(state,28);
        check(number(copied,"z")==0&&std::abs(number(copied,"alpha")-64./255)<1e-8&&number(copied,"pivotX")!=.5,"later priority/alpha/origin edits update retained D0 alongside the live layer");
        state=f.command("advance");check(number(layerObject(state,29),"w")==100&&number(layerObject(state,26),"rotation")==0,"delete/recreate clears the old retained base instead of leaking its geometry");f.passed=true;
    }
    Program pending;pending.image(1000,"fake",26);effectRequest(pending,26,3);pending.dialogue(0);effectRequest(pending,26,0);pending.push(7);pending.push(26);pending.op(0x251);pending.clone(27,26,"fake");pending.dialogue(1);pending.op(0xf4);
    Fixture f(std::move(pending),true,table,"fake 100 100\n");f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");f.command("tick","1484");auto state=f.command("advance");
    check(number(layerObject(state,27),"w")==300&&number(layerObject(state,27),"z")==7,"clone before prepare retains active-effect terminal semantics with explicit pending priority last");f.passed=true;
}

static void nativeEffectZeroFiniteFollowupMotion(){
    const std::string table="3 0 50 50 -32767 1000 1000 -100001 0 0 0 24000\n3 24000 150 80 -32767 -32767 -32767 -100001 0 0 0 24000\n";
    Program p;p.image(1000,"fake",26);effectRequest(p,26,3);p.dialogue(0);effectRequest(p,26,0);
    for(int n:{0,400,128,INT32_MIN,26})p.push(n);p.op(0x245);p.position(26,90,70,400);p.dialogue(1);p.clone(27,26,"fake");p.dialogue(0);p.op(0xf4);
    const std::string windows="3 0 0 0xffffffffffffffff -32767 -32767 -32767 -32767 -32767 -32767\n3 1 1 0xffffffffffffffff -32767 -32767 -32767 -32767 -32767 -32767\n";
    Fixture f(std::move(p),true,table,"fake 100 100\n","",windows);woh_destroy(f.handle);auto properties=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(properties);
    {std::ofstream out(properties);for(int index=0;index<2;++index){out<<"3 "<<index<<' '<<index<<" 0xffffffffffffffff "<<(index?22:11)<<' '<<(index?34:17);for(int n=0;n<11;++n)out<<" -32767";out<<'\n';}}
    f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");auto state=f.command("tick","1484");auto before=layerObject(state,26);
    state=f.command("advance");check(number(layerObject(state,26),"x")==number(before,"x"),"finite cancellation followed by native INTMIN motion starts from current graphics, not terminal D0");
    state=f.command("tick","1884");const auto current=layerObject(state,26);check(boolean(state,"canSave")&&std::abs(number(current,"alpha")-128./255)<1e-8,"the independent400ms fade/motion finish normally");
    state=f.command("advance");auto cloned=layerObject(state,27);check(number(cloned,"x")==number(current,"x")&&number(cloned,"y")==number(current,"y")&&number(cloned,"alpha")==number(current,"alpha"),"completed motion/fade commits their endpoints to D0 after cancellation, matching subsequent clone");
    check(number(cloned,"pivotX")==1.5&&number(cloned,"pivotY")==1.5,"pending243 CC origin overrides terminal effect6/7 before later endpoint and clone");f.passed=true;
}
