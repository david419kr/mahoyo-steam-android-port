// Synthetic37..40 profile/DTO/save fixtures; no game assets or user slots.
static Program nativeWaveNextProgram(int preset,int wrong=0){
 const int blend[]={0,20,11,17},color[]={0,39,37,0};Program p;
 p.image(1000,"grid",45);p.position(45,231,114);p.alpha(45,preset==40?0:preset==38?64:preset==39?32:255);
 for(int n:{0,wrong==2?(color[preset-37]==0?1:0):color[preset-37],45})p.push(n);p.op(0x253);
 p.blend(45,wrong==1?1:blend[preset-37],wrong==3?(preset==40?0:1):preset==40?1:0);p.axisScale(45,preset==40?2000:1000,preset==40?2000:preset==37?1000:1100);p.rotate(45,preset==40?-40000:preset==37?0:-14700);
 p.wave(45,preset);p.waitWave(45);p.dialogue(0);p.push(0);p.push(45);p.op(0x25a);p.dialogue(1);p.push(1);p.push(45);p.op(0x25a);p.dialogue(0);p.wave(45,0);p.waitWave(45);p.dialogue(0);p.op(0xf4);return p;
}
static void nativeWaveNextProfilesRestoreEnabled(){
 const int w[]={2042,1022,1022,2552},h[]={1780,1526,1526,2288};
 for(int preset=37;preset<=40;++preset){Fixture f(nativeWaveNextProgram(preset),false,"","grid 100 100\n");f.imageHeader("grid",w[preset-37],h[preset-37]);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));
  check(hasWave(state,45)&&number(waveObject(state,45),"preset")==preset,"exact new Native profile accepted");
  check((visibleLayers(state).find("\"enabled\":true")!=std::string::npos)==(preset==40),"enabledDTO only new40 nodes");
  woh_wave_clock(f.handle,1000);woh_snapshot(f.handle);woh_wave_clock(f.handle,1250);state=woh_snapshot(f.handle);check(number(waveObject(state,45),"elapsedMs")==250,"alpha0 source clock independent");auto before=visibleLayers(state);
  f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");check(currentHostSchema(saved),"WH18 unchanged");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh exact wave phase/DTO restore");
  state=f.command("advance");if(preset==40)check(visibleLayers(state).find("\"enabled\":false")!=std::string::npos&&number(layerObject(state,45),"alpha")==0,"25a disabled distinct from alpha0");
  f.command("save","2");before=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"existing saved enabled flag survives fresh restore");
  state=f.command("advance");if(preset==40)check(visibleLayers(state).find("\"enabled\":true")!=std::string::npos,"25a reenabled40");
  state=f.command("advance");if(waiting(state,"transitionReady"))state=acknowledge(f,state);check(!hasWave(state,45)&&visibleLayers(state).find("\"enabled\":")==std::string::npos,"cancel removes only40DTO with controller");f.passed=true;
 }
 for(int preset=37;preset<=40;++preset)for(int wrong=1;wrong<=3;++wrong){Fixture f(nativeWaveNextProgram(preset,wrong),false,"","grid 100 100\n");f.imageHeader("grid",1022,764);auto state=std::string(woh_command(f.handle,"action","start"));if(waiting(state,"transitionReady")){auto transition=state.substr(state.find("\"transition\":"));state=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(transition,"id"))).c_str());}check(state.find("Unsupported wave37..40 Native context")!=std::string::npos,"new wrongprofile rejects");f.passed=true;}
}
