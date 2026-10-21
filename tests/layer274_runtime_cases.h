// Synthetic opcode274 / Native5 integration, plus retained original CPU gold.
// Actual Archive source is NZ3:2252, selected18; no device/GPU equivalence claim.
#include "layer_shake.h"
static void native274PersistentMotionAndClear(){
 for(int id:{0,1,18}){
  Program p;p.image(1000,"fake",id);p.dialogue(0);p.push(id);p.op(0x274);p.commit();p.dialogue(1);p.push(id);p.op(0x272);p.commit();p.dialogue(0);p.op(0xf4);
  Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");g270Seed(f);uint64_t clock=1000;auto state=commitNext(f,clock);
  check(!boolean(state,"canSave")&&number(state,"nextTickMs")==16,"274 creates an indefinite active job only after actual scene ACK");
  for(int n=0;n<20;++n)state=f.command("tick",std::to_string(clock+=17).c_str());
  check(!boolean(state,"canSave")&&(number(layerObject(state,id),"x")!=150||number(layerObject(state,id),"y")!=150),"274 is a moving oscillator, not a no-op or finite timer");
  f.command("save","3");check(!fs::exists(f.root/"savedata/slot3.whs"),"274 packed transient job remains save-busy");
  state=commitNext(f,clock);check(boolean(state,"canSave")&&number(state,"nextTickMs")==0&&number(layerObject(state,id),"x")==150&&number(layerObject(state,id),"y")==150,"272 clears274 local motion and stops idle polling");
  f.command("save","3");auto saved=mahoyo::save::read(f.root/"savedata/slot3.whs");check(currentHostSchema(saved)&&g270Cache(saved)==(id<2?std::array<float,2>{0,0}:std::array<float,2>{19.25f,-37.5f}),"274 cancel clears primary cache only; no WH schema expansion");
  auto expected=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","3");check(visibleLayers(state)==expected&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"completed274 fresh-process restore is canonical and idle");f.passed=true;
 }
}
static void native274ClockRngAndGuard(){
 using namespace mahoyo::effects;
 LayerMotion274 job;unsigned seed=1,draws=0;auto rng=[&](){++draws;seed=seed*214013u+2531011u;return(seed>>16)&32767u;};
 for(unsigned frame=0;frame<4;++frame){auto got=job.sample(frame,rng);check(got.active&&draws==6,"Native5 initial two sign+four range draws; held frames draw nothing");if(frame<3)check(got.x==0&&got.y==0,"Native5 initial Y-gated held output");else check(got.x==-1.4758806228637695f&&got.y==.321958065032959f,"Native5 first changed exact original float gold");auto again=job.sample(frame,rng);check(again.x==got.x&&again.y==got.y&&draws==6,"one Core tick has one Native callback despite repeated JSON/command sampling");}
 auto canceled=job.sample(3,rng,true);check(!canceled.active&&draws==6&&!job.sample(10000000,rng).active,"cancel terminates even on duplicate tick and cannot revive");
 job.reset();auto restarted=job.sample(0,rng);check(restarted.active&&draws==12,"replacement gets a fresh packed state with shared RNG continuation");
 Program p;p.image(1000,"fake",18);p.push(18);p.op(0x274);p.dialogue(0);p.push(18);p.op(0x272);p.commit();p.dialogue(1);p.op(0xf4);
 Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0,"274 pending waits on real scene-ready barrier");auto before=visibleLayers(state);state=f.command("tick","100000");check(visibleLayers(state)==before,"loading time neither draws random nor advances274");
 auto tail=state.substr(state.find("\"transition\":"));auto ack=std::to_string(int(number(tail,"id")));f.command("transitionReady",ack.c_str());uint64_t t=100000;state=f.command("tick",std::to_string(t).c_str());for(int n=0;n<8;++n)state=f.command("tick",std::to_string(t+=17).c_str());
 before=visibleLayers(state);f.command("pause");state=f.command("tick","900000");check(visibleLayers(state)==before&&number(state,"nextTickMs")==0,"pause suppresses callback-count motion and its wake");f.command("resume");state=f.command("tick","900000");check(!boolean(state,"canSave"),"wallclock jumps do not complete fixed duration-1");
 // Toggle skip and disable it before a scheduler tick; the native274 job has
 // no finite completion predicate and must survive this flag change.
 f.command("skip");state=f.command("skip");check(!boolean(state,"canSave"),"skip cannot complete duration-1 native5");f.passed=true;
}
static void native274StackAndBounds(){
 for(int id:{-1,64,INT32_MIN,INT32_MAX}){
  Program p;p.image(1000,"fake",18);p.push(77);p.push(id);p.op(0x274);p.push(18);p.op(0x251);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);auto state=f.firstComplete();check(number(layerObject(state,18),"z")==77&&boolean(state,"canSave"),"274 consumes exactly one value, pushes nothing, invalid unsigned layer is ignored");f.passed=true;
 }
 {Program p;p.image(1000,"fake",18);p.push(63);p.op(0x274);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);auto state=startGlobalPulse(f);check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"missing graphics target makes no callback/RNG or permanent busy job");f.passed=true;}
 {Program p;p.op(0x274);Fixture f(std::move(p),true);auto state=std::string(woh_command(f.handle,"action","start"));check(waiting(state,"error"),"274 missing operand retains VM stack-underflow failure");f.passed=true;}
 for(int mode:{7,8}){Program p;p.image(1000,"fake",18);p.layerShake(18,1,1,20,-1,0,mode);p.dialogue(0);Fixture f(std::move(p),true);auto state=std::string(woh_command(f.handle,"action","start"));check(state.find("Unsupported advanced shake variant")!=std::string::npos,"internal274 tag does not make new raw270 modes valid");f.passed=true;}
}
