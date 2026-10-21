// Synthetic child17flag1/window fixtures only; no game pixels or device/save access.
static void nativeChild17Flag1CaptureCloneAndRestore(){
    for(int rawFlag:{-1,0,1,2}){
        Program p;p.image(1000,"fake",30,{{0,0,1920,1080,0,0}});p.image(1000,"fake",31,{{0,0,1920,1080,0,0}});p.image(1000,"fake",34,{},31);p.image(1000,"fake",35);p.alpha(34,160);
        p.push(16777170);p.push(1);p.push(34);p.op(0x253);p.blend(34,17,rawFlag);p.axisScale(34,1600,1600);p.rotate(34,-1700);p.commit();p.dialogue(0);p.clone(36,34,"fake");p.dialogue(1);p.blend(34,0);for(int n:{0,256,0,0,300,0})p.push(n);p.op(0x200);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));if(waiting(state,"frame"))state=f.command("tick","1000");auto child=layerObject(state,34);
        check(number(child,"parent")==31&&number(child,"blend")==17&&number(child,"blendFlag")==(rawFlag==1?1:0),"child17flag1 normalizes raw-1/0/1/2 with only1 becoming flag1");
        check(number(child,"colorMode")==1&&number(child,"rgb")==16777170&&number(child,"groupOpacityByte")==159,"child17flag1 preserves actual tint and160->159 native opacity");
        auto before=visibleLayers(state);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh restore retains child17flag1 graph/tint/geometry/opacity");
        std::vector<mahoyo::save::Document> invalid;
        invalid.push_back(editSavedLayer(good,34,[](auto& row){row[14]="2";}));
        invalid.push_back(editSavedLayer(good,34,[](auto& row){row[21]="1920";row[22]="1080";}));
        invalid.push_back(editSavedLayer(good,31,[](auto& row){row[21]="0";row[22]="0";}));
        invalid.push_back(editSavedLayer(good,31,[](auto& row){row[25]="30";}));
        invalid.push_back(editSavedLayer(good,35,[](auto& row){row[25]="34";}));
        for(const auto& bad:invalid){mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==before,"invalid child17flag1 flag/window/nesting rejects atomically");}
        mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);f.command("load","2");state=f.command("advance");auto clone=layerObject(state,36);
        check(number(clone,"blend")==17&&number(clone,"blendFlag")==(rawFlag==1?1:0)&&number(clone,"parent")==31&&number(clone,"rgb")==16777170,"clone retains child17flag1 parent/flag/tint");
        state=f.command("advance");check(waiting(state,"transitionReady"),"child17flag1 capture requires graphics acknowledgement");auto old=state.substr(state.find("\"old\":{\"scene\""));check(number(layerObject(state,34),"blend")==0&&number(layerObject(old,34),"blend")==17,"capture retains old child17flag1 blend independently");f.passed=true;
    }
    Program root;root.image(1000,"fake",5);root.blend(5,17,1);root.dialogue(0);root.op(0xf4);Fixture ordinary(std::move(root),true);ordinary.command("textSpeed","0");auto state=ordinary.command("action","start");check(number(layerObject(state,5),"blendFlag")==1,"child17 extension preserves root17flag1");ordinary.passed=true;
    Program wave;wave.image(1000,"fake",26,{{0,0,1920,1080,0,0}});wave.image(1000,"fake",5,{},26);wave.blend(5,17,1);wave.wave(5,6);wave.waitWave(5);wave.dialogue(0);wave.op(0xf4);Fixture waved(std::move(wave),true);state=woh_command(waved.handle,"action","start");check(waiting(state,"transitionReady"),"child17flag1 wave waits for graphics-ready before strict validation");auto transition=state.substr(state.find("\"transition\":"));state=woh_command(waved.handle,"transitionReady",std::to_string(uint64_t(number(transition,"id"))).c_str());check(state.find("\"error\":\"Unsupported parented wave:")!=std::string::npos,"normal child17flag1 support does not admit unreviewed parented wave");waved.passed=true;
}
static void nativeChild17Flag1EffectPropertyGate(){
    for(int flag:{0,1}){
        Program p;p.image(1000,"fake",26,{{0,0,1920,1080,0,0}});p.image(1000,"fake",5,{},26);p.blend(5,0,flag);for(int n:{0,0,0,915,5})p.push(n);p.op(0x278);p.dialogue(0);p.op(0xf4);
        const std::string table="915 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n915 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n";
        const std::string windows="915 0 0 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n";
        Fixture f(std::move(p),true,table,"","",windows);woh_destroy(f.handle);auto path=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(path);{std::ofstream out(path);for(int i=0;i<2;++i){out<<"915 "<<i<<' '<<i<<" 0x2000000";for(int n=0;n<13;++n)out<<' '<<(n==2?17:-32767);out<<'\n';}}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));
        check(number(layerObject(state,5),"blend")==17&&number(layerObject(state,5),"blendFlag")==flag,"effect property selects reviewed child17flag0/1");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"fresh active-effect restore retains child17flag");f.passed=true;
    }
}
