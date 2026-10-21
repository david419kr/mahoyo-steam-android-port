#pragma once
#include "../runtime/input_region.h"
namespace native118_12f_cases {
static std::string region(const std::string& s){auto at=s.find("\"inputRegion\":");check(at!=std::string::npos,"118 readiness DTO exists");return s.substr(at,s.find('}',at)-at+1);}
static std::string token(const std::string& s,const std::string& key){auto at=s.find("\""+key+"\":\"");check(at!=std::string::npos,"118 readiness token exists");at+=key.size()+4;return s.substr(at,s.find('"',at)-at);}
static std::string ready(Fixture& f,const std::string& s){auto r=region(s);return f.command("inputUiReady",(token(r,"generation")+" "+token(r,"request")+" "+std::to_string(uint64_t(number(s,"version")))+" 2400 1800").c_str());}
static std::string finishPrepare(Fixture& f,std::string s){for(int n=0;n<8&&waiting(s,"transitionReady");++n)s=acknowledge(f,s);return s;}
static void controller(){
    mahoyo::input_region::Controller c;const auto generation=c.generation(),epoch=c.epoch();c.begin118();auto first=c.request();
    check(c.readyPending()&&!c.stable().active&&!c.restricted()&&!c.safe()&&c.epoch()==epoch&&c.generation()==generation,"118 creates only volatile readiness; no region or physical reset");
    check(!c.ready(generation+1,first)&&c.readyPending(),"118 rejects stale generation");check(c.ready(generation,first)&&c.safe()&&!c.restricted(),"118 ACK releases the common barrier without persistent input restriction");
    check(c.begin120(),"existing120 still registers a region");check(c.ready(c.generation(),c.request()),"existing120 ACK");c.request122(true,144272);c.commit();
    const auto before=c.stable();const auto oldEpoch=c.epoch(),trigger=c.trigger();c.begin118();
    check(c.stable().active==before.active&&c.stable().currentEnabled==before.currentEnabled&&c.stable().currentTarget==before.currentTarget&&c.epoch()==oldEpoch&&c.trigger()==trigger,"118 preserves existing120/122 region, target and physical epoch");
    check(c.ready(c.generation(),c.request())&&c.restricted(),"118 completion preserves an already existing region");c.release121();check(!c.restricted()&&c.stable().currentEnabled,"121 retains its existing release-only behavior");
}
static void barrier(){
    Program p;p.push(817);p.op(0x118);p.image();p.push(0);p.op(0x251);p.dialogue(0);p.op(0x118);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto s=f.command("action","start");
    check(waiting(s,"inputRegionReady")&&!boolean(region(s),"active")&&!boolean(region(s),"enabled")&&boolean(region(s),"readyPending")&&!boolean(s,"canSave")&&number(s,"nextTickMs")==0,"118 yields for common UI/GL readiness without registering touch region or busy clock");
    auto instructions=number(s,"instructions"),pc=number(s,"modulePc");auto old=s;s=f.command("tick","900000");
    check(waiting(s,"inputRegionReady")&&number(s,"instructions")==instructions&&number(s,"modulePc")==pc,"time alone never fabricates118 readiness");
    auto r=region(old);s=f.command("inputUiReady",(token(r,"generation")+" "+token(r,"request")+" "+std::to_string(uint64_t(number(old,"version")))+" 2400 1800").c_str());check(waiting(s,"inputRegionReady"),"stale rendered version does not complete118");
    f.command("pause");s=ready(f,std::string(woh_snapshot(f.handle)));check(waiting(s,"inputRegionReady"),"paused118 rejects an otherwise matching ACK");s=f.command("resume");s=ready(f,s);s=finishPrepare(f,s);
    check(waiting(s,"input")&&!boolean(region(s),"active")&&!boolean(region(s),"readyPending")&&number(s,"nextTickMs")==0&&number(layerObject(s,0),"z")==817,"118 preserves stack sentinel, ordinary dialogue and unrestricted idle state");
    const auto firstLine=number(s,"textLine");s=f.command("advance");check(waiting(s,"inputRegionReady")&&!boolean(region(s),"active"),"repeated118 creates a fresh common barrier without a persistent region");s=ready(f,s);s=finishPrepare(f,s);check(waiting(s,"input")&&number(s,"textLine")==firstLine+1&&!boolean(region(s),"active"),"118 does not block ordinary advance after completion");f.passed=true;
}
static void existingRegion(){
    Program p;p.op(0x120);p.op(0x118);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto s=f.command("action","start");
    check(boolean(region(s),"active"),"120 still registers its original region");auto before=region(s);s=ready(f,s);
    check(waiting(s,"inputRegionReady")&&boolean(region(s),"active")&&token(region(s),"generation")==token(before,"generation")&&token(region(s),"epoch")==token(before,"epoch")&&token(region(s),"request")!=token(before,"request"),"118 preserves active120 and physical epoch while issuing a new readiness token");
    s=ready(f,s);s=finishPrepare(f,s);check(waiting(s,"input")&&boolean(region(s),"active"),"118 completion does not release an existing120 region");f.passed=true;
}
static void query(){
    Program p;p.push(54321);p.push(0);p.op(0x12f);p.op(0x30);p.dialogue(0);p.op(0x12f);p.dialogue(1);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto s=f.command("action","start");s=finishPrepare(f,s);
    check(waiting(s,"input")&&!boolean(region(s),"readyPending")&&!boolean(region(s),"active"),"12f game-JA query does not create readiness or touch state");f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");
    struct Host:mahoyo::vm::Host{std::vector<uint8_t>loadModule(const std::string&)override{return{};}mahoyo::vm::Flow invoke(uint32_t,mahoyo::vm::Machine&)override{return mahoyo::vm::Flow::Halt;}} host;mahoyo::vm::Machine vm(host);vm.loadState(saved.vm);
    check(vm.stack()==std::vector<int32_t>({54321,1}),"actual wik_fin literal0 is the JA comparator;12f pushes0 without consuming the lower stack");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");s=f.command("load","2");check(waiting(s,"input"),"game-JA query checkpoint loads without adding a schema field");s=f.command("advance");s=finishPrepare(f,s);f.command("save","2");saved=mahoyo::save::read(f.root/"savedata/slot2.whs");vm.loadState(saved.vm);check(vm.stack()==std::vector<int32_t>({54321,1,0}),"fresh Runtime continues returning fixed game-JA0 independent from UI language");f.passed=true;
}
static void run(){controller();barrier();existingRegion();query();}
}
