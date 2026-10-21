#pragma once
#include "../runtime/input_region.h"
namespace input120_cases {
using Input=mahoyo::input_region::Controller;
static int checks=0;
static void verify(bool b,const char* label){++checks;check(b,label);}
static std::string input(const std::string& s){auto at=s.find("\"inputRegion\":");check(at!=std::string::npos,"input DTO");return s.substr(at,s.find('}',at)-at+1);}
static std::string token(const std::string& s,const std::string& key){auto at=s.find("\""+key+"\":\"");check(at!=std::string::npos,"token exists");at+=key.size()+4;return s.substr(at,s.find('"',at)-at);}
static std::string ack(Fixture& f,std::string s){auto r=input(s);return f.command("inputUiReady",(token(r,"generation")+" "+token(r,"request")+" "+std::to_string(uint64_t(number(s,"version")))+" 2400 1800").c_str());}
static std::string pointer(const std::string& s,int down=1,int inside=1){auto r=input(s);return token(r,"generation")+" "+token(r,"trigger")+" "+token(r,"epoch")+" "+std::to_string(down)+" "+std::to_string(inside);}
static std::string prepare(Fixture& f,std::string s,uint64_t& clock){for(int i=0;i<10&&waiting(s,"transitionReady");++i){s=f.command("transitionReady",std::to_string(uint64_t(number(s.substr(s.find("\"transition\":")),"id"))).c_str());clock+=100;s=f.command("tick",std::to_string(clock).c_str());}return s;}
static void controllerNativeMath(){
 Input c;verify(!c.needsTick()&&c.safe(),"idle controller no wake");c.request122(true,144272);c.commit();verify(!c.needsTick(),"armed no hold no wake");
 auto g=c.generation(),t=c.trigger(),e=c.epoch();verify(c.pointer(g,t,e,true,true,0),"valid first press");
 for(int i=1;i<=177;++i){c.sample(uint64_t(i)*17,true);verify(c.elapsed()==std::min(3000,i*17)&&c.progressLeft()==4+std::min(3000,i*17)*192/3000,"Native177 callback math");verify(!c.jumpQueued(),"threshold frame does not jump");}
 c.sample(3026,false);verify(!c.jumpQueued(),"Native state3 runnable0 holds");c.request122(false,0);c.commit();c.sample(3043,true);verify(c.jumpQueued()&&c.jumpTarget()==144272,"script cancel preserves committed typeE");
 c.jumped();verify(!c.needsTick()&&c.safe(),"completed no continuous wake");
 c.request122(true,144272);c.commit();g=c.generation();t=c.trigger();e=c.epoch();c.pointer(g,t,e,true,false,0);c.sample(5000,true);verify(c.elapsed()==0&&!c.needsTick(),"outside initial press latches until release");c.pointer(g,t,e,false,true,5000);c.pointer(g,t,e,true,true,5000);c.sample(5500,true);verify(c.elapsed()==17,"one large gap capped17");c.pointer(g,t,e,false,true,5500);verify(c.elapsed()==0&&!c.needsTick(),"release resets hold and wake");
 c.pointer(g,t,e,true,true,6000);c.physicalReset();verify(!c.pointer(g,t,e,true,true,6000),"stale pointer epoch after lifecycle cancel");
 c.reset();verify(c.begin120()&&!c.begin120(),"Native120 existing region continue");auto request=c.request();verify(!c.ready(c.generation()+1,request)&&c.readyPending(),"stale UI ack rejected");verify(c.ready(c.generation(),request)&&c.restricted(),"actual ack keeps persistent region");c.release121();verify(!c.restricted(),"121 only region");
}
static Program runtimeProgram(){
 Program p;p.push(555);p.op(0x120);p.op(1);auto target=p.words.size();p.op(0);p.push(127);p.op(0x122);p.commit();p.dialogue(0);p.op(0x120);p.dialogue(0);
 p.words[target]=uint32_t(p.words.size()*4);p.op(0x121);p.push(-1);p.push(-1);p.op(0x122);p.commit();p.dialogue(1);p.op(0xf4);return p;
}
static void runtimeAckSaveHold(){
 Fixture f(runtimeProgram());f.command("textSpeed","0");auto s=f.command("action","start");verify(waiting(s,"inputRegionReady")&&!boolean(s,"canSave"),"120 blocks on real UI ack");auto old=s;
 s=f.command("tick","900000");verify(waiting(s,"inputRegionReady"),"clock does not manufacture UI ready");
 s=f.command("inputUiReady",(token(input(old),"generation")+" "+token(input(old),"request")+" "+std::to_string(uint64_t(number(old,"version")))+" 2400 1800").c_str());verify(waiting(s,"inputRegionReady"),"stale rendered version rejected");
 s=ack(f,s);uint64_t clock=1000;s=prepare(f,s,clock);verify(waiting(s,"input")&&boolean(input(s),"enabled"),"122 current only after actual scene prepare ack");verify(number(s,"nextTickMs")==0,"armed idle runtime no render loop");
 auto textLine=number(s,"textLine");s=f.command("advance");verify(number(s,"textLine")==textLine,"region gates ordinary advance");
 s=f.command("save","2");auto path=f.root/"savedata/slot2.whs";auto saved=mahoyo::save::read(path);std::string extra(saved.hostExtra.begin(),saved.hostExtra.end());verify(currentHostSchema(saved)&&extra.find("INPUTREGION1 1 1 ")!=std::string::npos,"stable active region persisted in WH20");
 struct Host:mahoyo::vm::Host{std::vector<uint8_t>loadModule(const std::string&)override{return{};}mahoyo::vm::Flow invoke(uint32_t,mahoyo::vm::Machine&)override{return mahoyo::vm::Flow::Halt;}} host;
 mahoyo::vm::Machine m(host);m.loadState(saved.vm);verify(m.stack()==std::vector<int32_t>{555},"Native120 pop0 and122 pop2 preserve sentinel");
 //Fresh Runtime restores stable state but needs a new real UI/render ack.
 woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");s=f.command("load","2");verify(boolean(input(s),"readyPending")&&!boolean(s,"canSave"),"fresh restore waits for UI; physical hold not restored");s=ack(f,s);verify(waiting(s,"input")&&boolean(s,"canSave"),"restore ack does not advance original VM wait");
 auto down=pointer(s);s=f.command("inputPointer",down.c_str());verify(!boolean(s,"canSave")&&number(s,"nextTickMs")==16,"only active hold wakes and blocks save");
 s=f.command("inputPointer",pointer(s,0).c_str());verify(number(s,"nextTickMs")==0,"up removes own wake");
 s=f.command("inputPointer",pointer(s).c_str());f.command("pause");s=f.command("resume");s=f.command("inputPointer",down.c_str());verify(number(s,"nextTickMs")==0,"stale pre-pause input epoch ignored");
 s=f.command("inputPointer",pointer(s).c_str());for(int i=1;i<=177;++i)s=f.command("tick",std::to_string(200000+i*17).c_str());verify(number(input(s),"elapsedMs")==3000&&waiting(s,"input"),"threshold remains a distinct frame");s=f.command("tick","203026");verify(waiting(s,"transitionReady")&&!boolean(input(s),"active"),"next callback jumps raw target and executes121");s=prepare(f,s,clock);verify(number(s,"textLine")==textLine+1&&!boolean(input(s),"enabled")&&number(s,"nextTickMs")==0,"target reached and negative122 committed; no idle wake");
 f.passed=true;
}
static void footerAtomicAndLegacy(){
 Input c;c.request122(true,144272);c.commit();std::ostringstream out;c.write(out);
 auto valid=[](uint32_t target){if(target!=144272)throw std::runtime_error("bad target");};std::istringstream good(out.str());auto state=Input::read(good,valid);verify(state.currentTarget==144272,"strict tagged roundtrip");
 for(const std::string row:{"INPUTREGION1 2 0 0 0 0 0","INPUTREGION1 1 1 999 0 0 0","INPUTREGION1 1 0 1 0 0 0","INPUTREGION1 1 0 0 0 1 144272","INPUTREGION1 1 0 0 1 2 0","INPUTREGION1 1 0 0 1 1 -1"}){bool threw=false;try{std::istringstream in(row);Input::read(in,valid);}catch(...){threw=true;}verify(threw,"malformed input tag rejected");}
 Fixture f(runtimeProgram());f.command("textSpeed","0");auto s=ack(f,f.command("action","start"));uint64_t clock=1000;s=prepare(f,s,clock);f.command("save","2");auto path=f.root/"savedata/slot2.whs";auto doc=mahoyo::save::read(path);std::string extra(doc.hostExtra.begin(),doc.hostExtra.end());auto at=extra.find("INPUTREGION1");check(at!=std::string::npos,"input footer position");
 auto bad=extra;bad.replace(at,std::string("INPUTREGION1").size(),"INPUTREGIONX");doc.hostExtra.assign(bad.begin(),bad.end());mahoyo::save::writeAtomic(path,doc);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());s=f.command("load","2");verify(s.find("Invalid saved input region")!=std::string::npos&&number(s,"textLine")==0&&!boolean(input(s),"active"),"malformed load leaves live context unchanged");
 auto old=extra.substr(0,at);old.replace(0,9,"WOHHOST18");doc.hostExtra.assign(old.begin(),old.end());mahoyo::save::writeAtomic(path,doc);s=f.command("load","2");verify(!boolean(input(s),"active")&&!boolean(input(s),"enabled")&&!boolean(input(s),"readyPending"),"WH18 defaults inactive without invented state");f.passed=true;
}
static void movieReleaseTransaction(){
 Program p;p.op(0x120);p.op(1);auto offset=p.words.size();p.op(0);p.push(127);p.op(0x122);p.commit();p.op(0x1c0);p.op(0x1c1);p.dialogue(0);
 p.words[offset]=uint32_t(p.words.size()*4);p.op(0x121);p.push(-1);p.push(-1);p.op(0x122);p.commit();p.dialogue(1);p.op(0xf4);
 Fixture f(std::move(p));f.command("textSpeed","0");auto s=ack(f,f.command("action","start"));uint64_t clock=1000;s=prepare(f,s,clock);verify(waiting(s,"movieReady"),"synthetic movie preparation is not manufactured");
 auto movieId=std::to_string(uint64_t(number(s.substr(s.find("\"movie\":")),"id")));s=f.command("moviePrepared",(movieId+" 200000").c_str());verify(waiting(s,"movie"),"backend movie ready event resumes its own wait");
 s=f.command("inputPointer",pointer(s).c_str());for(int i=1;i<=178;++i)s=f.command("tick",std::to_string(300000+i*17).c_str());
 verify(waiting(s,"inputJumpRelease")&&token(input(s),"jumpRelease")!="0"&&number(s,"nextTickMs")==0,"typeE waits actual player release without busy loop");
 auto previous=s;s=f.command("movieEnded",(movieId+" 200000 200000 0").c_str());verify(waiting(s,"inputJumpRelease"),"late old movie completion cannot perform pending jump");
 s=f.command("inputJumpReleased",(token(input(previous),"generation")+" "+token(input(previous),"jumpRelease")+" "+std::to_string(uint64_t(number(previous,"version")))+" 2400 1800").c_str());verify(waiting(s,"inputJumpRelease"),"stale release/render callback rejected");
 s=f.command("inputJumpReleased",(token(input(s),"generation")+" "+token(input(s),"jumpRelease")+" "+std::to_string(uint64_t(number(s,"version")))+" 2400 1800").c_str());verify(waiting(s,"transitionReady")&&!boolean(input(s),"active"),"current release ack permits only validated Core target");
 s=prepare(f,s,clock);verify(waiting(s,"input")&&!boolean(input(s),"enabled"),"movie cancellation preserves jump continuation");f.passed=true;
}
static void legacyInputDefaultsAll(){
 Fixture f(runtimeProgram());f.command("textSpeed","0");auto s=ack(f,f.command("action","start"));uint64_t clock=1000;s=prepare(f,s,clock);f.command("save","2");auto path=f.root/"savedata/slot2.whs";auto doc=mahoyo::save::read(path);
 for(int schema=3;schema<=18;++schema){
  std::ostringstream o;o<<"WOHHOST"<<schema<<"\n0\n0\n";
  if(schema>=7)o<<"PRELOADS1 0\n0\n0\n0\n0\n";
  if(schema>=8)o<<"WINDOWFADE2 1 65536 0\n";
  if(schema>=9)o<<"EFFECTFRAMES1 0\n";
  if(schema>=10)o<<"TEATIME1 0\nCOMMON1 0\n";
  if(schema>=12)o<<"GEOMETRYMODES1 0\n";
  if(schema>=13)o<<"WAVES1 0\n";
  if(schema>=14)o<<"LOCAL270CACHE1 0 0\n";
  if(schema>=15)o<<"FONTCARRY1 0\n";
  if(schema>=16)o<<"SCREENWAVE1 0 0\n";
  if(schema>=17){mahoyo::history::State h;h.legacy(doc.history.size());mahoyo::history::write(o,h,doc.history.size());}
  if(schema>=18)o<<"WAITBASELINE1 0 0\nEFFECTBASES1 0\n";
  auto bytes=o.str();doc.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(path,doc);
  woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());s=f.command("load","2");
  verify(waiting(s,"input")&&!boolean(input(s),"active")&&!boolean(input(s),"enabled")&&!boolean(input(s),"readyPending"),"WH3..18 migration inactive by default");
 }
 f.passed=true;
}
static void machineExternalJumpFrames(){
 struct Host:mahoyo::vm::Host{Program p{false};std::vector<uint8_t>loadModule(const std::string&)override{return p.build();}mahoyo::vm::Flow invoke(uint32_t op,mahoyo::vm::Machine&)override{check(op==0x120,"only synthetic yield");return mahoyo::vm::Flow::Yield;}} host;
 host.p.op(1);host.p.op(20);host.p.op(0x1a);host.p.op(0x120);host.p.op(0xf4);host.p.push(777);host.p.op(0x120);host.p.op(0x1b);
 mahoyo::vm::Machine m(host);m.start("synthetic.chs");m.run();verify(m.stack()==std::vector<int32_t>{777},"VM callframe setup");auto before=m.saveState();auto pc=m.pc();auto target=uint32_t(48+8*4);verify(m.externalJump(target)==pc&&m.pc()==target,"raw arena PC copied without adding header twice");auto after=m.saveState();verify(before.size()==after.size(),"external jump preserves payload shape");
 bool same=true;for(size_t i=0;i<before.size()-4;++i)if((i<8||i>=12)&&(i<24||i>=28))same=same&&before[i]==after[i];verify(same,"external jump does not alter frame/scope/memory/operands");
 m.run();verify(m.flow()==mahoyo::vm::Flow::Yield&&m.modulePc()==16&&m.stack()==std::vector<int32_t>{777},"jumped return uses preserved native callframe");
 for(uint32_t bad:{1u,52u,UINT32_MAX}){bool rejected=false;try{m.externalJump(bad);}catch(...){rejected=true;}verify(rejected,"bad or operand external target strict");}
}
static void pendingPrepare110And140(){
 for(bool dialogue:{false,true}){
  Program p;p.op(0x120);p.op(1);auto target=p.words.size();p.op(0);p.push(127);p.op(0x122);
  if(dialogue)p.dialogue(0);else{for(int n:{0,1,223800})p.push(n);p.op(0x110);}
  p.op(0xf4);p.words[target]=uint32_t(p.words.size()*4);p.op(0x121);p.push(-1);p.push(-1);p.op(0x122);p.commit();p.dialogue(1);p.op(0xf4);
  Fixture f(std::move(p));f.command("textSpeed","0");auto s=f.command("action","start");auto instructions=number(s,"instructions");
  verify(waiting(s,"inputRegionReady")&&!boolean(input(s),"enabled"),"new120 still requires actual UI-ready before122 executes");
  s=f.command("pause");s=ack(f,s);verify(waiting(s,"inputRegionReady")&&number(s,"instructions")==instructions,"paused120 ACK cannot advance VM");s=f.command("resume");
  auto r=input(s);auto stale=token(r,"generation")+" "+token(r,"request")+" "+std::to_string(uint64_t(number(s,"version"))-1)+" 2400 1800";
  s=f.command("inputUiReady",stale.c_str());verify(waiting(s,"inputRegionReady")&&number(s,"instructions")==instructions,"stale rendered version cannot start122 prepare");
  s=ack(f,s);verify(waiting(s,"transitionReady"),dialogue?"122-only140 requests real scene prepare":"122-only110 requests real scene prepare");
  verify(boolean(input(s),"enabled"),"110 and140 retain their existing prepare-time current commit");
  verify(number(input(s),"elapsedMs")==0&&!boolean(s,"canSave"),"prepare is not physical hold progress or a safe pending save");
  auto t=s.substr(s.find("\"transition\":"));auto id=uint64_t(number(t,"id"));instructions=number(s,"instructions");
  s=f.command("transitionReady",std::to_string(id+1).c_str());verify(waiting(s,"transitionReady")&&number(s,"instructions")==instructions,"stale scene ACK cannot finish122 prepare");
  s=f.command("tick","1000");verify(waiting(s,"transitionReady")&&number(s,"instructions")==instructions&&number(input(s),"elapsedMs")==0,"time alone before scene ACK cannot advance VM or invent physical hold");
  s=f.command("pause");s=f.command("transitionReady",std::to_string(id+1).c_str());verify(waiting(s,"transitionReady")&&number(s,"instructions")==instructions&&number(input(s),"elapsedMs")==0,"paused stale scene ACK leaves VM and hold progress alone");s=f.command("resume");
  s=f.command("transitionReady",std::to_string(id).c_str());verify(waiting(s,dialogue?"input":"frame")&&boolean(input(s),"enabled")&&token(input(s),"trigger")!="0","actual scene-ready completes registered122 and original wait");
  verify(number(s,"instructions")==instructions&&number(input(s),"elapsedMs")==0,"scene-ready does not invent hold or pass the blocking wait");
  s=f.command("inputPointer",pointer(s).c_str());s=f.command("tick","1017");verify(number(input(s),"elapsedMs")>0&&number(input(s),"elapsedMs")<=17,"prepared122 accepts genuine pointer state on the normal capped tick path");
  s=f.command("inputPointer",pointer(s,0).c_str());verify(number(input(s),"elapsedMs")==0&&number(s,"instructions")==instructions,"short release resets progress without jumping");
  auto oldPointer=pointer(s);s=f.command("pause");s=f.command("resume");s=f.command("inputPointer",oldPointer.c_str());s=f.command("tick","2000");verify(number(input(s),"elapsedMs")==0,"pre-pause gesture epoch remains stale after added prepare");
  f.passed=true;
 }
}
static void run(){pendingPrepare110And140();controllerNativeMath();runtimeAckSaveHold();footerAtomicAndLegacy();movieReleaseTransaction();legacyInputDefaultsAll();machineExternalJumpFrames();}
}
