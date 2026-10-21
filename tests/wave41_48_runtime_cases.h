#pragma once
namespace native_wave41_48_cases {
static std::string wave(const std::string& state,int id){auto l=layerObject(state,id);auto at=l.find("\"wave\":{");check(at!=std::string::npos,"new wave DTO exists");return l.substr(at);}
static std::string prepared(Fixture& f,std::string state){for(int i=0;i<8&&waiting(state,"transitionReady");++i)state=acknowledge(f,state);if(waiting(state,"frame"))state=f.command("tick","1000");return state;}
static std::string rejectedPrepare(Fixture& f){auto s=std::string(woh_command(f.handle,"action","start"));for(int n=0;n<8&&waiting(s,"transitionReady")&&s.find("\"error\":\"\"")!=std::string::npos;++n){auto tail=s.substr(s.find("\"transition\":"));auto payload=std::to_string(uint64_t(number(tail,"id")));s=woh_command(f.handle,"transitionReady",payload.c_str());}return s;}
static void rasterAndRestore(){
    for(int preset=41;preset<=48;++preset){
        Program p;p.image();if(preset>=46)for(int n:{0,66,0})p.push(n);if(preset>=46)p.op(0x253);p.wave(0,preset);p.commit();p.dialogue(0);p.wave(0,0);p.commit();p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p),true,"","fake 100 100\n");f.imageHeader("fake",preset>=45?2552:1532,(preset==43||preset==44)?8130:2034);f.command("textSpeed","0");auto state=prepared(f,f.command("action","start"));
        check(waiting(state,"input")&&number(wave(state,0),"preset")==preset&&number(wave(state,0),"strength")==1&&boolean(state,"canSave"),"new8 Native table/large43..44 raster shares saveable persistent lifetime");woh_wave_clock(f.handle,1000);woh_wave_clock(f.handle,1016);state=woh_snapshot(f.handle);const auto phase=number(wave(state,0),"elapsedMs");f.command("save","2");
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(number(wave(state,0),"preset")==preset&&number(wave(state,0),"elapsedMs")==phase,"new8 roundtrip uses unchanged WAVES1 packet andphase");
        state=prepared(f,f.command("advance"));check(layerObject(state,0).find("\"wave\":")==std::string::npos&&number(state,"nextTickMs")==0,"new8 stop removes raster without idle wake");f.passed=true;
    }
    Program p;p.image();p.wave(0,43);p.dialogue(0);p.op(0xf4);Fixture tooLarge(std::move(p),true,"","fake 100 100\n");tooLarge.imageHeader("fake",1532,8137);auto bad=rejectedPrepare(tooLarge);check(bad.find("Unsupported wave43 bitmap")!=std::string::npos,"new mode0 cap remains strict beyond3051cells");tooLarge.passed=true;
    for(int badKind=0;badKind<3;++badKind){Program x;x.image();if(badKind==0)x.blend(0,16,0);if(badKind==1)x.image(1000,"fake",1,{},0);if(badKind==2)x.noise(0,1);x.wave(0,41);x.dialogue(0);x.op(0xf4);Fixture f(std::move(x),true);auto bad=rejectedPrepare(f);check(bad.find("\"error\":\"\"")==std::string::npos,"new table domain does not broaden root material/nested/noise combos");f.passed=true;}
}
static void configFailureLifecycle(){
    for(int preset:{49,50,51}){
        Program p;p.image();p.image(1000,"fake",1);p.wave(0,2);p.dialogue(0);p.wave(0,preset);p.wave(1,42);p.commit();p.dialogue(1);p.wave(1,preset);p.commit();p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);f.command("textSpeed","0");auto s=prepared(f,f.command("action","start"));check(number(wave(s,0),"preset")==2,"failure profile fixture starts with an actual retained raster");s=prepared(f,f.command("advance"));check(layerObject(s,0).find("\"wave\":")==std::string::npos&&number(wave(s,1),"preset")==42,"Native config failure forces old jobs while keeping same-prepare valid sibling");s=prepared(f,f.command("advance"));check(layerObject(s,1).find("\"wave\":")==std::string::npos&&boolean(s,"canSave")&&number(s,"nextTickMs")==0,"49..51 leaves no active/save/wait/render debt");f.passed=true;
    }
}
static void run(){rasterAndRestore();configFailureLifecycle();}
}
