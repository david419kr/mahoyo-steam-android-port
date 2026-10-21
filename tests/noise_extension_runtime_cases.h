//Synthetic metadata and dimension headers only. Include after the common Runtime Fixture helpers.
namespace noise_extension_cases {
static void fresh(Fixture& f){woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());check(f.handle!=0,"noise fixture recreated");f.command("textSpeed","0");}
static void file(Fixture& f,const char* relative,const std::string& text){
    auto target=f.game/relative;fs::create_directories(target.parent_path());f.extraFiles.push_back(target);
    std::ofstream out(target,std::ios::binary);out<<text;check(bool(out),"write synthetic noise metadata");
}
static void module(Fixture& f,const char* name,Program p){
    auto target=f.game/"scripts"/name;f.extraFiles.push_back(target);auto bytes=p.build();
    std::ofstream out(target,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));check(bool(out),"write synthetic callback module");
}
static std::string settle(Fixture& f,std::string state){
    for(int n=0;n<16;++n){
        if(waiting(state,"transitionReady"))state=acknowledge(f,state);
        else if(waiting(state,"frame")||waiting(state,"budget")||waiting(state,"transition"))state=f.command("tick",std::to_string(1000+n*50).c_str());
        else return state;
    }
    throw std::runtime_error("noise extension fixture did not settle");
}
static void attach(Program& p,int clip){for(int n:{0,0,-1,clip,0})p.push(n);p.op(0x278);}
static void cancel(Program& p){for(int n:{0,0,0,0,0})p.push(n);p.op(0x278);}
static std::string advance(Fixture& f,std::string state){
    if(boolean(state,"revealing"))state=f.command("advance");
    return settle(f,f.command("advance"));
}
static void auxMeta(Fixture& f,int clip,const std::vector<int>& values,bool skipOnly=false){
    std::ostringstream core,props;
    for(size_t n=0;n<values.size();++n){
        const int time=values.size()==3?(n==0?0:n==1?3000:3001):int(n)*7000;
        const int duration=values.size()==3?3001:7000;
        core<<clip<<' '<<time<<" -32767 -32767 -32767 1000 1000 -100001 0 "<<(n?2:0)<<" 0 "<<duration<<'\n';
        props<<clip<<' '<<n<<' '<<n<<" 0x1000";
        for(int field=0;field<13;++field)props<<' '<<(field==11?(skipOnly?-32766:values[n]):-32767);
        props<<'\n';
    }
    file(f,"meta/effects.tsv",core.str());file(f,"meta/effects-properties.tsv",props.str());fresh(f);
}
static std::vector<std::string> firstEffectPacket(const mahoyo::save::Document& doc){
    std::istringstream input(std::string(doc.hostExtra.begin(),doc.hostExtra.end()));std::string row;
    check(bool(std::getline(input,row))&&row=="WOHHOST23","actual WH21 noise packet");
    size_t count=0;input>>count;std::getline(input,row);
    for(size_t n=0;n<count;++n)check(bool(std::getline(input,row)),"complete layer rows");
    input>>count;std::getline(input,row);check(count==1&&bool(std::getline(input,row)),"one active synthetic effect");
    auto fields=hostFields(row);check(fields.size()==35,"reuse current exact35-column packet");return fields;
}
static mahoyo::save::Document badBaseline(mahoyo::save::Document doc){
    std::string text(doc.hostExtra.begin(),doc.hostExtra.end());std::istringstream input(text);std::ostringstream output;std::string row;
    std::getline(input,row);output<<row<<'\n';size_t count;input>>count;std::getline(input,row);output<<count<<'\n';
    for(size_t n=0;n<count;++n){std::getline(input,row);output<<row<<'\n';}
    input>>count;std::getline(input,row);check(count==1,"one baseline mutation target");output<<count<<'\n';
    std::getline(input,row);auto fields=hostFields(row);check(fields.size()==35,"current baseline field exists");fields[34]="9";hostRow(output,fields,35);
    while(std::getline(input,row))output<<row<<'\n';
    text=output.str();doc.hostExtra.assign(text.begin(),text.end());return doc;
}
static void aux8372SaveCancelClone(){
    Program p;p.image();p.noise(0,1);attach(p,8372);p.dialogue(0);cancel(p);p.commit();p.dialogue(1);p.clone(1,0,"fake");p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");prepareNoiseImages(f);auxMeta(f,8372,{1,1,21});
    auto state=settle(f,f.command("action","start"));check(number(layerObject(state,0),"decimalMode")==1&&number(noiseObject(state,0),"periodMs")==50,"8372 starts at same effective1");
    f.command("save","2");auto doc=mahoyo::save::read(f.root/"savedata/slot2.whs");check(firstEffectPacket(doc).back()=="1","attachment decimal1 retained separately");
    fresh(f);state=f.command("load","2");check(number(layerObject(state,0),"decimalMode")==1,"active decimal survives fresh WH19");
    state=advance(f,state);check(number(layerObject(state,0),"decimalMode")==1,"finite cancel freezes live decimal");
    f.command("save","2");doc=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string text(doc.hostExtra.begin(),doc.hostExtra.end());
    auto at=text.find("EFFECTBASES1 1\n");check(at!=std::string::npos,"cancel preserves D0 effect packet");auto end=text.find('\n',at+15);auto fields=hostFields(text.substr(at+15,end-at-15));check(fields.size()==35&&fields.back()=="21","terminal D0 decimal21 differs from frozen live1");
    fresh(f);state=f.command("load","2");state=advance(f,state);check(number(layerObject(state,1),"decimalMode")==0,"242 target resets noise after stopped-base restore");f.passed=true;
}
static void aux7007RejectAndRestore(){
    Program p;p.image();attach(p,7007);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,"","fake 100 100\n");prepareNoiseImages(f);auxMeta(f,7007,{1,1});
    auto state=settle(f,f.command("action","start"));check(number(layerObject(state,0),"decimalMode")==1,"7007 applies selected aux1");
    f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");check(firstEffectPacket(good).back()=="0","7007 baseline0 remains independent of live1");
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",host18Fixture(good));
    auto rejected=std::string(woh_command(f.handle,"load","2"));check(rejected.find("Legacy active/stopped effect lacks initial noise state")!=std::string::npos&&number(layerObject(rejected,0),"decimalMode")==1,"old aux baseline rejects atomically");
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",badBaseline(good));
    rejected=woh_command(f.handle,"load","2");check(rejected.find("Invalid saved effect noise baseline")!=std::string::npos&&number(layerObject(rejected,0),"decimalMode")==1,"invalid19 baseline rejects atomically");
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);fresh(f);state=f.command("load","2");check(number(layerObject(state,0),"decimalMode")==1,"fresh7007 restores live noise");f.passed=true;
}
static void ordinary18(bool selectedSkip){
    Program p;p.image();p.noise(0,21);attach(p,1);p.dialogue(0);p.op(0xf4);
    const std::string core="1 0 -32767 -32767 -32767 1000 1000 -100001 0 0 0 7000\n1 7000 -32767 -32767 -32767 1000 1000 -100001 0 2 0 7000\n";
    Fixture f(std::move(p),true,core,"fake 100 100\n");prepareNoiseImages(f);if(selectedSkip)auxMeta(f,1,{0,0},true);else fresh(f);
    settle(f,f.command("action","start"));f.command("save","2");auto legacy=host18Fixture(mahoyo::save::read(f.root/"savedata/slot2.whs"));mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",legacy);
    fresh(f);auto state=f.command("load","2");check(number(layerObject(state,0),"decimalMode")==21,selectedSkip?"ordinary18 selected all-SKIP aux needs no new baseline":"ordinary18 active ordinary effect preserves noise21");f.passed=true;
}
static void callback82OrC9(bool c9){
    const char* name=c9?"c_9.chs":"9_6.chs";Program entry;entry.string(name);entry.op(0xf3);
    Fixture f(std::move(entry),true,"",c9?"img2376 100 100\n":"img0408 100 100\n");prepareNoiseImages(f);f.imageHeader(c9?"img2376":"img0408",100,100);
    Program p;p.push(c9?38:0);p.op(0x12a);p.image(1000,c9?"img2376":"img0408",0,c9?std::array<int32_t,6>{864,0,191,1080,0,94}:std::array<int32_t,6>{});
    p.blend(0,c9?22:6,0);if(!c9)p.noise(0,21);p.commit();p.dialogue(0);p.op(0xf4);module(f,name,std::move(p));fresh(f);
    auto state=settle(f,f.command("action","start"));
    if(c9){check(number(state,"finalCallback28")==120,"ordinary C9 own-window derives final28 without noise");check(layerObject(state,0).find("\"noise\":")==std::string::npos,"C9 final28 does not manufacture a noise source");}
    else check(number(noiseObject(state,0),"profile")==82&&number(noiseObject(state,0),"periodMs")==100,"82 exact context derives profile/period");f.passed=true;
}
static std::string completeLayer(const std::string& state,int id){
    auto layers=visibleLayers(state);auto at=layers.find("\"id\":"+std::to_string(id)+",");
    check(at!=std::string::npos,"callback owner exists");const auto start=layers.rfind('{',at);
    check(start!=std::string::npos,"callback owner object starts");size_t depth=0;bool quoted=false,escaped=false;
    for(size_t n=start;n<layers.size();++n){char c=layers[n];
        if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;else if(c=='{')++depth;else if(c=='}'&&!--depth)return layers.substr(start,n-start+1);
    }
    throw std::runtime_error("Incomplete callback owner JSON");
}
static void callback85(int colour=0,bool lateInvalid=false){
    Program entry;entry.string("a_0.chs");entry.op(0xf3);Fixture f(std::move(entry),true,"","aok_n_06_02_07 100 100\n");prepareNoiseImages(f);f.imageHeader("aok_n_06_02_07_40_00",1532,1526);
    Program p;p.push(33);p.op(0x12a);std::array<int,9> rest{{40,0,-1,0,0,0,0,0,0}};for(auto it=rest.rbegin();it!=rest.rend();++it)p.push(*it);
    p.string("CC");for(int n:{0,0,255,1000,50,50})p.push(n);p.string("aok_n_06_02_07");p.push(0);p.op(0x240);p.blend(0,17,0);p.noise(0,1);p.push(0);p.push(colour);p.push(0);p.op(0x253);p.commit();p.dialogue(0);if(lateInvalid){p.push(0);p.push(17);p.push(0);p.op(0x253);p.dialogue(1);}p.op(0xf4);
    module(f,"a_0.chs",std::move(p));fresh(f);auto state=settle(f,f.command("action","start"));auto layer=completeLayer(state,0);
    check(number(noiseObject(state,0),"profile")==85&&number(layer,"finalCallback28")==85,"A0 noise85 and final28 remain separate markers");
    check(boolean(layer,"nativeFullBitmapGeometryReviewed"),"85 exact SINGLE header is reviewed independently of base-pair metadata");
    check(number(layer,"colorMode")==colour,"85 reviewed colour0/70 is preserved");
    if(lateInvalid){
        check(waiting(state,"input")&&!boolean(state,"revealing"),"valid85 reaches real input before unsupported change");
        state=woh_command(f.handle,"advance","");check(waiting(state,"error")&&state.find("Unreviewed Native A0 final28 context")!=std::string::npos,"invalid colour17 remains strict and becomes a complete error DTO");
        const auto pc=number(state,"modulePc"),instructions=number(state,"instructions");
        for(const char* command:{"advance","advance","save","tick","auto","skip"}){
            state=woh_command(f.handle,command,std::string(command)=="save"?"2":std::string(command)=="tick"?"100000":"");
            check(waiting(state,"error")&&!boolean(state,"canSave")&&number(state,"nextTickMs")==0&&number(state,"modulePc")==pc&&number(state,"instructions")==instructions,"error wait freezes VM and saving after input failure");
        }
        state=woh_snapshot(f.handle);check(waiting(state,"error")&&number(state,"modulePc")==pc,"repeated snapshot never leaks a guard exception");
        for(const char* shape:{"\"layers\":[]","\"audio\":[]","\"text\":[]","\"actions\":[]","\"history\":[]","\"movie\":null","\"transition\":null","\"choice\":null","\"screenWave\":null","\"gallery\":null","\"musicRoom\":null","\"archive\":null","\"modal\":null","\"inputRegion\":null"})check(state.find(shape)!=std::string::npos,"error DTO includes parser-required collection/null fields");
        check(!fs::exists(f.root/"savedata/slot2.whs"),"error halt creates no save");
        state=f.command("title");check(state.find("\"scene\":\"title\"")!=std::string::npos&&state.find("\"error\":\"\"")!=std::string::npos,"title recovers without resetting/restarting the process");
    }
    f.passed=true;
}
static void terminalSkipClone(){
    Program p;p.image();attach(p,1);p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);p.op(0xf4);
    //The final SKIP case is synthetic, with different values/times from the game.
    const std::string core="1 0 12 24 -32767 -32767 -32767 -100001 0 0 0 1000\n1 1000 -32766 -32766 -32767 -32767 -32767 -100001 0 0 0 1000\n";
    Fixture f(std::move(p),true,core,"fake 100 100\n");fresh(f);auto state=settle(f,f.command("action","start"));const auto live=layerObject(state,0);
    check(number(live,"x")!=50&&number(live,"y")!=50,"live effect uses synthetic first key");
    f.command("save","2");fresh(f);state=f.command("load","2");state=advance(f,state);
    check(number(layerObject(state,1),"x")==50&&number(layerObject(state,1),"y")==50,"terminal SKIP clone restores D0 initial after fresh WH19");
    check(number(layerObject(state,0),"x")==number(live,"x")&&number(layerObject(state,0),"y")==number(live,"y"),"clone terminal evaluation preserves source live position");f.passed=true;
}
static const char* c9core="6666 0 242 137 0 -32767 -32767 -100001 2 0 0 4000\n6666 4000 241 -32767 160 -32767 -32767 -100001 0 -32767 0 4000\n";
static const char* c9win="6666 0 22566 0x1e944431 864 0 191 1080 0 94\n6666 1 22567 0x1e944431 806 0 308 1080 -32767 -32767\n";
static const char* c9props="6666 0 22566 0x1e944431 -32767 -32767 17 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 9530\n6666 1 22567 0x1e944431 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767\n";
static Program c9Entry(){Program p;p.string("c_9.chs");p.op(0xf3);return p;}
static void c9Setup(Fixture& f,bool stop=false,bool badMeta=false,int incompatible=0){
 using namespace noise_extension_cases;f.imageHeader("img2376",100,100);
 Program p;p.push(38);p.op(0x12a);p.image(1000,"img2376",7,{864,0,191,1080,0,94});p.blend(7,22,0);
 for(int n:{0,0,-1,6666,7})p.push(n);p.op(0x278);p.commit();p.dialogue(0);
 if(incompatible==1){for(int n:{0,0,-1,1,7})p.push(n);p.op(0x278);p.commit();p.dialogue(1);}if(incompatible==2){p.clone(8,7,"img2376");p.dialogue(1);}
 if(stop){for(int n:{0,0,0,0,7})p.push(n);p.op(0x278);p.commit();p.dialogue(1);}
 p.push(39);p.op(0x12a);p.dialogue(1);p.push(7);p.op(0x241);p.commit();p.dialogue(0);p.op(0xf4);
 module(f,"c_9.chs",std::move(p));file(f,"meta/effects.tsv",c9core);file(f,"meta/effects-window.tsv",c9win);
 auto props=std::string(c9props);if(badMeta)props.replace(props.find("9530"),4,"9531");if(incompatible==1){file(f,"meta/effects.tsv",std::string(c9core)+"1 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 10000\n1 10000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 10000\n");file(f,"meta/effects-window.tsv",std::string(c9win)+"1 0 0 0x0 -32767 -32767 -32767 -32767 -32767 -32767\n1 1 1 0x0 -32767 -32767 -32767 -32767 -32767 -32767\n");for(int i=0;i<2;++i){props+="1 "+std::to_string(i)+" "+std::to_string(i)+" 0x0";for(int k=0;k<13;++k)props+=" -32767";props+="\n";}}file(f,"meta/effects-properties.tsv",props);fresh(f);
}
static std::string c9Start(Fixture& f){using namespace noise_extension_cases;return settle(f,f.command("action","start"));}
static void c9Life(){
 using namespace noise_extension_cases;Fixture f(c9Entry(),false,"","img2376 100 100\n");c9Setup(f);
 auto state=c9Start(f),layer=completeLayer(state,7);check(number(layer,"blend")==17&&number(layer,"x")!=0&&number(layer,"width")==191&&number(layer,"finalCallback28")==120,"6666 first key selects17 while retaining initial window28");
 f.command("tick","1000");state=f.pulseTick("2984");layer=completeLayer(state,7);check(number(layer,"width")>191&&number(layer,"width")<308&&number(layer,"finalCallback28")==120,"6666 intermediate own rect still gets callback28");
 state=f.pulseTick("4984");layer=completeLayer(state,7);check(number(layer,"width")==308&&number(layer,"height")==1080&&number(layer,"softEdge")==94&&number(layer,"finalCallback28")==120,"6666 endpoint after natural job completion remains approved");
 f.command("save","2");fresh(f);state=f.command("load","2");check(number(completeLayer(state,7),"width")==308,"WH19 inactive terminal restores without new schema");
 state=advance(f,state);f.command("save","2");InspectHost inspect;mahoyo::vm::Machine saved(inspect);saved.loadState(mahoyo::save::read(f.root/"savedata/slot2.whs").vm);check(saved.variable(0x462)==39&&number(completeLayer(state,7),"finalCallback28")==120,"sub39 keeps the same approved rect callback");
 state=advance(f,state);check(visibleLayers(state)=="[]","owner7 explicit delete ends this Native profile");f.passed=true;
}
static void c9StopRestore(){
 using namespace noise_extension_cases;Fixture f(c9Entry(),false,"","img2376 100 100\n");c9Setup(f,true);auto state=c9Start(f);f.command("tick","1000");state=f.pulseTick("2984");
 auto w=number(completeLayer(state,7),"width");state=advance(f,state);check(number(completeLayer(state,7),"width")==w,"cancel freezes intermediate live rect");f.command("save","2");fresh(f);state=f.command("load","2");check(number(completeLayer(state,7),"width")==w&&number(state,"finalCallback28")==120,"stopped6666 + terminal D0 restores intermediate live rect");f.passed=true;
}
static void c9MetaNegative(){
 Fixture f(c9Entry(),false,"","img2376 100 100\n");c9Setup(f,false,true);auto state=std::string(woh_command(f.handle,"action","start"));if(waiting(state,"transitionReady"))state=acknowledge(f,state);
 check(state.find("Unreviewed Native C9 rect final28 context")!=std::string::npos&&waiting(state,"error"),"changed clip metadata does not widen approval");f.passed=true;
}
static void c9UnrelatedNegative(){
 for(int mode:{1,2}){Fixture f(c9Entry(),false,"","img2376 100 100\n");c9Setup(f,false,false,mode);auto state=c9Start(f);state=woh_command(f.handle,"advance","");if(waiting(state,"transitionReady"))state=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(state.substr(state.find("\"transition\":")),"id"))).c_str());
 check(waiting(state,"error")&&state.find("Unreviewed Native C9 rect final28 context")!=std::string::npos,mode==1?"different active clip cannot inherit terminal6666 approval":"clone owner8 cannot inherit owner7 dynamic approval");f.passed=true;}
}

}
static void nativeNoiseExtensionRuntimeCases(){
    noise_extension_cases::aux8372SaveCancelClone();
    noise_extension_cases::aux7007RejectAndRestore();
    noise_extension_cases::ordinary18(false);
    noise_extension_cases::ordinary18(true);
    noise_extension_cases::callback82OrC9(false);
    noise_extension_cases::callback82OrC9(true);
    noise_extension_cases::callback85();
    noise_extension_cases::callback85(70);
    noise_extension_cases::callback85(70,true);
    noise_extension_cases::terminalSkipClone();
    noise_extension_cases::c9Life();
    noise_extension_cases::c9StopRestore();
    noise_extension_cases::c9MetaNegative();
    noise_extension_cases::c9UnrelatedNegative();
}
