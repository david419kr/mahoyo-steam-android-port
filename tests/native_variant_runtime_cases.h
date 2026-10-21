// Synthetic CHS, native-name metadata and PNG headers only; no game pixels.
#include <algorithm>

static uint64_t nvFNV(const std::string& body){uint64_t h=14695981039346656037ull;for(unsigned char c:body){h^=c;h*=1099511628211ull;}return h;}
static std::string nvInventory(const std::string& body,size_t count){std::ostringstream out;out<<"WOH_NATIVE_IMAGES1 "<<count<<' '<<std::hex<<std::setfill('0')<<std::setw(16)<<nvFNV(body)<<'\n'<<body;return out.str();}
static std::string nvInventoryNames(std::vector<std::string> names){std::sort(names.begin(),names.end());std::string body;for(const auto& name:names)body+=name+"\n";return nvInventory(body,names.size());}
static const std::vector<std::string> nvNames={"fake.mzp","fake_04_04.mzp","fake_06_00.mzp","fake_06_02.mzp"};
static void nvWrite(Fixture& f,const std::string& file,const std::string& bytes){auto path=f.game/"meta"/file;fs::create_directories(path.parent_path());f.extraFiles.push_back(path);std::ofstream out(path,std::ios::binary);out.write(bytes.data(),std::streamsize(bytes.size()));check(bool(out),"write private native variant metadata");}
static void nvRestart(Fixture& f){woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());check(f.handle!=0,"fresh native variant fixture Runtime");}
static void nvDirect(Program& p,int x,int y,int id=0){p.push(y);p.push(x);p.push(id);p.op(0x256);}
static std::string nvTable(){return "915 0 50 50 64 -32767 -32767 -100001 0 0 0 1000\n915 1000 100 50 128 -32767 -32767 -100001 0 0 0 1000\n";}
static std::string nvWindows(uint64_t bits){std::ostringstream out;out<<"915 0 10000 0x"<<std::hex<<bits<<std::dec<<" 5 7 60 70 0 1\n915 1 10001 0x"<<std::hex<<bits<<std::dec<<" 25 7 80 70 0 1\n";return out.str();}
static void nvProperties(Fixture& f,uint64_t bits){std::ostringstream out;for(int frame=0;frame<2;++frame){out<<"915 "<<frame<<' '<<(10000+frame)<<" 0x"<<std::hex<<bits<<std::dec;for(int n=0;n<13;++n)out<<' '<<(n==7?6:n==8?0:-32767);out<<'\n';}nvWrite(f,"effects-properties.tsv",out.str());}
static void nvPrepare(Fixture& f,uint64_t bits,const std::string& inventory){f.imageHeader("fake_04_04",100,100);f.imageHeader("fake_06_00",200,120);f.imageHeader("fake_06_02",100,100);nvProperties(f,bits);if(!inventory.empty())nvWrite(f,"native-image-resources.tsv",inventory);nvRestart(f);f.command("textSpeed","0");}
static std::string nvReady(Fixture& f,std::string state,uint64_t& clock){for(int i=0;i<8;++i){if(waiting(state,"transitionReady"))state=acknowledge(f,state);else if(waiting(state,"frame"))state=f.command("tick",std::to_string(clock+=1000).c_str());else return state;}throw std::runtime_error("native variant fixture could not settle prepare");}
static void nvImageIs(const std::string& state,int id,const char* name,const char* reason){check(layerObject(state,id).find(std::string("\"image\":\"")+name+"\"")!=std::string::npos,reason);}
static std::vector<std::string> nvEffectRow(const mahoyo::save::Document& doc,bool stopped){std::istringstream input(std::string(doc.hostExtra.begin(),doc.hostExtra.end()));std::string line;size_t count=0;
    if(stopped){while(std::getline(input,line))if(line.rfind("EFFECTBASES1 ",0)==0){count=size_t(std::stoull(line.substr(13)));break;}}
    else{std::getline(input,line);input>>count;std::getline(input,line);for(size_t i=0;i<count;++i)std::getline(input,line);input>>count;std::getline(input,line);}
    check(currentHostSchema(doc)&&count==1,"exactly one current synthetic active/stopped effect baseline");std::getline(input,line);auto fields=hostFields(line);check(fields.size()==35&&fields[0]=="0"&&fields[1]=="915"&&fields[34]=="0","native variant baseline keeps original34 fields and appends decimal");return fields;
}

static void nativeMissingEffectVariantAndFreshBaselines(){
    const uint64_t bits=0x20004880ull; // VariantX + core X/Alpha + native window X/Width; VariantY unselected.
    for(bool strayPrepared:{false,true}){
        Program p;p.image(1000,"fake",0,{{5,7,60,70,0,1}});nvDirect(p,4,4);effectRequest(p,0,915);p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);effectRequest(p,0,0);p.commit();p.dialogue(0);p.clone(2,0,"fake");p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p),true,nvTable(),"fake 100 100\n","",nvWindows(bits));nvPrepare(f,bits,nvInventoryNames(nvNames));
        if(strayPrepared)f.imageHeader("fake_06_04",300,100); // PNG existence cannot invent a Native resource.
        uint64_t clock=1000;auto state=nvReady(f,f.command("action","start"),clock);nvImageIs(state,0,"fake_04_04.mzp","Native-absent6_4 retains actual4_4 on first sample");auto start=layerObject(state,0);check(number(start,"w")==100,"unselectedY4 is not replaced by propertyY0 and valid6_0");
        f.command("tick","1000");state=f.command("tick","1484");nvImageIs(state,0,"fake_04_04.mzp","Native-absent target still holds while time advances");auto mid=layerObject(state,0);check(number(mid,"x")>number(start,"x")&&number(mid,"alpha")>number(start,"alpha")&&number(mid,"width")>number(start,"width"),"missing resource never returns before X alpha and window sampling");check(number(state,"nextTickMs")==16&&boolean(state,"canSave"),"held variant keeps ordinary effect clock and save eligibility");
        f.command("save","2");auto active=mahoyo::save::read(f.root/"savedata/slot2.whs");auto fields=nvEffectRow(active,false);check(fields[22]=="4"&&fields[23]=="4","active WH18 baseline retains original variant axes, not failed request");auto before=visibleLayers(state);nvRestart(f);f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh active effect restore preserves held image and sampled geometry");
        state=nvReady(f,f.command("advance"),clock);nvImageIs(state,1,"fake_04_04.mzp","terminal clone uses same absent-resource preservation");auto clone=layerObject(state,1);check(std::abs(number(clone,"alpha")-128/255.)<.00001&&number(clone,"width")==80,"terminal clone still samples final alpha and window");
        state=nvReady(f,f.command("advance"),clock);f.command("save","2");auto stopped=mahoyo::save::read(f.root/"savedata/slot2.whs");fields=nvEffectRow(stopped,true);check(fields[22]=="4"&&fields[23]=="4"&&std::abs(std::stod(fields[7])-128/255.)<.00001&&fields[13]=="80","finite cancellation commits a full terminal D0 while absent variant stays4_4");auto stoppedVisible=visibleLayers(state);nvRestart(f);f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==stoppedVisible,"fresh stopped-D0 restore keeps original live graphics distinct from terminal baseline");
        state=nvReady(f,f.command("advance"),clock);nvImageIs(state,2,"fake_04_04.mzp","clone after fresh stopped-D0 restore keeps held variant");auto restoredClone=layerObject(state,2);check(std::abs(number(restoredClone,"x")-std::stod(fields[3]))<.00001&&std::abs(number(restoredClone,"alpha")-128/255.)<.00001&&number(restoredClone,"width")==80,"fresh D0 clone retains selected terminal geometry and alpha/window");f.passed=true;
    }
}

static void nativeValidVariantAndFiniteCompletion(){
    for(bool bothAxes:{false,true}){
        const uint64_t bits=bothAxes?0x60004800ull:0x20004880ull;Program p;p.image(1000,"fake",0,{{5,7,60,70,0,1}});nvDirect(p,4,4);effectRequest(p,0,915);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,nvTable(),"fake 100 100\n","",nvWindows(bits));nvPrepare(f,bits,nvInventoryNames(nvNames));uint64_t clock=1000;auto state=nvReady(f,f.command("action","start"),clock);auto first=layerObject(state,0);
        nvImageIs(state,0,bothAxes?"fake_06_00.mzp":"fake_04_04.mzp",bothAxes?"selected XY accepts Native-present6_0":"unselectedY4 remains authoritative even when6_0 is prepared");check(number(first,"w")== (bothAxes?200:100),"valid target alone changes intrinsic width");f.command("tick","1000");state=f.command("tick","2500");auto last=layerObject(state,0);check(number(last,"x")>number(first,"x")&&std::abs(number(last,"alpha")-128/255.)<.00001,"held and accepted variants both reach finite motion/alpha endpoint");check(waiting(state,"input")&&number(state,"nextTickMs")==0&&boolean(state,"canSave"),"Native-absent lookup cannot leave finite effect polling forever");f.passed=true;
    }
}

static void nativeVariantStrictFallbackAndDirect256(){
    for(int kind=0;kind<5;++kind){const uint64_t bits=0x20004880ull;Program p;p.image();nvDirect(p,4,4);if(kind==4)nvDirect(p,6,4);else effectRequest(p,0,915);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,nvTable(),"fake 100 100\n","",nvWindows(bits));auto names=nvNames;if(kind==1)names.erase(names.begin());if(kind==2||kind==3)names.push_back("fake_06_04.mzp");nvPrepare(f,bits,kind==0?"":nvInventoryNames(names));if(kind==3){f.imageHeader("fake_06_04",100,100);std::ofstream bad(f.game/"images/fake_06_04.mzp.png",std::ios::binary|std::ios::trunc);bad<<"corrupt";}
        auto state=std::string(woh_command(f.handle,"action","start"));check(state.find("Missing or invalid prepared image: fake_06_04.mzp")!=std::string::npos,"inventory absent/unknown base/Native-present missing or corrupt PNG/direct256 all retain strict image error");f.passed=true;
    }
}

static void nativeVariantInventoryMalformedCreationErrors(){
    std::vector<std::string> invalid;
    const std::string body="fake.mzp\n",valid=nvInventory(body,1);auto header=valid.substr(0,valid.find('\n'));
    invalid.push_back(nvInventory(body,2)); // correct checksum, wrong complete count
    invalid.push_back(nvInventory("fake.mzp\nfake.mzp\n",2));
    invalid.push_back(nvInventory("z.mzp\na.mzp\n",2));
    invalid.push_back(nvInventory("../fake.mzp\n",1));
    invalid.push_back(nvInventory("FAKE.mzp\n",1));
    invalid.push_back(nvInventory("fake.mzp",1));
    invalid.push_back(header+"\n"); // truncated body, old checksum
    invalid.push_back("WOH_NATIVE_IMAGES1 1 0000000000000000\n"+body);
    invalid.push_back(header+"\r\n"+body);
    auto tab=valid;tab[tab.find(' ')]='\t';invalid.push_back(tab);
    invalid.push_back(" "+valid);
    invalid.push_back("WOH_NATIVE_IMAGES1 01 "+header.substr(header.rfind(' ')+1)+"\n"+body);
    invalid.push_back(nvInventory(body,20001));
    invalid.push_back(std::string(2*1024*1024+1,'x'));
    for(const auto& text:invalid){Program p;p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));nvWrite(f,"native-image-resources.tsv",text);nvRestart(f);auto state=std::string(woh_snapshot(f.handle));check(state.find("Invalid native image resource inventory")!=std::string::npos,"present malformed inventory is a creation error, never an empty/absent fallback");f.passed=true;}
}
