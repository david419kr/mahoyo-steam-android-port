// Synthetic244 contracts only; no installed assets, device, or user slots.
static void relative244Request(Program& p,int id,int dx,int dy,int duration=1000,int curve=0,int type=1,const char* anchor="CC"){
    p.string(anchor);for(int n:{curve,duration,dy,dx,id,type})p.push(n);p.op(0x244);
}
static void relative244Wait(Program& p,int id,int mask=63){for(int n:{1,mask,id})p.push(n);p.op(0x27f);}
static std::string relative244Ack(Fixture& f,const std::string& s){auto tail=s.substr(s.find("\"transition\":"));return f.command("transitionReady",std::to_string(uint64_t(number(tail,"id"))).c_str());}
static void relative244RuntimeCases(){
    {Program p;p.image(1000,"fake",20);p.axisScale(20,-1000,1000);p.origin(20,7,11);p.rotate(20,900);p.dialogue(0);
     relative244Request(p,20,93,0,1000,0,1,"TL");for(int n:{0,1000,0,INT32_MIN,20})p.push(n);p.op(0x245);relative244Wait(p,20);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start");check(waiting(before,"input"),"244 initial idle input");auto a=layerObject(before,20);
     auto state=f.command("advance");check(waiting(state,"transitionReady")&&!boolean(state,"canSave"),"244 plus245 uses existing prepared wait and busy save");auto preparedStart=layerObject(state,20);
     f.command("tick","999999");check(layerObject(woh_snapshot(f.handle),20)==layerObject(state,20),"244 no animation clock during resource acknowledgement");
     state=relative244Ack(f,state);check(waiting(state,"animation"),"244 participates in27f position wait");f.command("tick","10000");state=f.command("tick","10484");auto mid=layerObject(state,20);
     //Native float current=200/3 then ADDSS124 yields this exact delta.
     constexpr double nativeDelta=371.99997711181640625;
     check(std::abs(number(mid,"x")-number(preparedStart,"x")-nativeDelta*.5)<1e-6&&std::abs(number(mid,"alpha")-.5)<1e-9,"actual244 Native f32 endpoint and independent245 alpha fade share1000ms timeline");
     for(const char* key:{"w","h","rotation","pivotX","pivotY","parent"})check(number(mid,key)==number(a,key),"244 preserves negative dimensions/custom origin/rotation/parent");
     check(!boolean(state,"canSave"),"active244 motion cannot be saved");f.command("pause");f.command("tick","20000");check(layerObject(woh_snapshot(f.handle),20)==mid,"paused244 retains pose");
     f.command("resume");f.command("tick","30000");state=f.command("tick","30484");auto done=layerObject(state,20);
     check(waiting(state,"input")&&boolean(state,"canSave")&&std::abs(number(done,"x")-number(preparedStart,"x")-nativeDelta)<1e-6&&number(done,"alpha")==0,"244/fade complete together and restore idle-save eligibility");
     f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(layerObject(state,20)==done,"fresh WH19 restores completed244 canonical geometry with no pending queue");
     mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",host18Fixture(saved));state=f.command("load","2");check(layerObject(state,20)==done,"ordinary legacy18 geometry stays unchanged after244 completion");f.passed=true;}
    {Program p;p.image();p.position(0,51,51);p.dialogue(0);p.push(77);relative244Request(p,0,1,-1,0);relative244Request(p,0,1,-1,0);relative244Request(p,0,1,-1,0);p.push(0);p.op(0x251);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start"),a=layerObject(before,0);auto state=f.command("advance");check(waiting(state,"transitionReady"),"244 pending immediate change prepares through140");state=relative244Ack(f,state);auto after=layerObject(state,0);
     check(number(after,"x")==number(a,"x")+9&&number(after,"y")==number(a,"y")-9,"244 repeated f32 integer deltas accumulate, raw1 is3 design pixels");check(number(after,"z")==77&&boolean(state,"canSave"),"244 consumes exactly7 words and preserves lower operand for251");f.passed=true;}
    {Program p;p.image();p.position(0,100,50,1000);p.commit();p.dialogue(0);relative244Request(p,0,3,0,1000);p.push(500);p.push(500);p.op(0x111);relative244Wait(p,0,1);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");state=relative244Ack(f,state);f.command("tick","1000");state=f.command("tick","1000");check(waiting(state,"input"),"absolute legacy motion enters input after prepared scene");auto a=layerObject(state,0);
     state=f.command("advance");check(waiting(state,"frame"),"244 pending survives an asynchronous nonprepare111");f.command("tick","2000");state=f.command("tick","2484");check(waiting(state,"transitionReady"),"244 starts only when later27f prepares");auto prepared=layerObject(state,0);check(number(prepared,"x")>number(a,"x"),"old motion progresses before relative prepare replaces position category");
     state=relative244Ack(f,state);f.command("tick","4000");state=f.command("tick","4984");check(waiting(state,"input")&&std::abs(number(layerObject(state,0),"x")-number(prepared,"x")-12)<1e-6,"244 uses prepare-time current point rather than queue-time pose");f.passed=true;}
    {Program p;p.image();p.position(0,51,51);p.dialogue(0);relative244Request(p,0,33554432,0,0);relative244Request(p,0,100,100,1000,999,0);relative244Request(p,64,100,100);relative244Request(p,-1,100,100);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start");auto state=f.command("advance");state=relative244Ack(f,state);check(layerObject(state,0)==layerObject(before,0),"244 int32 wrap/type0 diagnostic/invalid indices preserve actual layer");f.passed=true;}
    {Program p;p.image();p.dialogue(0);relative244Request(p,0,3,0);p.push(0);p.op(0x241);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");auto state=f.command("advance");check(waiting(state,"input")&&boolean(state,"canSave")&&visibleLayers(state)=="[]","delete cancels pending244 without phantom motion/layer");f.passed=true;}
    {Program p;p.image();p.dialogue(0);relative244Request(p,0,3,0);p.push(100);p.push(100);p.op(0x111);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start");auto state=f.command("advance");check(!boolean(state,"canSave"),"unprepared244 is save-busy");f.command("title");state=f.command("action","start");check(layerObject(state,0)==layerObject(before,0)&&boolean(state,"canSave"),"title/new game clears pending244 and absolute-request markers");f.passed=true;}
    {Program p;p.image(1000,"fake",1);p.image(1000,"fake",20,{},1);p.position(20,51,51);p.dialogue(0);relative244Request(p,20,3,0,0);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start"),parent=layerObject(before,1),child=layerObject(before,20);auto state=f.command("advance");state=relative244Ack(f,state);
     check(layerObject(state,1)==parent&&number(layerObject(state,20),"parent")==1&&number(layerObject(state,20),"x")==number(child,"x")+12,"244 changes child-local position only and preserves parent/pool relationship");f.passed=true;}
    {Program p;p.image(1000,"fake",14);p.position(14,51,51);p.dialogue(0);relative244Request(p,14,-46,0,300);relative244Wait(p,14,1);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start"),a=layerObject(before,14);auto state=f.command("advance");state=relative244Ack(f,state);f.command("tick","1000");state=f.command("tick","1284");
     check(waiting(state,"input")&&number(layerObject(state,14),"x")==number(a,"x")-183,"actual8DOT5 negative46 truncates to negative61 internal, not raw negative184");f.passed=true;}
    {Program p;p.image();p.position(0,51,51);p.dialogue(0);relative244Request(p,0,3,0,1000);for(int n:{0,1,1500})p.push(n);p.op(0x110);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");auto before=f.command("action","start"),a=layerObject(before,0);auto state=f.command("advance");check(waiting(state,"transitionReady"),"pending244 alone crosses existing110 prepare barrier");state=relative244Ack(f,state);f.command("tick","1000");state=f.command("tick","1984");
     check(waiting(state,"frame")&&number(layerObject(state,0),"x")==number(a,"x")+12,"110 duration remains independent of position completion");state=f.command("tick","2484");check(waiting(state,"input")&&boolean(state,"canSave"),"110 keeps Native wait clock and idle save eligibility after244");f.passed=true;}
    {Program p;p.image(1000,"fake",20);p.position(20,51,51);p.dialogue(0);relative244Request(p,20,3,0,1000);p.clone(21,20,"");p.push(20);p.op(0x241);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true,"","fake\t100\t100\n");f.command("textSpeed","0");auto before=f.command("action","start"),a=layerObject(before,20);auto state=f.command("advance");auto clone=layerObject(state,21);
     check(waiting(state,"input")&&boolean(state,"canSave")&&number(clone,"x")==number(a,"x")+12,"clone reads relative terminal state, source delete clears only its pending position");f.passed=true;}
    {Program p;p.image();p.dialogue(0);p.position(0,51,50,1000);relative244Request(p,0,3,0);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");auto state=std::string(woh_command(f.handle,"advance",""));check(waiting(state,"error")&&state.find("mixed absolute/relative position before prepare")!=std::string::npos,"unreviewed absolute243 then relative244 same batch fails explicitly");f.passed=true;}
}
