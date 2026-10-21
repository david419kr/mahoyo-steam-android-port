// Selected script4 / Native2: ordinary voice slot0 lifetime with existing callbacks.
// Core fixtures use explicitly simulated backend events; no decoded audio/device.
static void nativeVoiceLinkedShakeMode4(){
 auto voice=[](Program& p,const char* name){p.push(0);p.string(name);p.op(0x1a0);};
 auto make=[&](bool voiced){Program p;p.image(1000,"fake",6);if(voiced)voice(p,"c20_nz3_0005");p.layerShake(6,5,5,100,-1,0,4);p.dialogue(0);voice(p,"c30_nz3_0005");p.dialogue(1);p.op(0xf4);return p;};
 {
  Fixture f(make(false),true);auto state=startGlobalPulse(f);check(boolean(state,"canSave")&&number(layerObject(state,6),"x")==150&&number(state,"nextTickMs")==0,"no ordinary voice cancels native2 immediately, no endless random fallback");f.passed=true;
 }
 {
  Fixture f(make(true),true);auto state=startGlobalPulse(f);auto id=std::to_string(uint64_t(number(audioChannel(state,"voice"),"id")));check(!boolean(state,"canSave")&&number(layerObject(state,6),"x")!=150,"queued ordinary voice counts before actual backend prepared/EOF");
  state=f.pulseTick("100000");check(!boolean(state,"canSave"),"no timer guesses voice completion in headless execution");auto moving=visibleLayers(state);f.command("pause");state=f.command("tick","900000");check(visibleLayers(state)==moving&&number(state,"nextTickMs")==0,"pause preserves pending voice and freezes existing local clock");f.command("resume");state=f.pulseTick("900000");
  state=f.command("audioEnded",("bgm "+id).c_str());check(!boolean(state,"canSave"),"BGM EOF does not finish normal-voice shake");state=f.command("audioEnded",("se0 "+id).c_str());check(!boolean(state,"canSave"),"SE0 is a different domain from normal voice0");state=f.command("audioEnded",("voice "+std::to_string(std::stoull(id)+500)).c_str());check(!boolean(state,"canSave"),"stale normal EOF ignored");
  state=f.command("advance");auto replacement=std::to_string(uint64_t(number(audioChannel(state,"voice"),"id")));check(replacement!=id&&!boolean(state,"canSave"),"new queued ordinary voice preserves slot predicate across replacement");
  state=f.command("audioEnded",("voice "+id).c_str());check(!boolean(state,"canSave"),"old EOF cannot finish replacement-linked shake");state=f.command("audioError",("voice "+id+" stale").c_str());check(!waiting(state,"error")&&!boolean(state,"canSave"),"old error cannot poison new voice");
  // This is an explicit deterministic backend event simulation, not a claim
  // that an original clip finished at the synthetic scheduler time.
  state=f.command("audioEnded",("voice "+replacement).c_str());check(boolean(state,"canSave")&&number(layerObject(state,6),"x")==150&&number(layerObject(state,6),"y")==150&&number(state,"nextTickMs")==0,"matching actual/simulated EOF reaches original cancel path immediately");f.command("save","2");auto d=mahoyo::save::read(f.root/"savedata/slot2.whs");check(currentHostSchema(d),"no new active-job schema");f.passed=true;
 }
 {
  Fixture f(make(true),true);auto state=startGlobalPulse(f);auto id=std::to_string(uint64_t(number(audioChannel(state,"voice"),"id")));state=woh_command(f.handle,"audioError",("voice "+id+" synthetic-decode-error").c_str());check(waiting(state,"error")&&state.find("Audio playback failed")!=std::string::npos&&number(state,"nextTickMs")==0,"current audio error stays genuine Core error, not guessed EOF");f.passed=true;
 }
 {
  Fixture f(make(true),true);historyAsset(f,"c20_nz3_0005");auto state=startGlobalPulse(f);state=f.command("history");auto rev=historyTokenValue(state,"historyRevision");state=f.command("historyReplay",(rev+" 0 1").c_str());check(replayObject(state)!="null"&&audioChannel(state,"voice").find("\"path\":\"\"")!=std::string::npos,"replay issues a real normal-slot stop and uses its own slot1");state=f.command("historyClose",rev.c_str());check(number(layerObject(state,6),"x")==150&&boolean(state,"canSave"),"history-only playback must not sustain normal-voice linked shake");f.passed=true;
 }
 for(int primary:{0,1,6}){
  Program p;p.image(1000,"fake",primary);p.dialogue(0);voice(p,"c20_nz3_0005");p.layerShake(primary,5,5,100,-1,0,4);p.commit();p.dialogue(1);p.op(0xf4);
  Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");g270Seed(f);uint64_t clock=1000;auto state=commitNext(f,clock);check(!boolean(state,"canSave"),"normal-voice Native2 job active before actual EOF");
  auto id=std::to_string(uint64_t(number(audioChannel(state,"voice"),"id")));state=f.command("audioEnded",("voice "+id).c_str());check(boolean(state,"canSave"),"EOF consumes Native2 primary/secondary job");f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");const auto cache=g270Cache(saved);check(cache==(primary<2?std::array<float,2>{0,0}:std::array<float,2>{19.25f,-37.5f}),"Native2 completion clears real primary cache only");
  auto wanted=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==wanted&&boolean(state,"canSave"),"completed Native2 restores canonical scene in fresh process");f.command("save","2");check(g270Cache(mahoyo::save::read(f.root/"savedata/slot2.whs"))==cache,"completed Native2 cache survives exact fresh roundtrip");f.passed=true;
 }
 for(int mode:{4}){Program p;p.image(1000,"fake",6);p.layerShake(-1,5,5,100,-1,0,mode);p.dialogue(0);Fixture f(std::move(p),true);auto state=std::string(woh_command(f.handle,"action","start"));check(state.find("Unsupported advanced shake variant")!=std::string::npos,"unproved negative-target4 remains strict");f.passed=true;}
}
