// Synthetic API-only regressions for the bounded nonvisual253 transaction.
// Uses the existing Program/Fixture/JSON and WH19 legacy-fixture helpers.
#pragma once
namespace color253_pending_cases {
static void request(Program& p,int id,int mode,int rgb=0){p.push(rgb);p.push(mode);p.push(id);p.op(0x253);}
static std::string settleColorInput(Fixture& f,std::string state){
    for(int n=0;n<100;++n){
        if(waiting(state,"input")&&!boolean(state,"revealing"))return state;
        if(waiting(state,"transitionReady")){auto at=state.find("\"transition\":");check(at!=std::string::npos,"pending color prepare descriptor");state=f.command("transitionReady",std::to_string(uint64_t(number(state.substr(at),"id"))).c_str());}
        else state=f.command("tick",std::to_string((n+1)*1000).c_str());
    }
    throw std::runtime_error("Pending color fixture did not reach input");
}
static std::string startColorInput(Fixture& f){f.command("textSpeed","0");return settleColorInput(f,f.command("action","start"));}
static bool absent(const std::string& state,int id){return visibleLayers(state).find("\"id\":"+std::to_string(id)+",")==std::string::npos;}
static std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
static void exactImage(Program& p,int id,const std::string& name,int x,int y,int alpha,int z,int parent,std::array<int,4> window={}){
    std::array<int,9> rest{{0,0,parent,window[0],window[1],window[2],window[3],0,0}};
    for(auto i=rest.rbegin();i!=rest.rend();++i)p.push(*i);
    p.string("CC");for(int n:{0,z,alpha,1000,y,x})p.push(n);p.string(name);p.push(id);p.op(0x240);
}
static void actualForwardReset(){
    Program p;exactImage(p,1,"img0125",-3,-121,0,4850,-1,{{0,0,1920,1080}});p.rotate(1,0);p.axisScale(1,2000,2000);
    request(p,21,52);exactImage(p,21,"sou_n_12_18_07",263,114,255,5410,1);
    for(int n:{0,0,0,0,1500,1500,21})p.push(n);p.op(0x247);p.push(1);p.push(21);p.op(0x25a);p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(p);f.imageHeader("img0125",100,100);f.imageHeader("sou_n_12_18_07",100,100);auto s=startColorInput(f);auto l=layerObject(s,21),other=layerObject(s,1);
    check(number(l,"colorMode")==0&&number(l,"rgb")==0,"forward25321 is reset by normal24021 before25a/commit");
    check(number(other,"colorMode")==0&&number(other,"alpha")==0,"actual owner21 is not silently changed to1");
    check(number(l,"alpha")==1&&boolean(s,"canSave"),"resolved child transaction is saveable");f.passed=true;
}
static void repeatAndOtherLive(){
    Program p;p.image(1000,"fake",1);request(p,21,52,0x111111);request(p,21,53,0xabcdef);request(p,1,7,0x123456);p.image(1000,"fake",21);p.dialogue(0);p.op(0xf4);
    Fixture f(p,true);auto s=startColorInput(f);check(number(layerObject(s,1),"colorMode")==7&&number(layerObject(s,1),"rgb")==0x123456,"other existing live253 is preserved");
    check(number(layerObject(s,21),"colorMode")==0&&number(layerObject(s,21),"rgb")==0,"repeat pending does not leak through normal240 reset");check(boolean(s,"canSave"),"repeated pending resolved");f.passed=true;
}
static void boundaryAndDelete(){
    for(int erase:{63,-1}){Program p;p.image(1000,"fake",1);request(p,63,52);request(p,64,52);request(p,-1,52);request(p,INT32_MIN,52);request(p,INT32_MAX,52);p.push(erase);p.op(0x241);if(erase<0)p.image(1000,"fake",1);p.dialogue(0);p.op(0xf4);Fixture f(p,true);auto s=startColorInput(f);check(absent(s,63)&&absent(s,64)&&absent(s,-1),"63 pending and invalid IDs never create graphics");check(boolean(s,"canSave"),"single/all deletion clears pending savebusy");f.passed=true;}
}
static void normalRecreate(){Program p;p.image(1000,"fake",21);request(p,21,52,0x123456);p.image(1000,"fake",21);p.dialogue(0);p.op(0xf4);Fixture f(p,true);auto s=startColorInput(f);check(number(layerObject(s,21),"colorMode")==0&&number(layerObject(s,21),"rgb")==0,"existing normal240 recreation resets color");f.passed=true;}
static void wh19AndLegacyBusy(){
    for(int schema:{17,18,19}){Program p;p.image(1000,"fake",1);p.dialogue(0);request(p,21,52);p.op(0xf4);Fixture f(p,true);startColorInput(f);f.command("save","2");const auto slot=f.root/"savedata/slot2.whs";auto document=mahoyo::save::read(slot);check(currentHostSchema(document),"pending color adds no WH schema field");if(schema==17)document=host17Fixture(document);else if(schema==18)document=host18Fixture(document);mahoyo::save::writeAtomic(slot,document);const auto before=bytes(slot);
        f.command("advance");auto s=f.command("tick","4000");check(waiting(s,"end")&&absent(s,21)&&!boolean(s,"canSave"),"unresolved record stays nonvisual and unsaveable");s=f.command("save","2");check(bytes(slot)==before,"busy save preserves current/legacy slot bytes");s=f.command("load","2");check(waiting(s,"input")&&boolean(s,"canSave")&&absent(s,21),"normal WH19/legacy restore has empty transient pending bank");f.passed=true;
    }
}
static void newGameAndTitleClear(){
    for(bool useTitle:{false,true}){Program p;p.image(1000,"fake",1);request(p,21,52);p.op(0xf4);Fixture f(p,true);f.command("action","start");if(useTitle)f.command("title");Program fresh;fresh.image(1000,"fake",1);fresh.dialogue(0);fresh.op(0xf4);auto data=fresh.build();{std::ofstream out(f.game/"scripts/1_0.chs",std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));check(bool(out),"rewrite own synthetic script only");}auto s=startColorInput(f);check(boolean(s,"canSave")&&absent(s,21),"newgame/title clears transient record");f.passed=true;}
}
static void unresolvedGuards(){
    for(bool clone:{false,true}){Program p;p.image(1000,"fake",1);request(p,21,52);if(clone)p.clone(22,1,"fake");else p.commit();p.dialogue(0);p.op(0xf4);Fixture f(p,true);std::string s=woh_command(f.handle,"action","start");check(waiting(s,"error")&&s.find("Unsupported unresolved nonvisual color request")!=std::string::npos,"unreviewed prepare/clone is failclosed rather than blindNOP");check(absent(s,21)&&absent(s,22),"unresolved failure creates no pending/clone graphics");f.passed=true;}
}
static void unsupportedModeAndEmptyCreate(){
    for(bool empty:{false,true}){Program p;p.image(1000,"fake",1);request(p,21,empty?52:83);if(empty)p.image(1000,"",21);p.op(0xf4);Fixture f(p,true);std::string s=woh_command(f.handle,"action","start");check(waiting(s,"error")&&s.find(empty?"Unsupported nonvisual color reset without an image":"Unsupported color mode 83")!=std::string::npos,"unreviewed color/empty create remains strict");check(absent(s,21),"unsupported input creates no phantom layer");f.passed=true;}
}
static void run(){actualForwardReset();repeatAndOtherLive();boundaryAndDelete();normalRecreate();wh19AndLegacyBusy();newGameAndTitleClear();unresolvedGuards();unsupportedModeAndEmptyCreate();}
}
