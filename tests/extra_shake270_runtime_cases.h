// Synthetic CPU runtime integration. Original native7 gold is recorded by
// tools/host_reverse_extra270.py; this is not device/GPU validation.
#include "layer_shake.h"
static void nativeExtra270SquareMode3(){
 using namespace mahoyo::effects;
 const LayerShakeMode2 exact{0,1,150,3000};const LayerShakeGeometry plain;
 for(uint64_t t:{0,1,74,75,76,149,150,225,2999}){
  const auto got=sampleLayerShakeMode3(exact,t,plain);
  const float want=(t%150>=75)?0.7777777910232544f:0.f;
  check(got.active&&got.x==0&&got.y==want,"native7 exact half-period step and no finite decay");
 }
 check(!sampleLayerShakeMode3(exact,3000,plain).active&&!sampleLayerShakeMode3(exact,75,plain,true).active,"native7 finite endpoint/cancel resets local offset");
 check(sampleLayerShakeMode3({-4,-5,150,-1},225,plain).x==-2.5f,"native7 negative axes and indefinite phase");
 check(sampleLayerShakeMode3(exact,75,plain).y!=sampleLayerShakeMode2(exact,75,plain).y,"native7 cannot alias native6 finite envelope");
 for(int mode:{2,3}){
  Program p;p.image(1000,"fake",19);p.image(1000,"fake",23);p.push(77);p.layerShake(19,0,1,150,3000,0,mode);p.push(19);p.op(0x251);
  p.push(0);p.push(0);p.op(0x1a0); // Voice operand contract; no audio asset needed.
  p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");
  check(waiting(state,"transitionReady")&&!boolean(state,"canSave")&&number(state,"nextTickMs")==0,"270 -> voice -> 140 prepares local shake without explicit200/110");
  state=f.command("tick","100000");check(number(layerObject(state,19),"y")==150,"pending140 pulse consumes no loading time");
  auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));f.command("transitionReady",id.c_str());f.pulseTick("100000");
  check(number(layerObject(state,19),"z")==77,"native270 consumes seven operands with no pushed result");
  state=f.pulseTick("100058");if(mode==3)check(number(layerObject(state,19),"y")==150,"native7 still low at74ms");
  state=f.pulseTick("100059");const auto want=mode==3?sampleLayerShakeMode3(exact,75,{150,150,3,2.9779410362243652f}):sampleLayerShakeMode2(exact,75,{150,150,3,2.9779410362243652f});
  check(std::abs(number(layerObject(state,19),"y")-(150+double(want.y)*3))<1e-5,"ACK starts exact selected-layer native phase and geometry");
  check(number(layerObject(state,23),"y")==150,"selected pulse leaves other image unmodified");
  f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"active pulse remains unsavable without schema extension");
  if(mode==3){state=f.pulseTick("100133");check(number(layerObject(state,19),"y")>154,"native7 constant amplitude through149ms");state=f.pulseTick("100134");check(number(layerObject(state,19),"y")==150,"native7 period wraps to exact zero");}
  auto frozen=visibleLayers(state);f.command("pause");state=f.command("tick","900000");check(visibleLayers(state)==frozen&&number(state,"nextTickMs")==0,"pause excludes wall-clock debt");f.command("resume");f.pulseTick("900000");state=f.pulseTick("904000");
  check(boolean(state,"canSave")&&number(layerObject(state,19),"y")==150,"finite pulse ends at canonical rect and releases save guard");
  f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");check(currentHostSchema(saved),"completed native7 needs no new WH schema");auto expected=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==expected&&boolean(state,"canSave"),"fresh runtime restores completed native7 scene and portable cache");
  mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",host18Fixture(saved));state=f.command("load","2");check(visibleLayers(state)==expected&&boolean(state,"canSave"),"real legacy18 conversion still restores stable native7 completion");f.passed=true;
 }
 for(int primary:{0,1,19})for(bool cancel:{false,true}){
  Program p;p.image(1000,"fake",primary);p.dialogue(0);p.layerShake(primary,0,1,150,cancel?-1:300,0,3);p.commit();p.dialogue(1);p.push(primary);p.op(0x272);p.commit();p.dialogue(0);p.op(0xf4);
  Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");g270Seed(f);uint64_t clock=1000;auto state=commitNext(f,clock);state=f.pulseTick(std::to_string(clock+59).c_str());clock+=59;
  check(number(layerObject(state,primary),"y")>154&&!boolean(state,"canSave"),"primary/follower square job is active before completion or cancel");
  state=cancel?commitNext(f,clock):f.pulseTick(std::to_string(clock+400).c_str());check(boolean(state,"canSave")&&number(layerObject(state,primary),"y")==150,"selected272 or finite completion resets square offset");
  f.command("save","2");const auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");check(g270Cache(saved)==(primary<2?std::array<float,2>{0,0}:std::array<float,2>{19.25f,-37.5f}),"only real primary native7 terminal/cancel callback clears shared cache");f.passed=true;
 }
 for(auto profile:std::vector<std::pair<int,int>>{{-1,3},{19,7},{19,8}}){Program p;p.image(1000,"fake",19);p.layerShake(profile.first,0,1,150,3000,0,profile.second);p.dialogue(0);Fixture f(std::move(p),true);auto state=std::string(woh_command(f.handle,"action","start"));check(state.find("Unsupported advanced shake variant")!=std::string::npos,"global3 and unproved script7/8 remain strict");f.passed=true;}
}
