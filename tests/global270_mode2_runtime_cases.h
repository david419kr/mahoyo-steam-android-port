#include "layer_shake.h"

static std::array<float,2> g270Cache(const mahoyo::save::Document& d){
 std::string extra(d.hostExtra.begin(),d.hostExtra.end());auto at=extra.find("LOCAL270CACHE1 ");check(at!=std::string::npos,"cache footer present");std::istringstream input(extra.substr(at));std::string key;std::array<float,2> pair;input>>key>>pair[0]>>pair[1];return pair;
}
static void g270Seed(Fixture& f){
 f.command("save","2");auto d=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(d.hostExtra.begin(),d.hostExtra.end());auto at=extra.find("LOCAL270CACHE1 ");check(at!=std::string::npos,"seed footer");auto end=extra.find('\n',at);extra.replace(at,end-at,"LOCAL270CACHE1 19.25 -37.5");d.hostExtra.assign(extra.begin(),extra.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",d);f.command("load","2");
}
static void global270Mode2AllProfiles(){
 for(auto p:std::vector<std::array<int,3>>{{18,120,360},{8,150,500},{14,120,600}}){
  Program program;for(int id:{0,1,8,9})program.image(1000,"fake",id);program.position(8,50,0);program.layerShake(-1,0,p[0],p[1],p[2],0,2);program.commit();program.dialogue(0);program.op(0xf4);
  Fixture f(std::move(program),true);auto start=startGlobalPulse(f);check(!boolean(start,"canSave"),"allmode2 active guard");auto state=f.pulseTick("1059");auto want=mahoyo::effects::sampleLayerShakeMode2({0,p[0],p[1],p[2]},75,{150,150,3,2.9779410362243652f});
  for(int id:{0,1,9})check(std::abs(number(layerObject(state,id),"y")-(150+double(want.y)*3))<1e-4,"allmode2 uses local sine per present owner");
  want=mahoyo::effects::sampleLayerShakeMode2({0,p[0],p[1],p[2]},75,{150,-50,3,2.9779410362243652f});check(std::abs(number(layerObject(state,8),"y")-(-50+double(want.y)*3))<1e-4,"allmode2 uses distinct projected geometry");
  auto text=state.substr(state.find("\"text\":"));check(number(text,"x")==300&&number(text,"y")==120,"allmode2 does not shake NVL text");f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"allmode2 active save rejected");state=f.pulseTick(std::to_string(1000+p[2]+100).c_str());check(boolean(state,"canSave")&&number(layerObject(state,9),"y")==150,"allmode2 all finite callbacks complete");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before&&boolean(state,"canSave"),"allmode2 completed fresh restore");f.passed=true;
 }
}
static void global270Mode2CacheCompleteCancel(){
 for(bool present:{false,true})for(bool cancel:{false,true}){
  Program p;if(present){p.image(1000,"fake",0);p.image(1000,"fake",1);}p.image(1000,"fake",8);p.dialogue(0);p.layerShake(-1,0,18,120,360,0,2);p.commit();p.dialogue(1);p.push(-1);p.op(0x272);p.commit();p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");g270Seed(f);uint64_t clock=1000;auto state=commitNext(f,clock);state=f.pulseTick(std::to_string(clock+59).c_str());clock+=59;
  check(!boolean(state,"canSave"),"seeded active allmode2 still unsavable");
  if(cancel)state=commitNext(f,clock);else state=f.pulseTick(std::to_string(clock+1000).c_str());check(boolean(state,"canSave"),"terminal allmode2 releases guard");auto before=visibleLayers(state);f.command("save","3");auto d=mahoyo::save::read(f.root/"savedata/slot3.whs");check(g270Cache(d)==(present?std::array<float,2>{0,0}:std::array<float,2>{19.25f,-37.5f}),"natural/cancel primary clears cache, missing primary preserves");
  woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","3");check(visibleLayers(state)==before&&boolean(state,"canSave"),"terminal cache saved state fresh restore");f.command("save","4");check(g270Cache(mahoyo::save::read(f.root/"savedata/slot4.whs"))==g270Cache(d),"fresh restore cache exact");f.passed=true;
 }
}
static void global270Mode2DelayWaitPause(){
 Program p;for(int id:{0,1,8})p.image(1000,"fake",id);p.layerShake(-1,0,18,120,360,400,2);for(int n:{1,128,-1})p.push(n);p.op(0x27f);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"allmode2 wait prepares");
 state=f.command("tick","900000");check(number(layerObject(state,8),"y")==150,"pending ready holds phase");state=f.command("transitionReady","1");check(waiting(state,"animation"),"finite allmode2 participates in mask128");f.command("tick","1000");for(int n=2;n<=8;++n){state=f.command("tick",std::to_string(n*1000).c_str());check(number(layerObject(state,8),"y")==150&&waiting(state,"animation"),"allmode2 delay capped50ms");}
 state=f.command("tick","9000");check(number(layerObject(state,8),"y")==150,"allmode2 begins sinephase0");state=f.command("tick","9050");auto frozen=visibleLayers(state);f.command("pause");state=f.command("tick","990000");check(visibleLayers(state)==frozen,"allmode2 pause frozen");f.command("resume");f.command("tick","990000");for(int i=1;i<=8;++i)state=f.command("tick",std::to_string(990000+i*50).c_str());check(waiting(state,"input")&&boolean(state,"canSave"),"allmode2 wait releases only after actual callbacks");f.passed=true;
}
