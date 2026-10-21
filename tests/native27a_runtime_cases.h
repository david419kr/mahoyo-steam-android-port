#pragma once
//Synthetic CHS only; Native27A's event registry is not the active-job vector.
namespace native27a_cases {
static const std::string finite="205 0 -32767 -32767 0 -32767 -32767 -100001 0 0 0 500\n205 500 -32767 -32767 255 -32767 -32767 -100001 0 0 0 500\n";
static const std::string loop="206 0 -32767 -32767 0 -32767 -32767 -100001 0 0 200 1000\n206 200 -32767 -32767 255 -32767 -32767 -100001 0 0 200 1000\n206 1000 -32767 -32767 0 -32767 -32767 -100001 0 0 200 1000\n";
static void effect(Program& p,int id,int clip){for(int n:{0,0,0,clip,id})p.push(n);p.op(0x278);}
static void pause(Program& p,int raw){p.push(raw);p.op(0x27a);}
static void prepare(Program& p){for(int n:{0,1,0})p.push(n);p.op(0x110);}
static std::string prepared(Fixture& f,std::string s){for(int n=0;n<8&&waiting(s,"transitionReady");++n)s=acknowledge(f,s);return s;}
static double alpha(const std::string& s,int id=0){return number(layerObject(s,id),"alpha");}
static std::string start(Fixture& f){f.command("textSpeed","0");return prepared(f,f.command("action","start"));}
static void rootChildClockAndIdempotence(){
    Program p;p.image();p.image(1000,"fake",1,{},0);effect(p,0,206);effect(p,1,206);p.dialogue(0);
    pause(p,7);prepare(p);p.dialogue(1);pause(p,-1);prepare(p);p.dialogue(0);pause(p,0);prepare(p);p.dialogue(1);pause(p,0);prepare(p);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true,loop,"fake 100 100\n");auto s=start(f);f.command("tick","1000");s=f.command("tick","1084");check(alpha(s)>0&&alpha(s)<1,"registered root and child first animate");
    s=prepared(f,f.command("advance"));const auto frozen=visibleLayers(s);check(number(s,"nextTickMs")==0&&boolean(s,"canSave"),"paused-only stable input has no idle timer and remains saveable");
    s=f.command("tick","900000");check(visibleLayers(s)==frozen,"pause skips root and child callbacks regardless of elapsed wall time");s=prepared(f,f.command("advance"));s=f.command("tick","1900000");check(visibleLayers(s)==frozen,"repeated nonzero pause does not reset phase");
    s=prepared(f,f.command("advance"));check(visibleLayers(s)==frozen&&number(s,"nextTickMs")==16,"resume preserves the frozen raw phase and schedules the existing jobs");s=f.command("tick","2000000");check(alpha(s)>alpha(frozen)&&alpha(s,1)==alpha(s),"resumed root and child advance from the preserved phase");
    auto before=alpha(s);s=prepared(f,f.command("advance"));s=f.command("tick","3000000");check(alpha(s)>before,"same-state resume does not restart at the first key");f.passed=true;
}
static void pendingOrderAndRegisteredSnapshot(){
    for(int kind=0;kind<4;++kind){
        Program p;p.image();if(kind){effect(p,0,205);p.dialogue(0);} //kind0 is never registered.
        if(kind==2){for(int n:{1,32,0})p.push(n);p.op(0x27f);} //explicit query retires completed registry.
        if(kind==3)pause(p,1);effect(p,0,206);if(kind!=3)pause(p,1);prepare(p);p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p),true,finite+loop,"fake 100 100\n");auto s=start(f);
        if(kind){f.command("tick","1000");s=f.command("tick","2000");check(alpha(s)==1&&number(s,"nextTickMs")==0,"finite completion stops callbacks without deleting its native registration");s=prepared(f,f.command("advance"));}
        auto before=alpha(s);s=f.command("tick","900000");
        if(kind==1)check(alpha(s)==before&&number(s,"nextTickMs")==0,"finished old registered ID selects its replacement for278 then27A");
        else check(alpha(s)>before&&number(s,"nextTickMs")==16,"unregistered IDs, completed-query cleanup and27A then278 never pause a new job");f.passed=true;
    }
}
static void manualLiveAndSave(){
    Program p;p.image();effect(p,0,206);p.dialogue(0);pause(p,1);prepare(p);p.dialogue(1);
    p.alpha(0,64);p.position(0,90,70);for(int n:{0x123456,6,0})p.push(n);p.op(0x253);prepare(p);p.dialogue(0);pause(p,0);prepare(p);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,loop,"fake 100 100\n");auto s=start(f);f.command("tick","1000");f.command("tick","1084");s=prepared(f,f.command("advance"));s=prepared(f,f.command("advance"));
    check(std::abs(alpha(s)-64./255.)<1e-8&&number(layerObject(s,0),"colorMode")==6&&number(layerObject(s,0),"rgb")==0x123456,"paused callbacks preserve independent manual alpha/position/color setters");
    const auto live=visibleLayers(s);s=f.command("tick","900000");check(visibleLayers(s)==live&&boolean(s,"canSave")&&number(s,"nextTickMs")==0,"paused setters remain current and stable over ticks");f.command("save","2");
    const auto doc=mahoyo::save::read(f.root/"savedata/slot2.whs");check(doc.layers.size()==1&&std::abs(doc.layers[0].alpha-64./255.)<1e-8,"existing live layer packet retains manual alpha independently from baseline");
    std::string extra(doc.hostExtra.begin(),doc.hostExtra.end());check(extra.find("WOHHOST23\n")==0,"paused restore regression requires the coordinated WH21 writer");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");s=f.command("load","2");check(visibleLayers(s)==live&&number(s,"nextTickMs")==0,"fresh paused restore preserves the actual live layer instead of resampling baseline");s=f.command("tick","1000000");check(visibleLayers(s)==live,"first restored tick also skips the paused callback");s=prepared(f,f.command("advance"));check(number(s,"nextTickMs")==16,"fresh restored job can resume without resetting phase");f.passed=true;
}
static void finiteWaitAndIndependentJobs(){
    Program p;p.image();p.image(1000,"fake",1);effect(p,0,205);p.dialogue(0);pause(p,1);prepare(p);p.alpha(1,32);p.position(1,100,50,100);for(int n:{1,33,-1})p.push(n);p.op(0x27f);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,finite,"fake 100 100\n");auto s=start(f);f.command("tick","1000");f.command("tick","1084");s=prepared(f,f.command("advance"));auto held=alpha(s);check(waiting(s,"animation"),"paused finite backend2 remains a27F wait target");
    f.command("tick","2000");s=f.command("tick","900000");check(waiting(s,"animation")&&alpha(s)==held&&std::abs(number(layerObject(s,1),"x")-250)<1e-7,"old deadline cannot release paused finite wait while unrelated motion completes");const auto instructions=number(s,"instructions");s=f.command("advance");check(number(s,"instructions")==instructions&&waiting(s,"animation"),"ordinary text advance does not bypass paused effect wait");s=f.command("title");check(s.find("\"scene\":\"title\"")!=std::string::npos,"title teardown can leave a paused finite wait cleanly");f.passed=true;
}
static void deletionAndLoopWait(){
    Program p;p.image();effect(p,0,206);p.dialogue(0);pause(p,1);prepare(p);for(int n:{1,32,0})p.push(n);p.op(0x27f);p.dialogue(1);p.push(0);p.op(0x241);p.image();effect(p,0,206);pause(p,1);prepare(p);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true,loop,"fake 100 100\n");auto s=start(f);s=prepared(f,f.command("advance"));check(waiting(s,"input")&&number(s,"nextTickMs")==0,"paused loop is skipped by27F without cancelling its registration");s=prepared(f,f.command("advance"));check(waiting(s,"input")&&number(s,"nextTickMs")==0,"same-prepare delete/create retains old ID visibility until native prepare");f.passed=true;
}
static void stackAndEmpty(){
    Program p;p.push(2345);pause(p,1);pause(p,-12);pause(p,0);p.image();p.push(0);p.op(0x251);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,"","fake 100 100\n");auto s=start(f);check(number(layerObject(s,0),"z")==2345&&number(s,"nextTickMs")==0&&boolean(s,"canSave"),"27A consumes exactly one signed value with no push and empty registry causes no job");f.passed=true;
}
static void core(){rootChildClockAndIdempotence();pendingOrderAndRegisteredSnapshot();finiteWaitAndIndependentJobs();deletionAndLoopWait();stackAndEmpty();}
static void run(){core();manualLiveAndSave();}
}
