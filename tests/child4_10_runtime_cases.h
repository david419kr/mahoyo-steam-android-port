// Private synthetic fixture only: no original pixels, Stage, device or user saves.
static Program nativeChild4Or10Body(int mode,int raw=-1,int invalidContext=0){
 const int parent=mode==10?2:6,child=mode==10?8:19;
 Program p;p.image(1000,"img0408",parent,invalidContext==1?std::array<int32_t,6>{}:std::array<int32_t,6>{0,0,1920,1080,0,0});
 p.image(1000,mode==10?"img1214":"img1767",child,invalidContext==2?std::array<int32_t,6>{0,0,16,16,0,0}:std::array<int32_t,6>{},parent);
 p.push(mode==10?1:10);p.push(mode==10?1:0);p.push(child);p.op(0x256);
 p.alpha(child,mode==10?192:255);p.blend(child,mode,raw);
 if(invalidContext==3)p.image(1000,"fake",25,{},child);
 if(invalidContext==4)p.wave(child,1);
 p.waitWave(child);p.dialogue(0);p.clone(child+1,child,mode==10?"img1214":"img1767");p.dialogue(1);p.op(0xf4);return p;
}
static void prepareNativeChild4Or10Images(Fixture&f){for(const char* n:{"img0408","img1214","img1214_01_01","img1767","img1767_00_10","fake"})f.imageHeader(n,96,64);}
static void nativeChild4Or10DirectRestore(){
 for(int mode:{4,10})for(int raw:{-1,0,2}){
  int parent=mode==10?2:6,child=mode==10?8:19;
  Fixture f(nativeChild4Or10Body(mode,raw),false,"","img0408 48 32\nimg1214 48 32\nimg1767 48 32\nfake 48 32\n");prepareNativeChild4Or10Images(f);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));
  auto c=layerObject(state,child);check(number(c,"blend")==mode&&number(c,"blendFlag")==0&&number(c,"parent")==parent,"reviewed child4/10 normalized flag0 context");
  check(c.find(mode==10?"img1214_01_01.mzp":"img1767_00_10.mzp")!=std::string::npos,"full requested variant retained");
  auto before=visibleLayers(state);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh restore preserves reviewed child4/10");
  for(int flag:{-1,1,2}){auto bad=editSavedLayer(good,child,[=](auto&row){row[14]=std::to_string(flag);});mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==before,"malformed child4/10 saved flags rejected atomically");}
  auto bad=editSavedLayer(good,parent,[](auto&row){row[21]="0";row[22]="0";});mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==before,"no-window parent restore rejects atomically");
  mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);state=f.command("load","2");state=f.command("advance");auto clone=layerObject(state,child+1);check(number(clone,"blend")==mode&&number(clone,"parent")==parent&&number(clone,"blendFlag")==0,"clone preserves reviewed child4/10 material");f.passed=true;
 }
 for(int mode:{4,10})for(int invalid=0;invalid<=4;invalid++){
  Fixture f(nativeChild4Or10Body(mode,invalid==0?1:0,invalid));prepareNativeChild4Or10Images(f);auto rejected=std::string(woh_command(f.handle,"action","start"));
  if(waiting(rejected,"transition")){auto t=rejected.substr(rejected.find("\"transition\":"));rejected=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(t,"id"))).c_str());}
  check(rejected.find("Unsupported")!=std::string::npos,"flag1/no-parent-window/own-window/nesting/wave stays strict");f.passed=true;
 }
}
static void nativeChild4Or10Effect(){
 const std::string table="1\t0\t0\t0\t224\t1000\t1000\t-100001\t0\t0\t0\t1000\n1\t1000\t0\t0\t224\t1000\t1000\t-100001\t0\t0\t0\t1000\n";
 for(int mode:{4,10})for(int flag:{0,1}){
  Program p;p.image(1000,"img0408",6,{{0,0,1920,1080,0,0}});p.image(1000,"fake",19,{},6);p.blend(19,0,flag);p.waitWave(19);for(int n:{0,0,0,1,19})p.push(n);p.op(0x278);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,table);f.imageHeader("img0408",96,64);woh_destroy(f.handle);
  std::ostringstream props;for(int frame:{0,1})props<<"1\t"<<frame<<"\t"<<frame<<"\t0x2000000\t-32767\t-32767\t"<<mode<<"\t-32767\t-32767\t-32767\t-32767\t-32767\t-32767\t-32767\t-32767\t-32767\t-32767\n";
  auto path=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(path);{std::ofstream out(path);out<<props.str();}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));auto t=state.substr(state.find("\"transition\":"));state=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(t,"id"))).c_str());
  if(flag)check(state.find("Unsupported effect blend mode "+std::to_string(mode))!=std::string::npos,"effect cannot bypass child flag1 gate");else{check(number(layerObject(state,19),"blend")==mode,"effect selects reviewed child4/10");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"fresh active effect child4/10 restore");}f.passed=true;
 }
}
static void nativeChild4Or10RoutesAndRestore(){nativeChild4Or10DirectRestore();nativeChild4Or10Effect();}
