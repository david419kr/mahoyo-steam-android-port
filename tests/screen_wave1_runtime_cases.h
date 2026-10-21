#pragma once
// Include after screen_wave_runtime_cases.h and the common synthetic Fixture.
// No game pixels or user saves.

static void nativeScreenRasterOneSaveRestoreAndStop(){
    Program p;p.image();p.push(817);p.wave(-1,1);p.push(0);p.op(0x251);p.commit();p.dialogue(0);
    p.wave(-1,0);p.commit();p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");auto state=screenStart(f);
    check(number(screenObject(state),"preset")==1&&number(screenObject(state),"strength")==1&&number(layerObject(state,0),"z")==817,"actual global1 consumes exactly three operands and preserves the sentinel below them");
    check(boolean(state,"canSave")&&number(state,"nextTickMs")==16,"global1 steady raster remains dynamic and saveable");
    const auto savedScreen=screenObject(state),savedLayers=visibleLayers(state);f.command("save","2");
    auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string host(document.hostExtra.begin(),document.hostExtra.end());
    check(currentHostSchema(document)&&host.find("SCREENWAVE1 1 1\n1 0 0 0 1 ")!=std::string::npos,"existing WH screen footer preserves preset1 rather than its old default9");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());check(f.handle!=0,"fresh global1 runtime created");f.command("textSpeed","0");state=f.command("load","2");
    check(screenObject(state)==savedScreen&&visibleLayers(state)==savedLayers,"fresh Runtime load preserves global1 identity phase and scene exactly");
    state=f.command("tick","5000000");check(number(screenObject(state),"preset")==1&&number(screenObject(state),"elapsedMs")==number(savedScreen,"elapsedMs")+16,"loaded global1 rebases platform clock without a wall-time phase jump");
    state=f.command("advance");check(waiting(state,"transitionReady")&&screenObject(state)!="null","actual global stop remains pending until graphics readiness");
    state=acknowledge(f,state);if(waiting(state,"frame"))state=f.command("tick","5000016");
    check(waiting(state,"input")&&screenObject(state)=="null"&&number(state,"nextTickMs")==0,"actual global stop0 releases the raster and idle polling");
    f.command("title");state=f.command("load","2");check(screenObject(state)==savedScreen,"title cleanup does not change saved preset1 metadata");f.passed=true;
}

static void nativeScreenRasterOneReplacementAndStrictRestore(){
    Program p;p.image();p.wave(-1,1);p.commit();p.dialogue(0);p.wave(-1,9);p.commit();p.dialogue(1);p.wave(-1,1);p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");auto state=screenStart(f);auto first=screenObject(state);
    check(number(first,"preset")==1,"replacement fixture starts with exact global1");
    state=f.command("advance");check(waiting(state,"transitionReady")&&screenObject(state)==first,"global9 replacement does not overwrite committed1 before ready");
    state=acknowledge(f,state);if(waiting(state,"frame"))state=f.command("tick","2000");
    check(waiting(state,"input")&&number(screenObject(state),"preset")==9&&screenObject(state).find("\"id\":\"2\"")!=std::string::npos,"global1-to9 commits a fresh identity and preserves the requested preset");
    state=f.command("advance");state=acknowledge(f,state);if(waiting(state,"frame"))state=f.command("tick","3000");
    check(waiting(state,"input")&&number(screenObject(state),"preset")==1&&screenObject(state).find("\"id\":\"3\"")!=std::string::npos,"global9-to1 commits the distinct Native1 material");
    f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");const auto originalScreen=screenObject(state),originalLayers=visibleLayers(state);const auto pc=number(state,"modulePc"),instructions=number(state,"instructions");
    for(int preset:{0,2,8,10,51,52}){
        auto bad=good;std::string host(bad.hostExtra.begin(),bad.hostExtra.end());auto at=host.find("SCREENWAVE1 3 1\n");check(at!=std::string::npos,"strict restore fixture has one exact screen record");at=host.find('\n',at)+1;host.replace(at,1,std::to_string(preset));bad.hostExtra.assign(host.begin(),host.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);
        auto rejected=f.command("load","2");check(rejected.find("\"notice\":\"Loaded\"")==std::string::npos&&screenObject(rejected)==originalScreen&&visibleLayers(rejected)==originalLayers&&number(rejected,"modulePc")==pc&&number(rejected,"instructions")==instructions,"unreviewed saved preset rejects atomically without changing the VM or current global1");
    }
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);state=f.command("load","2");check(screenObject(state)==originalScreen,"valid preset1 remains loadable after rejected packets");f.passed=true;
    for(auto args:std::vector<std::array<int,3>>{{{0,1,0}},{{-1,2,0}},{{-1,10,0}},{{-1,1,-1}}}){
        Program invalid;invalid.image();invalid.wave(args[0],args[1],args[2]);invalid.dialogue(0);Fixture rejected(std::move(invalid),true);
        auto error=std::string(woh_command(rejected.handle,"action","start"));check(waiting(error,"error")&&error.find("Unsupported")!=std::string::npos&&screenObject(error)=="null","local1 otherpositive and negativefade remain explicit errors");rejected.passed=true;
    }
}
