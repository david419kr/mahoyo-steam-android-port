// Synthetic CHS and CTD only. No installed game files, devices or user saves.
#include "../runtime/runtime_api.h"
#include "../runtime/save_store.h"
#include "../runtime/host_state20.h"
#include "../runtime/language_text.h"
#include "../runtime/shared_state.h"
#include "../runtime/noise.h"
#include "../runtime/archive_catalog.h"
#include "../runtime/history.h"
#include "../runtime/vm/machine.h"
#include <chrono>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <utility>
#include <tuple>
#include <vector>

namespace fs=std::filesystem;
static void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static double number(const std::string& state,const std::string& key){auto at=state.find("\""+key+"\":");if(at==std::string::npos)throw std::runtime_error("Missing field "+key);return std::stod(state.substr(at+key.size()+3));}
static bool boolean(const std::string& state,const std::string& key){return state.find("\""+key+"\":true")!=std::string::npos;}
static bool waiting(const std::string& state,const std::string& name){return state.find("\"waiting\":\""+name+"\"")!=std::string::npos;}
struct Program {
    std::vector<uint32_t> words;
    std::vector<std::pair<size_t,std::string>> strings;
    //Unrelated audio/geometry fixtures explicitly start with an immediate
    //window show, so a first-paragraph auto fade cannot alter their clocks.
    explicit Program(bool immediateWindow=true){if(immediateWindow){push(0);op(0x153);}}
    void op(uint32_t value){words.push_back(value);}
    void push(int32_t value){op(0);op(uint32_t(value));}
    void string(std::string value){op(3);strings.push_back({words.size(),std::move(value)});op(0);}
    void dialogue(int id,bool wait=true){push(1);push(wait?1:0);push(0);push(0);string(id?"$000001":"$000000");op(0x140);}
    void image(int scale=1000,const std::string& name="fake",int id=0,std::array<int32_t,6> window={},int parent=-1){std::array<int,9> rest{{0,0,parent,window[0],window[1],window[2],window[3],window[5],window[4]}};for(auto it=rest.rbegin();it!=rest.rend();++it)push(*it);string("CC");push(0);push(0);push(255);push(scale);push(50);push(50);string(name);push(id);op(0x240);}
    void clone(int id,int source,const std::string& name,int scale=INT32_MIN,const std::string& anchor="CC") {push(source);for(int n=0;n<9;++n)push(INT32_MIN);string(anchor);for(int n:{INT32_MIN,INT32_MIN,INT32_MIN,scale,INT32_MIN,INT32_MIN})push(n);string(name);push(id);op(0x242);}
    void axisScale(int id,int x,int y){for(int n:{0,0,0,y,x,INT32_MIN,INT32_MIN,id})push(n);op(0x246);}
    void origin(int id,int x,int y){for(int n:{INT32_MIN,y,x,id})push(n);op(0x250);}
    void range(int id,const std::string& name,int x0,int y0,int x1,int y1){push(y1);push(x1);push(y0);push(x0);if(name.empty())push(0);else string(name);push(id);op(0x220);}
    void preload(int tag,const std::string& name,int brightness=0,int contrast=0,int x=0,int y=0,int mode=0,int rgb=0){push(rgb);push(mode);push(y);push(x);push(contrast);push(brightness);if(name.empty())push(0);else string(name);push(tag);op(0x222);}
    void commit(){for(int n:{0,256,0,0,0,1})push(n);op(0x200);}
    void sound(int slot,int from,int to,int pan0,int pan1,int duration,int delay=0,const std::string& name="fake",int backend=0){for(int n:{delay,duration,pan1,pan0,to,from,backend})push(n);string(name);push(slot);op(0x197);}
    void sound190(int slot,int backend,const std::string& name,int volume,int pan,int fade=0,int delay=0){for(int n:{delay,fade,pan,volume})push(n);if(name.empty())push(0);else string(name);push(backend);push(slot);op(0x190);}
    void volumeFade(int slot,int to,int duration,int delay,int reserved0=0,int reserved1=0){for(int n:{delay,duration,to,reserved1,reserved0,slot})push(n);op(0x196);}
    void bgm(int id,int volume=100,int loop=1,int fade=0,int oldFade=0){for(int n:{oldFade,fade,loop,volume,id})push(n);op(0x180);}
    void globalShake(int x=8,int y=2,int period=200,int cycles=1,int delay=0){for(int n:{delay,cycles,period,y,x})push(n);op(0x340);}
    void layerShake(int id,int x=0,int y=5,int period=300,int duration=300,int delay=0,int mode=2){for(int n:{mode,delay,duration,period,y,x,id})push(n);op(0x270);}
    void position(int id,int x,int y,int duration=0){string("CC");for(int n:{0,duration,y,x,INT32_MIN,INT32_MIN,id,0})push(n);op(0x243);}
    void rotate(int id,int angle){for(int n:{0,0,0,0,angle,angle,id})push(n);op(0x247);}
    void targetOffset(int id,int x,int y,int divisor=1){for(int n:{0,divisor,0,0,y,x,id})push(n);op(0x259);}
    void wave(int id,int preset,int fade=0){for(int n:{fade,preset,id})push(n);op(0x258);}
    void waitWave(int id=-1){for(int n:{1,64,id})push(n);op(0x27f);}
    void alpha(int id,int value){for(int n:{0,0,value,value,id})push(n);op(0x245);}
    void blend(int id,int value,int flag=-1){for(int n:{flag,0,value,id})push(n);op(0x252);}
    void clock(int first=2,int hour=10,int minute=51,int x=-375,int y=-283,int scale=1000,int color=35){for(int n:{color,scale,y,x,minute,hour,first})push(n);op(0x380);}
    void clockTime(int first,int hour,int minute){for(int n:{minute,hour,first})push(n);op(0x381);}
    void noise(int id,int raw){push(raw);push(id);op(0x257);}
    std::vector<uint8_t> build(){
        std::vector<uint8_t> suffix;for(const auto& item:strings){words[item.first]=uint32_t(words.size()*4+suffix.size());suffix.insert(suffix.end(),item.second.begin(),item.second.end());suffix.push_back(0);}
        std::vector<uint8_t> result(48);std::memcpy(result.data(),"HunexCompiledScriptVer1.00",26);result[27]=21;
        for(auto word:words)for(unsigned k=0;k<4;++k)result.push_back(uint8_t(word>>(8*k)));
        result.insert(result.end(),suffix.begin(),suffix.end());return result;
    }
};
struct Fixture {
    fs::path root,game;
    uint64_t handle=0;
    uint64_t testPlatformLast=0;bool testPlatformAnchor=false;
    bool passed=false;
    std::vector<fs::path> extraFiles;
    explicit Fixture(Program program,bool image=false,const std::string& effectTable={},const std::string& layout={},const std::string& loopMap={},const std::string& windowTable={}){
        root=fs::temp_directory_path()/("mahoyo-runtime-api-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root),"allocate private fixture directory");game=root/"game";
        fs::create_directories(game/"scripts");fs::create_directories(game/"text");
        {auto bytes=program.build();std::ofstream file(game/"scripts/1_0.chs",std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));check(bool(file),"write synthetic CHS");}
        {std::ofstream file(game/"text/script_text_ja.ctd",std::ios::binary);file<<"abcdefghij\nklmnopqrst\n";}
        if(image){
            // Backend metadata fixture only: no copyrighted pixels or decoder run.
            fs::create_directory(game/"images");std::vector<uint8_t> png(24);const uint8_t magic[]={137,80,78,71,13,10,26,10};std::memcpy(png.data(),magic,8);png[11]=13;std::memcpy(png.data()+12,"IHDR",4);png[19]=100;png[23]=100;
            std::ofstream file(game/"images/fake.mzp.png",std::ios::binary);file.write(reinterpret_cast<const char*>(png.data()),std::streamsize(png.size()));
        }
        if(!effectTable.empty()){fs::create_directory(game/"meta");std::ofstream file(game/"meta/effects.tsv",std::ios::binary);file<<effectTable;}
        if(!layout.empty()){fs::create_directory(game/"meta");std::ofstream file(game/"meta/image-layout.tsv",std::ios::binary);file<<layout;}
        if(!loopMap.empty()){fs::create_directories(game/"meta");std::ofstream file(game/"meta/audio-loop-map.tsv",std::ios::binary);file<<loopMap;}
        if(!windowTable.empty()){fs::create_directories(game/"meta");std::ofstream file(game/"meta/effects-window.tsv",std::ios::binary);file<<windowTable;}
        handle=woh_create(game.string().c_str());check(handle!=0,"create synthetic runtime");
    }
    ~Fixture(){
        if(handle)woh_destroy(handle);
        if(!passed){std::cerr<<"Synthetic fixtures retained: "<<root.string()<<'\n';return;}
        // Exact names made by this fixture; no recursive deletion or other slots.
        std::error_code ignored;
        for(const auto& path:{root/"savedata/slot2.whs",root/"savedata/seen-images.whp",root/"savedata/trophies.whp",root/"savedata/shared.whp",game/"scripts/1_0.chs",game/"text/script_text_ja.ctd",game/"images/fake.mzp.png",game/"meta/effects.tsv",game/"meta/image-layout.tsv",game/"meta/audio-loop-map.tsv",game/"meta/effects-window.tsv"})fs::remove(path,ignored);
        for(const auto& path:extraFiles)fs::remove(path,ignored);
        for(const auto& path:{root/"savedata",game/"scripts",game/"text",game/"images",game/"meta",game,root})fs::remove(path,ignored);
    }
    void imageHeader(const std::string& name,uint32_t width,uint32_t height){
        //Only the runtime's PNG dimension reader is exercised, never a decoder.
        fs::create_directories(game/"images");const auto path=game/"images"/(name+".mzp.png");extraFiles.push_back(path);
        std::vector<uint8_t> png(24);const uint8_t magic[]={137,80,78,71,13,10,26,10};std::memcpy(png.data(),magic,8);png[11]=13;std::memcpy(png.data()+12,"IHDR",4);
        for(unsigned n=0;n<4;++n){png[16+n]=uint8_t(width>>(24-8*n));png[20+n]=uint8_t(height>>(24-8*n));}
        std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(png.data()),std::streamsize(png.size()));check(bool(file),"write private image dimension fixture");
    }
    std::string command(const char* name,const char* payload=""){
        std::string state=woh_command(handle,name,payload);if(state.find("\"error\":\"\"")==std::string::npos)throw std::runtime_error("Synthetic command "+std::string(name)+": "+state);
        const std::string type(name);if(type=="tick"){testPlatformLast=std::stoull(payload);testPlatformAnchor=true;}
        if(type=="resume"||type=="transitionReady"||type=="action"||type=="load"||type=="title"||number(state,"nextTickMs")==0)testPlatformAnchor=false;
        return state;
    }
    //Old local-waveform tests choose a phase, rather than test a long stalled
    //frame. Reach that phase with real bounded frames after the native50ms cap
    //was restored. Coarse scheduler tests deliberately call command directly.
    std::string pulseTick(const char* payload){const auto target=std::stoull(payload);if(!testPlatformAnchor||target<testPlatformLast)return command("tick",payload);std::string state;do{auto next=std::min(target,testPlatformLast+uint64_t(50));state=command("tick",std::to_string(next).c_str());}while(testPlatformLast<target);return state;}
    std::string firstComplete(){command("action","start");command("tick","1000");auto state=command("tick","1500");check(waiting(state,"input")&&!boolean(state,"revealing")&&number(state,"nextTickMs")==0,"first paragraph reaches idle input wait");return state;}
};
static Program appendProgram(){Program program;program.dialogue(0);program.op(0x142);program.dialogue(1);program.op(0xf4);return program;}
static std::string preloads(const std::string& state){auto first=state.find("\"preloads\":")+11;return state.substr(first,state.find(']',first)-first+1);}
static std::string visibleLayers(const std::string& state){
    auto first=state.find("\"layers\":");if(first==std::string::npos)throw std::runtime_error("Missing layer array");first+=9;
    while(first<state.size()&&(state[first]==' '||state[first]=='\t'||state[first]=='\r'||state[first]=='\n'))++first;
    if(first==state.size()||state[first]!='[')throw std::runtime_error("Invalid layer array");
    size_t depth=0;bool quoted=false,escaped=false;
    for(size_t at=first;at<state.size();++at){const char c=state[at];if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}else if(c=='"')quoted=true;else if(c=='[')++depth;else if(c==']'&&!--depth)return state.substr(first,at-first+1);}
    throw std::runtime_error("Unterminated layer array");
}
static std::string audioChannel(const std::string& state,const std::string& channel){auto at=state.find("\"channel\":\""+channel+"\"");check(at!=std::string::npos,"expected audio channel exists");auto first=state.rfind('{',at);return state.substr(first,state.find('}',at)-first+1);}
static void bgmLoopContract(){
    Program p;p.bgm(1,80);p.dialogue(0);p.bgm(1,40,1,1000);p.dialogue(1);p.bgm(1,60,0,10,9999);p.dialogue(0);p.bgm(1,60,0);p.dialogue(0);p.bgm(2,100,1,1000,500);p.dialogue(1);p.bgm(0,100,1,100);p.dialogue(0);p.bgm(2);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),false,"","","m01.hw 48000 48000 2 0 2400 24000\nm02.hw 48000 48000 2 0 1200 0\n");
    woh_destroy(f.handle);f.handle=0;auto music=f.game/"meta/music-map.tsv";f.extraFiles.push_back(music);{std::ofstream file(music);file<<"1\tm01.hw\n2\tm02.hw\n";}f.handle=woh_create(f.game.string().c_str());
    f.command("textSpeed","0");auto state=f.command("action","start"),a=audioChannel(state,"bgm");auto id=number(a,"id");
    check(boolean(a,"loop")&&number(a,"volume")==.8&&number(a,"oldFadeOutMs")==50&&number(a,"startFrame")==2400&&number(a,"endFrame")==24000,"180 loop bit overrides wrapper flag and exports exact partial range");
    state=f.command("advance");a=audioChannel(state,"bgm");check(number(a,"id")==id&&number(a,"volume")==.8,"same file and loop preserve position and current gain");
    f.command("tick","1000");state=f.command("tick","1484");check(std::abs(number(audioChannel(state,"bgm"),"volume")-.6)<1e-9,"same BGM fades existing gain without decode/restart");
    f.command("tick","1984");state=f.command("advance");a=audioChannel(state,"bgm");check(number(a,"id")!=id&&!boolean(a,"loop")&&a.find("loopRegion")==std::string::npos&&number(a,"oldFadeOutMs")==50,"changed loop creates a one-shot and short fade forces old50ms");
    auto once=uint64_t(number(a,"id"));state=f.command("audioEnded",("bgm "+std::to_string(once-1)).c_str());check(audioChannel(state,"bgm").find("m01.hw")!=std::string::npos,"stale one-shot completion cannot end active BGM");
    state=f.command("audioEnded",("bgm "+std::to_string(once)).c_str());check(audioChannel(state,"bgm").find("\"path\":\"\"")!=std::string::npos,"current one-shot completion marks inactive");
    state=f.command("advance");check(number(audioChannel(state,"bgm"),"id")!=once,"same one-shot restarts after actual completion");
    state=f.command("advance");a=audioChannel(state,"bgm");check(number(a,"oldFadeOutMs")==500&&number(a,"volume")==0&&number(a,"endFrame")==48000,"long new fade retains explicit old fade and normalizes native end0");
    state=f.command("advance");f.command("tick","10000");state=f.command("tick","10100");check(audioChannel(state,"bgm").find("\"path\":\"\"")!=std::string::npos,"180ID0 stops existing music without resolving a file");
    state=f.command("advance");a=audioChannel(state,"bgm");
    auto active=uint64_t(number(a,"id"));state=f.command("audioError",("bgm "+std::to_string(active-1)+" stale").c_str());check(!waiting(state,"error"),"stale audio failure ignored");
    state=woh_command(f.handle,"audioError",("bgm "+std::to_string(active)+" decoded-frame-count").c_str());check(waiting(state,"error")&&state.find("Audio playback failed")!=std::string::npos&&number(state,"nextTickMs")==0,"active decode failure becomes controlled error without a retry timer");
    f.command("title");f.passed=true;
}
static std::string layerObject(const std::string& state,int id){auto at=state.find("\"id\":"+std::to_string(id)+",");check(at!=std::string::npos,"expected layer exists");auto first=state.rfind('{',at);return state.substr(first,state.find('}',at)-first+1);}
static bool sameLayerXY(const std::string& a,const std::string& b,int id){auto left=layerObject(a,id),right=layerObject(b,id);return number(left,"x")==number(right,"x")&&number(left,"y")==number(right,"y");}
static std::string modalObject(const std::string& state){auto first=state.find("\"modal\":");check(first!=std::string::npos,"modal field exists");first+=8;return state.substr(first,state.find(",\"sharedGeneration\":",first)-first);}
static bool modalPhase(const std::string& state,const char* phase){return modalObject(state).find(std::string("\"phase\":\"")+phase+"\"")!=std::string::npos;}
static std::string modalButton(const std::string& state,const char* id){auto modal=modalObject(state);auto at=modal.find(std::string("\"id\":\"")+id+"\"");check(at!=std::string::npos,"expected TEATIME action exists");auto first=modal.rfind('{',at);return modal.substr(first,modal.find('}',at)-first+1);}
static std::string modalCommand(Fixture& fixture,const std::string& state,const char* action){auto modal=modalObject(state);auto payload=std::to_string(uint64_t(number(modal,"id")))+" "+std::to_string(uint64_t(number(modal,"revision")))+" "+action;return fixture.command("modalAction",payload.c_str());}
static std::vector<std::string> hostFields(const std::string& row){std::istringstream input(row);std::vector<std::string> values;std::string value;while(input>>std::quoted(value))values.push_back(value);return values;}
static void hostRow(std::ostream& output,const std::vector<std::string>& values,size_t count,int quoted=-1){check(values.size()>=count,"host fixture has all requested legacy columns");for(size_t n=0;n<count;++n){if(n)output<<' ';if(int(n)==quoted)output<<std::quoted(values[n]);else output<<values[n];}output<<'\n';}
static bool currentHostSchema(const mahoyo::save::Document& document){std::string text(document.hostExtra.begin(),document.hostExtra.end());return text.rfind("WOHHOST23\n",0)==0;}
// Intentional legacy fixture migration only: validate the entire20 suffix
// before dropping registry/paused/RNG/pool state unavailable in true WH19.
static mahoyo::save::Document host19Fixture(mahoyo::save::Document document){
    std::string extra(document.hostExtra.begin(),document.hostExtra.end());
    if(extra.rfind("WOHHOST19\n",0)==0)return document;
    check(currentHostSchema(document),"derive19 from actual20");
    std::istringstream input(extra);std::ostringstream output;std::string row;std::getline(input,row);output<<"WOHHOST19\n";
    auto take=[&](){check(bool(std::getline(input,row)),"complete20 fixture row");return row;};
    auto natural=[&](const std::string& value){size_t used=0;auto n=std::stoull(value,&used);check(used==value.size()&&value.find_first_not_of("0123456789")==std::string::npos,"20 fixture integer");return n;};
    auto count=[&](){auto fields=hostFields(take());check(fields.size()==1,"20 fixture count field");auto n=natural(fields[0]);check(n<=64,"20 fixture bounded count");output<<row<<'\n';return size_t(n);};
    std::set<int> layers;std::vector<int> active;
    for(size_t n=count();n;--n){auto text=take();auto fields=hostFields(text);check(fields.size()==31,"20 layer fields unchanged");auto id=natural(fields[0]);check(id<64&&layers.insert(int(id)).second,"20 fixture layer IDs");output<<text<<'\n';}
    for(size_t n=count();n;--n){auto text=take();auto fields=hostFields(text);check(fields.size()==35,"20 active packet unchanged");auto id=natural(fields[0]);check(id<64,"20 fixture active ID bounds");active.push_back(int(id));output<<text<<'\n';}
    for(;;){auto text=take();if(text.rfind("EFFECTREGISTRY1 ",0)==0)break;
        if(text.rfind("EXACTLAYOUT1 ",0)==0)continue;
        if(text.rfind("AUDIOPAN1 ",0)==0){auto fields=hostFields(text);check(fields.size()==2,"21 fixture pan count");for(size_t n=natural(fields[1]);n;--n)take();continue;}
        output<<text<<'\n';}
    std::ostringstream suffix;suffix<<row<<'\n'<<input.rdbuf();std::istringstream footer(suffix.str());
    (void)mahoyo::host_state20::read(footer,active,layers);(void)mahoyo::language::readTextState(footer,document.history.size());std::string trailing;check(!(footer>>trailing),"20 fixture footer consumed exactly");
    extra=output.str();document.hostExtra.assign(extra.begin(),extra.end());return document;
}
static mahoyo::save::Document host18Fixture(mahoyo::save::Document document){
    document=host19Fixture(std::move(document));std::string prefix(document.hostExtra.begin(),document.hostExtra.end());check(prefix.rfind("WOHHOST19\n",0)==0,"legacy fixture derives from validated WH19");std::istringstream input(std::string(document.hostExtra.begin(),document.hostExtra.end()));std::ostringstream output;std::string row;std::getline(input,row);output<<"WOHHOST18\n";
    auto take=[&](){check(bool(std::getline(input,row)),"complete current fixture row");return row;};
    auto natural=[&](const std::string& value){size_t used=0;auto n=std::stoull(value,&used);check(used==value.size()&&value.find_first_not_of("0123456789")==std::string::npos,"fixture count is a decimal integer");return n;};
    auto count=[&](){auto values=hostFields(take());check(values.size()==1,"fixture count has one field");auto n=natural(values[0]);check(n<=64,"bounded layer/effect fixture count");return size_t(n);};
    const auto layers=count();output<<layers<<'\n';std::array<bool,64> identities{};
    for(size_t n=0;n<layers;++n){auto text=take();auto values=hostFields(text);check(values.size()==31,"WH19 retains exactly31 layer fields");auto id=natural(values[0]);check(id<64&&!identities[id],"fixture layer IDs are unique");identities[id]=true;output<<text<<'\n';}
    auto packet=[&](){auto values=hostFields(take());check(values.size()==35,"WH19 effect packet has exactly35 fields");size_t used=0;auto decimal=std::stoll(values[34],&used);check(used==values[34].size()&&decimal>=-2147483648LL&&decimal<=2147483647LL,"appended decimal field is i32");values.pop_back();hostRow(output,values,34);};
    const auto effects=count();output<<effects<<'\n';for(size_t n=0;n<effects;++n)packet();
    for(;;){auto text=take();if(text.rfind("EFFECTBASES1 ",0)==0)break;check(text.rfind("INPUTREGION1 ",0)!=0&&text.rfind("BITMAPGEOMETRY1 ",0)!=0,"stopped effect footer precedes WH19-only footers");output<<text<<'\n';}
    auto stopped=hostFields(row);check(stopped.size()==2,"stopped fixture count has exact fields");auto bases=natural(stopped[1]);check(bases<=64,"bounded stopped effect fixture count");output<<row<<'\n';for(size_t n=0;n<bases;++n)packet();
    check(take()=="INPUTREGION1 0 0 0 0 0 0","WH19 fixture has neutral InputRegion placeholder");auto bitmap=hostFields(take());check(bitmap.size()==2&&bitmap[0]=="BITMAPGEOMETRY1"&&natural(bitmap[1])==layers,"bitmap footer matches layer count");std::array<bool,64> bitmapIds{};
    for(size_t n=0;n<layers;++n){auto values=hostFields(take());check(values.size()==3,"bitmap fixture row has exact fields");auto id=natural(values[0]);check(id<64&&identities[id]&&!bitmapIds[id]&&natural(values[1])>0&&natural(values[2])>0,"bitmap dimensions preserve every layer once");bitmapIds[id]=true;}
    check(!std::getline(input,row),"WH19 fixture has no trailing rows");auto extra=output.str();document.hostExtra.assign(extra.begin(),extra.end());return document;
}
static mahoyo::save::Document oldHostFixture(mahoyo::save::Document document,int schema,const char* footer){document=host18Fixture(std::move(document));std::string extra(document.hostExtra.begin(),document.hostExtra.end());auto at=extra.find(std::string("\n")+footer);check(at!=std::string::npos,"legacy footer boundary exists");extra.resize(at+1);extra.replace(0,9,"WOHHOST"+std::to_string(schema));document.hostExtra.assign(extra.begin(),extra.end());return document;}
static mahoyo::save::Document host17Fixture(mahoyo::save::Document document){return oldHostFixture(std::move(document),17,"WAITBASELINE1 ");}
static mahoyo::save::Document host16Fixture(mahoyo::save::Document document){return oldHostFixture(std::move(document),16,"HISTORYMETA1 ");}
static mahoyo::save::Document host15Fixture(mahoyo::save::Document document){return oldHostFixture(std::move(document),15,"SCREENWAVE1 ");}
static mahoyo::save::Document legacyHost(mahoyo::save::Document document,int schema){
    check(schema>=9&&schema<=11,"bounded synthetic legacy schema");document=host18Fixture(std::move(document));std::istringstream input(std::string(document.hostExtra.begin(),document.hostExtra.end()));std::ostringstream output;std::string row;size_t count;
    std::getline(input,row);check(row=="WOHHOST18","legacy fixture normalized to exact34-field packets");output<<"WOHHOST"<<schema<<'\n';
    input>>count;std::getline(input,row);output<<count<<'\n';for(size_t n=0;n<count;++n){std::getline(input,row);auto values=hostFields(row);check(values.size()==31,"current layer field count before legacy conversion");hostRow(output,values,26,6);}
    input>>count;std::getline(input,row);output<<count<<'\n';for(size_t n=0;n<count;++n){std::getline(input,row);auto values=hostFields(row);check(values.size()==34,"normalized effect packet field count before legacy conversion");hostRow(output,values,schema==11?29:17);}
    while(std::getline(input,row)){if(row.rfind("GEOMETRYMODES1 ",0)==0||(schema<10&&row.rfind("TEATIME1 ",0)==0))break;output<<row<<'\n';}
    auto extra=output.str();document.hostExtra.assign(extra.begin(),extra.end());return document;
}
static std::vector<std::string> savedLayerFields(const mahoyo::save::Document& document,int id){std::istringstream input(std::string(document.hostExtra.begin(),document.hostExtra.end()));std::string row;std::getline(input,row);size_t count;input>>count;std::getline(input,row);for(size_t n=0;n<count;++n){std::getline(input,row);auto values=hostFields(row);if(std::stoi(values[0])==id)return values;}throw std::runtime_error("missing synthetic saved layer");}
struct InspectHost final:mahoyo::vm::Host {
    std::vector<uint8_t> loadModule(const std::string&)override{throw std::runtime_error("VM inspector cannot execute modules");}
    mahoyo::vm::Flow invoke(uint32_t,mahoyo::vm::Machine&)override{throw std::runtime_error("VM inspector cannot execute host calls");}
};
static Program teatimeProgram(int selector=1,int marker=12345){Program p(false);p.image(1000,"BLACK");p.push(marker);p.push(selector);p.op(0x13c);p.push(0);p.op(0x153);p.push(0);p.op(0x251);p.dialogue(0);p.op(0xf4);return p;}
static std::string readyTeatime(Fixture& fixture){fixture.command("textSpeed","0");auto state=fixture.command("action","start");check(waiting(state,"teatime")&&modalPhase(state,"opening"),"13c enters an actual modal wait");fixture.command("tick","1000");state=fixture.command("tick","1984");check(modalPhase(state,"ready")&&number(state,"nextTickMs")==0,"opening1000ms reaches unscheduled Ready");return state;}
static std::string finishTeatime(Fixture& fixture,const std::string& state,uint64_t platform=1000000){auto closing=modalCommand(fixture,state,"next");check(modalPhase(closing,"closing"),"Next starts a modal close rather than skipping13c");fixture.command("tick",std::to_string(platform).c_str());return fixture.command("tick",std::to_string(platform+484).c_str());}
static void teatimeOpeningIdleAndNext(){
    Fixture f(teatimeProgram());f.command("textSpeed","0");auto state=f.command("action","start");auto instructions=number(state,"instructions");
    check(waiting(state,"teatime")&&state.find("\"scene\":\"teatime\"")!=std::string::npos&&modalPhase(state,"opening"),"13c yields into an intermission scene without advancing its VM");
    check(number(state,"nextTickMs")==16&&!boolean(state,"canSave")&&!boolean(modalButton(state,"archive"),"enabled"),"opening exposes the native Archive action only as disabled until Ready");
    check(number(modalButton(state,"save"),"alpha")==0&&number(modalButton(state,"title"),"alpha")==0&&number(modalButton(state,"next"),"alpha")==0,"native Save/Title/Next parts begin transparent");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"opening cannot create a premature slot");f.command("tick","1000");state=f.command("tick","1384");
    check(number(modalButton(state,"save"),"alpha")==.5&&number(modalButton(state,"title"),"alpha")==0&&number(modalButton(state,"next"),"alpha")==0,"400ms shows only half of native event1 Save");
    state=f.command("tick","1983");check(modalPhase(state,"opening")&&!boolean(modalButton(state,"next"),"enabled"),"input remains disabled at999ms");state=f.command("tick","1984");
    check(modalPhase(state,"ready")&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"native1000ms gate releases Ready without idle rendering");
    const auto ready=state;const auto layers=visibleLayers(state);for(const auto* command:{"advance","auto","skip","hide","history","title"}){state=f.command(command);check(modalPhase(state,"ready")&&number(state,"instructions")==instructions&&visibleLayers(state)==layers,"game controls cannot auto-close or replace a modal");}
    state=f.command("action","start");check(number(state,"instructions")==instructions&&!boolean(state,"automatic")&&!boolean(state,"skipping"),"new-game/auto/skip input cannot bypass13c");
    auto id=uint64_t(number(modalObject(state),"id")),revision=uint64_t(number(modalObject(state),"revision"));
    for(const auto& payload:{std::to_string(id+1)+" "+std::to_string(revision)+" next",std::to_string(id)+" "+std::to_string(revision-1)+" next",std::to_string(id+1)+" "+std::to_string(revision)+" archive"}){state=f.command("modalAction",payload.c_str());check(modalPhase(state,"ready")&&number(state,"instructions")==instructions,"stale IDs/revisions for Next and Archive remain inert");}
    state=f.command("tick","900000");check(number(state,"nextTickMs")==0&&number(state,"instructions")==instructions,"arbitrary idle wall time cannot advance the suspended VM");
    state=modalCommand(f,state,"next");check(modalPhase(state,"closing")&&!boolean(state,"canSave"),"Next locks interaction until its500ms close completes");
    f.command("tick","1000000");state=f.command("tick","1000483");check(modalPhase(state,"closing")&&number(state,"instructions")==instructions&&std::abs(number(modalObject(state),"alpha")-132./65536)<1e-9,"499ms close preserves the same suspended VM and native fixed16 alpha");
    state=f.command("tick","1000484");check(waiting(state,"input")&&modalObject(state)=="null"&&number(state,"z")==12345&&number(state,"instructions")>instructions,"500ms resumes at the next opcode with exactly one13c pop and no result push");
    const auto finished=number(state,"instructions");state=f.command("modalAction",(std::to_string(id)+" "+std::to_string(revision)+" next").c_str());check(number(state,"instructions")==finished&&waiting(state,"input"),"late modal input never consumes the next paragraph");f.passed=true;
}
static void teatimePauseSaveReturnAndRestore(){
    Fixture f(teatimeProgram(8,6789));f.command("textSpeed","0");auto state=f.command("action","start");f.command("tick","1000");state=f.command("tick","1384");auto part=modalButton(state,"save");
    f.command("pause");state=f.command("tick","900000");check(modalButton(state,"save")==part&&number(state,"nextTickMs")==0,"pause holds opening control alpha and removes polling");f.command("resume");f.command("tick","900000");state=f.command("tick","900584");check(modalPhase(state,"ready"),"resume consumes only remaining logical opening time");
    auto instructions=number(state,"instructions");state=modalCommand(f,state,"save");auto saving=state;
    check(modalPhase(state,"saving")&&number(modalObject(state),"alpha")==0&&number(state,"nextTickMs")==0&&boolean(state,"canSave"),"Save hides immediately with no fade and keeps the modal suspended");
    f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(good.hostExtra.begin(),good.hostExtra.end());check(currentHostSchema(good)&&extra.find("TEATIME1 8\nCOMMON1 ")!=std::string::npos,"portable slot records ready-restorable selector and common generation");
    InspectHost host;mahoyo::vm::Machine saved(host);saved.loadState(good.vm);check(saved.stack()==std::vector<int32_t>{6789},"saving13c preserves its untouched lower operand and no return value");
    state=modalCommand(f,state,"save-return");check(modalPhase(state,"ready")&&number(modalObject(state),"alpha")==1&&number(state,"instructions")==instructions&&number(state,"nextTickMs")==0,"Save return immediately restores Ready without resuming or replaying opening");
    state=modalCommand(f,saving,"save-return");check(modalPhase(state,"ready")&&number(state,"instructions")==instructions,"duplicate Save callback is stale");state=modalCommand(f,state,"save");
    state=f.command("tick","9999999");check(modalPhase(state,"saving")&&number(state,"instructions")==instructions,"Save UI never closes by elapsed time");auto oldId=number(modalObject(state),"id");
    state=f.command("load","2");check(modalPhase(state,"ready")&&number(modalObject(state),"id")!=oldId&&number(state,"instructions")==instructions&&number(state,"nextTickMs")==0,"loading a Saving slot creates a fresh immediately Ready modal without executing13c again");
    state=modalCommand(f,saving,"save-return");check(modalPhase(state,"ready")&&number(state,"instructions")==instructions,"old Save window cannot act on the loaded modal generation");
    //Drop the five WH12 layer columns as well as newer effect columns; merely
    //changing the signature would leave an invalid legacy stream.
    auto legacy11=legacyHost(good,11);mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",legacy11);state=f.command("load","2");check(modalPhase(state,"ready")&&number(state,"instructions")==instructions,"actual26-column legacyWOHHOST11 layers remain readable");
    auto legacy=legacyHost(good,10);mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",legacy);state=f.command("load","2");check(modalPhase(state,"ready")&&number(state,"instructions")==instructions,"legacyWOHHOST10 intermission slots remain readable");
    state=modalCommand(f,state,"title");check(state.find("\"scene\":\"title\"")!=std::string::npos&&modalObject(state)=="null"&&number(state,"nextTickMs")==0,"native Title closes immediately and never schedules a later VM resume");
    state=f.command("load","2");state=modalCommand(f,state,"next");f.command("tick","2000000");state=f.command("tick","2000234");check(number(modalObject(state),"alpha")==.5,"Next quarter-second close uses logical time");
    f.command("pause");state=f.command("tick","3000000");check(number(modalObject(state),"alpha")==.5,"paused wall time cannot finish Next");f.command("resume");f.command("tick","3000000");state=f.command("tick","3000233");check(modalPhase(state,"closing"),"resume at499ms still holds the same VM");state=f.command("tick","3000234");check(waiting(state,"input")&&number(state,"z")==6789&&modalObject(state)=="null","loaded current VM resumes once at500ms, not from title/new game");f.passed=true;
}
static void teatimeNumericPopupState(){
    for(int selector:{1,2,8,9}){Fixture f(teatimeProgram(selector));woh_destroy(f.handle);f.handle=0;mahoyo::shared::State seed;seed.generation=40;seed.variables.resize(4096);seed.flags.resize(16384);seed.variables[26]=1<<5;seed.flags[26]=1;mahoyo::shared::write(f.root/"savedata/shared.whp",seed);f.handle=woh_create(f.game.string().c_str());auto state=readyTeatime(f);auto common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common.has_value(),"TEATIME writes shared state independently of slots");int expected=(1<<5)|((selector==1||selector==8||selector==9)?1<<selector:0);check(common->variables[26]==expected&&common->flags[26]==1,"selector popup writes numeric26 bits without treating global flag26 as its gate");check(common->generation==uint64_t(selector==2?40:41),"only changed shared progress increments its generation");check(modalObject(state).find(selector==2?"\"theme\":\"white\"":"\"theme\":\"black\"")!=std::string::npos,"selector maps ink palette rather than an invented catalog");state=finishTeatime(f,state);check(waiting(state,"input")&&number(state,"z")==12345,"both selector palettes resume the same current VM");check(!fs::exists(f.root/"savedata/slot2.whs"),"first popup progress does not auto-create story slots");f.passed=true;}
}
static void sharedProgressSurvivesOlderSlotAndFreshProcess(){
    Program p;p.push(111);p.push(5);p.op(0xe0);p.dialogue(0);p.push(73);p.push(17);p.op(0xe2);p.push(1);p.push(9);p.op(0xe6);p.push(222);p.push(5);p.op(0xe0);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");f.command("action","start");f.command("save","2");auto old=mahoyo::save::read(f.root/"savedata/slot2.whs");
    f.command("advance");f.command("pause");auto advanced=mahoyo::shared::read(f.root/"savedata/shared.whp");check(advanced&&advanced->variables[17]==73&&advanced->flags[9]==1,"pause persists both changed shared numbers and shared flags");f.command("resume");auto state=f.command("load","2");check(number(state,"sharedGeneration")==advanced->generation,"older slot cannot roll common generation backwards");
    f.command("save","2");auto loaded=mahoyo::save::read(f.root/"savedata/slot2.whs");InspectHost host;mahoyo::vm::Machine vm(host),previous(host);vm.loadState(loaded.vm);previous.loadState(old.vm);check(vm.variable(17,true)==73&&vm.flag(9,true)&&vm.variable(5)==111&&vm.pc()==previous.pc(),"old local PC/variables restore while newer shared progress wins");
    f.command("title");f.command("action","start");f.command("save","2");auto fresh=mahoyo::save::read(f.root/"savedata/slot2.whs");vm.loadState(fresh.vm);check(vm.variable(17,true)==73&&vm.flag(9,true)&&vm.variable(5)==111,"new game preserves shared manager and creates fresh local story state");
    woh_destroy(f.handle);f.handle=0;auto before=mahoyo::shared::read(f.root/"savedata/shared.whp");f.handle=woh_create(f.game.string().c_str());auto after=mahoyo::shared::read(f.root/"savedata/shared.whp");check(before&&after&&before->generation==after->generation&&before->variables==after->variables&&before->flags==after->flags,"fresh title process cannot overwrite external progress with an empty temporary VM");
    f.command("load","2");f.command("save","2");vm.loadState(mahoyo::save::read(f.root/"savedata/slot2.whs").vm);check(vm.variable(17,true)==73&&vm.flag(9,true),"fresh process loads shared state before restoring a story slot");f.passed=true;
}
static void legacySlotAdoptsSharedWithoutCommonFile(){
    Program p;p.push(99);p.push(17);p.op(0xe2);p.push(1);p.push(9);p.op(0xe6);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");f.command("action","start");f.command("save","2");auto legacy=mahoyo::save::read(f.root/"savedata/slot2.whs");
    std::string extra(legacy.hostExtra.begin(),legacy.hostExtra.end());check(extra.find("TEATIME1 ")!=std::string::npos,"private current slot contains an intermission/common suffix");legacy=legacyHost(legacy,9);woh_destroy(f.handle);f.handle=0;mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",legacy);check(fs::remove(f.root/"savedata/shared.whp"),"remove only this synthetic common fixture");
    f.handle=woh_create(f.game.string().c_str());check(!fs::exists(f.root/"savedata/shared.whp"),"fresh title without common data must not manufacture an empty shared file");auto state=f.command("load","2");auto common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common&&common->generation>0&&common->variables[17]==99&&common->flags[9]==1,"legacy slot imports its own shared manager when common file is absent");
    f.command("save","2");auto old=mahoyo::save::read(f.root/"savedata/slot2.whs");InspectHost host;mahoyo::vm::Machine vm(host);vm.loadState(old.vm);check(vm.variable(17,true)==99&&vm.flag(9,true),"legacy adoption persists into the current portable slot");
    auto imported=old;vm.setVariable(17,777,true);vm.setFlag(9,false,true);imported.vm=vm.saveState();extra.assign(imported.hostExtra.begin(),imported.hostExtra.end());auto epoch=extra.find("COMMON1 ");check(epoch!=std::string::npos,"current slot has a common generation marker");auto end=extra.find('\n',epoch);auto newer=common->generation+50;extra.replace(epoch,end-epoch,"COMMON1 "+std::to_string(newer));imported.hostExtra.assign(extra.begin(),extra.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",imported);state=f.command("load","2");common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common&&common->generation==newer&&common->variables[17]==777&&common->flags[9]==0,"a portable slot with a newer common epoch replaces older external globals atomically");
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",old);f.command("load","2");f.command("save","2");vm.loadState(mahoyo::save::read(f.root/"savedata/slot2.whs").vm);check(vm.variable(17,true)==777&&!vm.flag(9,true),"loading the old slot again cannot relock or revert the newer shared state");f.passed=true;
}
static std::string commitNext(Fixture& fixture,uint64_t& clock){
    auto state=fixture.command("advance");check(waiting(state,"transitionReady"),"queued200 waits for the graphics barrier");
    auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));fixture.command("transitionReady",id.c_str());
    state=fixture.command("tick",std::to_string(clock+=16).c_str());check(waiting(state,"input"),"cut commit resumes the next text wait");return state;
}
static void imagePreloadPopAndVisibility(){
    Program p;p.image();p.push(77);p.range(30,"MISSING.mzp",3,-7,1,2);p.range(64,"invalid",0,0,9,9);p.range(12,"",1,2,3,4);
    p.preload(50,"");p.preload(-42,"MISSING",-9,23,-4,8,70,int32_t(0xaabbccddu));p.op(0x221);p.op(0x131);p.op(0x251);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);auto state=f.firstComplete();auto hints=preloads(state);
    check(number(state,"z")==77,"220 consumes6,221 consumes0,222 consumes8 and none push a result");
    check(hints.find("\"base\":\"missing.mzp.mzp\"")!=std::string::npos,"preload adds native extension unconditionally");
    check(hints.find("\"xMin\":3,\"yMin\":-7,\"xMax\":1,\"yMax\":2")!=std::string::npos,"reverse and negative inclusive ranges remain unmodified");
    check(hints.find("\"tag\":-42,\"variantX\":-4,\"variantY\":8")!=std::string::npos,"opaque tag and single variants are preserved");
    check(hints.find("\"brightness\":-9,\"contrast\":23,\"colorMode\":70,\"rgb\":12307677")!=std::string::npos,"222 retains all native filter operands and low24bit RGB");
    check(visibleLayers(state).find("missing")==std::string::npos&&visibleLayers(state).find("\"id\":30")==std::string::npos,"missing warm-cache hints do not create visible layers or read files");
    check(hints.find("invalid")==std::string::npos&&hints.find("\"layerId\":12")==std::string::npos,"invalid layer andNULL preload name still consume operands without changing state");
    check(number(state,"seenImageCount")==0,"preload operations do not unlock gallery images");f.passed=true;
}
static void imagePreloadCommitSaveAndRelease(){
    Program p;p.image();p.push(0x123456);p.push(70);p.push(0);p.op(0x253);p.push(9);p.push(0);p.op(0x254);p.push(-8);p.push(0);p.op(0x255);
    p.range(0,"OLD_RANGE",1,2,3,4);p.preload(25,"old_image");p.dialogue(0);p.commit();p.dialogue(1);
    p.preload(26,"new_image",5,6,2,3,12,0xabcdef);p.op(0x221);p.dialogue(0);p.commit();p.dialogue(1);
    p.op(0x221);p.dialogue(0);p.commit();p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");auto layers=visibleLayers(state),initial=preloads(state);
    check(initial.find("\"brightness\":-8,\"contrast\":9,\"colorMode\":70,\"rgb\":1193046")!=std::string::npos,"220 snapshots its existing layer filter without altering the layer");
    f.command("save","2");auto path=f.root/"savedata/slot2.whs";auto good=mahoyo::save::read(path);
    std::string extension(good.hostExtra.begin(),good.hostExtra.end());check(currentHostSchema(good)&&extension.find("PRELOADS1 0\n0\n1\n")!=std::string::npos,"current host schema records pending ranges and images before a commit");
    f.command("title");state=f.command("load","2");check(preloads(state)==initial&&visibleLayers(state)==layers,"pending preload intentions roundtrip without reissuing CHS or touching pixels");
    uint64_t clock=1000;state=commitNext(f,clock);check(preloads(state)==initial,"200 transfers pending intentions to retained state");
    state=f.command("advance");auto replacement=preloads(state);
    check(replacement.find("new_image.mzp")!=std::string::npos&&replacement.find("old_")==std::string::npos,"221 removes effective old hints while retaining new222 queued before it");
    check(visibleLayers(state)==layers,"preload release does not delete or move visible layers");
    f.command("save","2");good=mahoyo::save::read(path);extension.assign(good.hostExtra.begin(),good.hostExtra.end());check(extension.find("PRELOADS1 1\n")!=std::string::npos,"pending release intent is saved separately from retained/new queues");
    f.command("title");state=f.command("load","2");check(preloads(state)==replacement,"releaseOld plus pending222 survives a slot restore");
    state=commitNext(f,clock);check(preloads(state)==replacement,"commit releases old entries before appending the new pending image");
    state=f.command("advance");check(preloads(state)=="[]"&&visibleLayers(state)==layers,"221 with no new pending hint clears effective cache only");
    state=commitNext(f,clock);check(preloads(state)=="[]"&&visibleLayers(state)==layers,"empty release commit leaves the scene intact");
    auto bad=good;auto text=std::string(bad.hostExtra.begin(),bad.hostExtra.end());auto at=text.find("PRELOADS1 1");check(at!=std::string::npos,"release fixture is present");text.replace(at,11,"PRELOADS1 2");bad.hostExtra.assign(text.begin(),text.end());mahoyo::save::writeAtomic(path,bad);
    auto previous=state;state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&preloads(state)==preloads(previous)&&visibleLayers(state)==layers,"invalid hint state cannot partially mutate a live slot load");
    //Version6 has19 layer/11 effect fields and no later extensions.
    auto normalized=host18Fixture(good);std::istringstream current(std::string(normalized.hostExtra.begin(),normalized.hostExtra.end()));std::ostringstream old;std::string row;size_t count;
    std::getline(current,row);current>>count;std::getline(current,row);old<<"WOHHOST6\n"<<count<<'\n';
    for(size_t i=0;i<count;++i){std::getline(current,row);auto values=hostFields(row);check(values.size()==31,"WH6 conversion starts with31 layer fields");hostRow(old,values,19,6);}
    current>>count;std::getline(current,row);old<<count<<'\n';for(size_t i=0;i<count;++i){std::getline(current,row);auto values=hostFields(row);check(values.size()==34,"WH6 conversion starts with normalized34 effect fields");hostRow(old,values,11);}
    text=old.str();good.hostExtra.assign(text.begin(),text.end());mahoyo::save::writeAtomic(path,good);
    state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&preloads(state)=="[]"&&visibleLayers(state)==layers,"WOHHOST6 remains readable with empty preload intentions");f.passed=true;
}

static void effectCaptureWindowSave(){
    Program p;p.image(1000,"fake",0,{{870,0,889,1080,1,75}});p.dialogue(0);p.push(205);p.push(0);p.op(0x279);for(int n:{0,0,0,205,0})p.push(n);p.op(0x278);p.commit();p.dialogue(1);p.op(0xf4);
    std::string effects="205 0 0 0 64 -32767 -32767 -100001 0 0 0 1000\n205 1000 50 50 0 -32767 -32767 -100001 0 0 0 1000\n";
    std::string windows="205 0 692 0x4144080 1 2 -32767 -32767 -32767 -32767\n205 1 693 0x4144080 -32767 -32767 -32767 -32767 -32767 -32767\n";
    Fixture f(std::move(p),true,effects,"","",windows);f.command("textSpeed","0");auto initial=f.command("action","start");check(initial.find("\"softEdge\":75")!=std::string::npos,"240 retains exact native canvas window fields");
    //Native A3E70 quantizes captured raw50 to49.5 before the*4-pivot mapping.
    uint64_t clock=1000;auto state=commitNext(f,clock);auto layer=layerObject(state,0);check(number(layer,"x")==148&&number(layer,"y")==148&&number(layer,"width")==889&&number(layer,"softEdge")==75,"279 captures old raw anchor/window before new278 and200, then applies native coordinate grid");
    check(number(layer,"alpha")>.98,"captured alpha255 replaces the original first alpha64");
    f.command("save","2");check(fs::exists(f.root/"savedata/slot2.whs"),"active effects can serialize their live override plus initial window");
    auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(good.hostExtra.begin(),good.hostExtra.end());check(extra.find("EFFECTFRAMES1 1\n692 50 50 255 870 0 889 1080\n")!=std::string::npos,"global first-row override is separate from playback state");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());auto loaded=f.command("load","2");check(visibleLayers(loaded)==visibleLayers(state),"fresh process restores captured fields and playback exactly");
    f.command("tick","2000");state=f.command("tick","2484");layer=layerObject(state,0);check(number(layer,"width")==889&&number(layer,"x")==148,"later hold continues captured native grid/window after fresh load");
    check(std::abs(number(layer,"alpha")-.484)<.00001,"restored effect continues its retained timeline");
    f.passed=true;
}

static void parentWindowOffsetAndDetach(){
    Program p;p.image(1000,"fake",0,{{870,0,889,1080,1,75}});p.image(1000,"fake",1,{},0);p.image(1000,"fake",2,{},1);p.dialogue(0);p.push(0);p.op(0x241);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(number(layerObject(state,1),"x")==1020&&number(layerObject(state,1),"parent")==0,"child output receives parent window X without inheriting image scale");
    f.command("save","2");auto doc=mahoyo::save::read(f.root/"savedata/slot2.whs");check(doc.layers[1].x==150,"portable base retains child-local canonical coordinates");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(number(layerObject(state,1),"x")==1020,"WH10 restores parent graph and applies window offset once");
    state=f.command("advance");check(number(layerObject(state,1),"parent")==-1&&number(layerObject(state,1),"x")==150&&number(layerObject(state,2),"parent")==1,"deleting a parent detaches only direct children and preserves their local coordinates");f.passed=true;
}
static void nativeEffectWaitMask(){
    Program p;p.image();for(int n:{0,0,0,205,0})p.push(n);p.op(0x278);for(int n:{1,63,0})p.push(n);p.op(0x27f);p.dialogue(0);p.op(0xf4);
    const std::string table="205 0 -32767 -32767 0 -32767 -32767 -100001 0 0 0 500\n205 500 -32767 -32767 255 -32767 -32767 -100001 0 0 0 500\n";
    Fixture f(std::move(p),true,table);auto state=f.command("action","start");check(waiting(state,"transitionReady"),"27f first prepares its pending effect graphics");state=f.command("tick","99999");check(waiting(state,"transitionReady")&&number(layerObject(state,0),"alpha")==0,"first keyframe is applied before preparation and loading cannot consume its fade");f.command("transitionReady","1");f.command("tick","1000");state=f.command("tick","1483");check(waiting(state,"animation"),"effect wait does not finish at499ms");state=f.command("tick","1484");check(waiting(state,"input")&&number(layerObject(state,0),"alpha")==1,"effect wait prepares and retains its exact500ms endpoint before text");f.passed=true;
}
static void nativeEffectWaitSkipsLoops(){
    const std::string loop="205 0 -32767 -32767 0 -32767 -32767 -100001 0 0 200 1000\n205 200 -32767 -32767 255 -32767 -32767 -100001 0 0 200 1000\n205 1000 -32767 -32767 0 -32767 -32767 -100001 0 0 200 1000\n";
    auto effect=[](Program& p,int layer,int clip){for(int n:{0,0,0,clip,layer})p.push(n);p.op(0x278);};
    auto wait=[](Program& p,int layer,int mask){for(int n:{1,mask,layer})p.push(n);p.op(0x27f);};
    {
        Program p;p.image();p.push(77);effect(p,0,205);wait(p,0,32);p.push(0);p.op(0x251);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,loop);f.command("textSpeed","0");auto state=f.command("action","start");
        check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0,"27f loop-only query still prepares graphics before querying its zero wait duration");auto blocked=number(state,"instructions");state=f.command("tick","900000");check(waiting(state,"transitionReady")&&number(state,"instructions")==blocked&&number(layerObject(state,0),"alpha")==0,"loading cannot consume the loop or advance past27f");state=f.command("transitionReady","1");check(waiting(state,"input")&&state.find("\"transition\":null")!=std::string::npos&&number(state,"z")==77,"loop-only GPU acknowledgement resumes immediately without an extra tick or return value");auto instructions=number(state,"instructions");check(number(state,"nextTickMs")==16,"loop-only wait leaves the effect alive and scheduled");f.command("tick","1000");state=f.command("tick","1584");check(std::abs(number(layerObject(state,0),"alpha")-.5)<1e-8,"effect still animates after27f has resumed the VM");state=f.command("tick","2184");check(waiting(state,"input")&&std::abs(number(layerObject(state,0),"alpha")-.75)<1e-8&&number(state,"nextTickMs")==16&&number(state,"instructions")==instructions,"27f neither deletes nor cancels the continuing effect across its loop boundary");f.passed=true;
    }
    {
        Program p;p.image();effect(p,0,205);p.position(0,75,50,300);wait(p,0,33);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,loop);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"loop effect cannot short-circuit a simultaneous finite motion wait");f.command("transitionReady","1");f.command("tick","1000");state=f.command("tick","1283");check(waiting(state,"animation"),"finite motion still waits through299ms");state=f.command("tick","1284");check(waiting(state,"input")&&number(state,"nextTickMs")==16,"27f resumes at the finite motion deadline while loop effect stays active");state=f.command("tick","2184");check(std::abs(number(layerObject(state,0),"alpha")-.75)<1e-8,"loop remains scheduled after an unrelated motion finishes");f.passed=true;
    }
    {
        Program p;p.image();p.image(1000,"fake",1);effect(p,0,205);effect(p,1,206);wait(p,-1,32);p.dialogue(0);p.op(0xf4);
        const std::string finite="206 0 -32767 -32767 0 -32767 -32767 -100001 0 0 0 300\n206 300 -32767 -32767 255 -32767 -32767 -100001 0 0 0 300\n";
        Fixture f(std::move(p),true,loop+finite);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"all-layer event32 skips an earlier looping backend2 job but still waits for a later one-shot job");f.command("transitionReady","1");f.command("tick","1000");state=f.command("tick","1283");check(waiting(state,"animation"),"one-shot backend2 does not finish at299ms");state=f.command("tick","1284");check(waiting(state,"input")&&number(layerObject(state,1),"alpha")==1&&number(state,"nextTickMs")==16,"all-layer effect query finishes at300ms without waiting for or removing the1000ms loop");state=f.command("tick","2184");check(std::abs(number(layerObject(state,0),"alpha")-.75)<1e-8&&number(layerObject(state,1),"alpha")==1,"finished one-shot remains at its endpoint and the unrelated loop keeps repeating");f.passed=true;
    }
    {
        Program p;p.image();p.push(77);wait(p,0,32);p.push(0);p.op(0x251);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0,"bare27f with no active job still has a graphics-ready barrier");state=f.command("transitionReady","1");check(waiting(state,"input")&&number(state,"z")==77&&number(state,"nextTickMs")==0,"empty job query resumes directly on ready acknowledgement without a synthetic frame delay");f.passed=true;
    }
}

static void nativeWindowModeState(){
    Program p;p.image();p.push(77);p.push(0);p.op(0x154);p.push(0);p.op(0x251);p.dialogue(0);p.push(7);p.op(0x154);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(number(state,"z")==77&&number(state,"windowMode")==1&&number(state,"windowAlphaFixed16")==0&&!boolean(state,"windowVisible"),"154 consumes only1 and mode1 suppresses standard window auto-show");
    f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(number(state,"windowMode")==1&&number(state,"windowAlphaFixed16")==0,"stable automatic-display mode persists in WINDOWFADE2");
    state=f.command("advance");check(number(state,"windowMode")==0&&boolean(state,"windowVisible")&&number(state,"windowAlphaFixed16")==0,"nonzero154 enables the following140 auto-show without directly forcing opacity");
    f.command("tick","1000");state=f.command("tick","1484");check(number(state,"windowAlphaFixed16")==65536&&boolean(state,"canSave"),"enabled native window completes its usual timeline");f.passed=true;
}
static void nativeTrophyGatesAndPersistence(){
    Program p;p.image();p.push(77);p.push(1);p.op(0x138);p.push(0);p.op(0x251);p.dialogue(0);
    p.push(16);p.op(0x138);p.dialogue(1);
    for(int bit:{153,178,181,187,196,197,200,205}){p.push(bit);p.op(0x128);}p.push(16);p.op(0x138);p.dialogue(0);
    p.push(18);p.op(0x138);p.dialogue(1);
    for(int bit:{129,130,131,132,133,134}){p.push(bit);p.op(0x128);}p.push(19);p.op(0x138);p.dialogue(0);
    for(int bit:{6,7,8,9}){p.push(bit);p.op(0x128);}p.push(25);p.op(0x138);p.dialogue(1);
    for(int id=1;id<=26;++id){p.push(id);p.op(0x138);}p.push(INT32_MAX);p.op(0x138);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(number(state,"z")==77&&number(state,"trophyCount")==1&&!boolean(state,"platinumUnlocked"),"138 consumes1/noPush/noYield and unlocks the requested normal trophy");
    state=f.command("advance");check(number(state,"trophyCount")==1,"16 gate does not unlock without all original global flags");
    state=f.command("advance");check(number(state,"trophyCount")==2,"16 uses global flag bitset, independently of numeric variables");
    state=f.command("advance");check(number(state,"trophyCount")==3,"18 independently unlocks but cannot bypass the24 gate");
    state=f.command("advance");check(number(state,"trophyCount")==5,"19 plus complete six-bit gate unlock19 and24");
    state=f.command("advance");check(number(state,"trophyCount")==6,"25 requires global6/7/8/9");
    state=f.command("advance");check(number(state,"trophyCount")==26&&boolean(state,"platinumUnlocked"),"all26 produce platinum and unknown selectors do not manufacture extra trophies");
    check(!fs::exists(f.root/"savedata/slot1.whs")&&!fs::exists(f.root/"savedata/slot2.whs"),"achievement persistence is independent of portable slots");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=woh_snapshot(f.handle);check(number(state,"trophyCount")==26&&boolean(state,"platinumUnlocked"),"new process restores Android local trophies without replaying the game");f.command("textSpeed","0");state=f.command("action","start");check(number(state,"trophyCount")==26,"new game cannot relock global achievements");f.passed=true;
}
static void nativeDialogueWindowLifecycle(){
    Program p(false);p.dialogue(0);p.push(300);p.op(0x152);p.commit();p.push(0);p.push(1);p.push(400);p.op(0x110);p.push(300);p.op(0x153);p.commit();p.dialogue(1);p.op(0x151);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");check(number(state,"windowAlphaFixed16")==0&&boolean(state,"windowVisible")&&!boolean(state,"canSave"),"140 own prepare starts auto show and guards its unsaved timeline");
    f.command("tick","1000");state=f.command("tick","1484");check(number(state,"windowAlphaFixed16")==65536&&boolean(state,"canSave"),"auto show reaches idle input");
    state=f.command("advance");check(waiting(state,"transitionReady")&&number(state,"windowAlphaFixed16")==65536,"152 queues without yielding or immediately fading the old window");
    f.command("transitionReady","1");state=f.command("tick","2000");check(!boolean(state,"windowVisible")&&number(state,"windowAlphaFixed16")>0,"logical hidden flag does not discard remaining fade alpha");
    state=f.command("tick","2134");check(number(state,"windowAlphaFixed16")==0,"hide is complete at the half-duration point");
    state=f.command("tick","2400");check(waiting(state,"transitionReady")&&number(state,"windowAlphaFixed16")==0,"153 records its own pending show after the existing400ms wait");
    f.command("transitionReady","2");state=f.command("tick","3000");check(waiting(state,"input")&&number(state,"windowAlphaFixed16")==0,"140 preserves explicit153 duration and text rather than replacing it");
    state=f.command("tick","3134");check(number(state,"windowAlphaFixed16")==0,"show keeps the first half hidden");
    state=f.command("tick","3209");check(number(state,"windowAlphaFixed16")==32768,"show starts only in the second half");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"active window fade blocks incomplete snapshots");
    state=f.command("tick","3284");check(number(state,"windowAlphaFixed16")==65536&&boolean(state,"canSave"),"show completes at exact end and returns safe idle");
    f.command("save","2");f.command("title");state=f.command("load","2");check(number(state,"windowAlphaFixed16")==65536&&boolean(state,"windowVisible"),"WH10 restores stable window state");
    state=f.command("advance");check(waiting(state,"frame")&&state.find("\"text\":[]")!=std::string::npos,"151 shares141 text clearing without deleting the backlog");f.passed=true;
}
static void nativeZeroResultPreservesStack(){
    Program program;program.image();program.push(17);for(int i=0;i<5;++i)program.push(1234);program.op(0x201);program.op(0x131);program.op(0x251);program.dialogue(0);program.op(0xf4);
    Fixture fixture(std::move(program),true);auto state=fixture.firstComplete();
    check(number(state,"z")==17,"native0x201 consumes exactly five values and0x131 pushes zero without consuming the remaining value");fixture.passed=true;
}
static void solidColorScaleWithoutAsset(){
    Program program;program.image(1000,"WHITE",23);for(int n:{0,0,0,100,100,100,100,23})program.push(n);program.op(0x246);program.dialogue(0);program.op(0xf4);
    Fixture fixture(std::move(program));auto state=fixture.firstComplete();auto layer=layerObject(state,23);
    check(number(layer,"w")==192&&number(layer,"h")==108,"solid canvas uses1920x1080 extent without requesting an empty image name");
    check(number(layer,"x")==864&&number(layer,"y")==486,"solid scaling keeps the canonical center");
    fixture.command("save","2");state=fixture.command("load","2");layer=layerObject(state,23);check(number(layer,"w")==192&&number(layer,"h")==108,"scaled solid survives save/load without an image asset");fixture.passed=true;
}
static void nativeBlendFlagNormalization(){
    for(int mode:{2,3,4,5,7,9,12,20,24,25,99})for(int flag:{-1,0,1,2}){
        Program program;program.image();for(int n:{1,0,7,0})program.push(n);program.op(0x252);
        program.push(17);for(int n:{flag,INT32_MIN,mode,0})program.push(n);program.op(0x252);program.op(0x131);program.op(0x251);program.dialogue(0);program.op(0xf4);
        Fixture fixture(std::move(program),true);auto state=fixture.firstComplete();
        check(number(state,"blend")==double(mode==99?0:mode),"native invalid blend maps to zero while valid supported blends survive");
        check(number(state,"blendFlag")==double(flag==1||(flag==-1&&mode==5)),"native mode5 alone maps raw-1 to flag1 while9 and other modes map it to0");
        check(number(state,"z")==17,"blend consumes exactly four including unused reserved operand");
        if(mode==5||mode==9){auto before=visibleLayers(state);fixture.command("save","2");woh_destroy(fixture.handle);fixture.handle=woh_create(fixture.game.string().c_str());state=fixture.command("load","2");check(visibleLayers(state)==before,"WH13 preserves root5/9 and each normalized flag across fresh Runtime restore");}fixture.passed=true;
    }
}
static void rootBlendTwoThreeEffectAndRestoreGates(){
    for(int mode:{2,3})for(bool child:{false,true}){
        Program p;if(child)p.image(1000,"fake",18,{{0,0,1920,1080,0,1}});p.image(1000,"fake",0,{},child?18:-1);for(int n:{0,0,0,915,0})p.push(n);p.op(0x278);p.dialogue(0);p.op(0xf4);
        const std::string table="915 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n915 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n";
        const std::string windows="915 0 0 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n";
        Fixture f(std::move(p),true,table,"","",windows);woh_destroy(f.handle);auto properties=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(properties);{std::ofstream out(properties);for(int index=0;index<2;++index){out<<"915 "<<index<<' '<<index<<" 0x2000000";for(int field=0;field<13;++field)out<<' '<<(field==2?mode:-32767);out<<'\n';}}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));
        if(child)check(state.find("Unsupported effect blend mode")!=std::string::npos,"root2/3 effect support does not pass an unreviewed q1 child");
        else{check(number(layerObject(state,0),"blend")==mode,"native effect property may select supported root2/3");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"WH13 restores root2/3 layers and active effects");
            auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");std::istringstream in(std::string(good.hostExtra.begin(),good.hostExtra.end()));std::ostringstream out;std::string line;std::getline(in,line);out<<line<<'\n';std::getline(in,line);out<<line<<'\n';size_t count=std::stoul(line);for(size_t n=0;n<count;++n){std::getline(in,line);auto fields=hostFields(line);if(fields[0]=="0")fields[13]="26";hostRow(out,fields,fields.size(),6);}while(std::getline(in,line))out<<line<<'\n';auto bytes=out.str();good.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);state=f.command("load","2");check(state.find("Invalid saved layer properties")!=std::string::npos&&visibleLayers(state)==before,"unreviewed root blend26 cannot enter through saved-layer restoration");}
        f.passed=true;
    }
}
static void nativeImageUnlock(){
    Program program;program.image();program.push(17);program.string("not_an_image");program.op(0x228);program.string("IMG0242");program.op(0x228);program.op(0x131);program.op(0x251);program.dialogue(0);program.op(0xf4);
    Fixture fixture(std::move(program),true);auto state=fixture.firstComplete();
    check(number(state,"z")==17,"image unlock consumes exactly one string and pushes nothing");
    check(number(state,"seenImageCount")==1,"image unlock is recorded without requiring a prepared image");
    auto document=mahoyo::save::read(fixture.root/"savedata/seen-images.whp");std::string names(document.hostExtra.begin(),document.hostExtra.end());
    check(names=="WOHSEEN1\n\"img0242.mzp\"\n","native img prefix conversion is retained in the global registry");
    fixture.command("save","2");fixture.command("title");state=fixture.firstComplete();check(number(state,"seenImageCount")==1,"new play keeps and deduplicates global unlocks");
    state=fixture.command("load","2");check(number(state,"seenImageCount")==1,"slot load does not replace the global image registry");
    woh_destroy(fixture.handle);fixture.handle=woh_create(fixture.game.string().c_str());state=fixture.command("title");check(number(state,"seenImageCount")==1,"global unlocks survive process recreation without altering slot schema");fixture.passed=true;
}
static void targetPixelOffsetSaveRestore(){
    Program program;program.image();for(auto n:{1,10,-1,2,-41,-5,0})program.push(n);program.op(0x259);program.dialogue(0);
    for(auto n:{0,0,0,0,8,-9,0})program.push(n);program.op(0x259);program.dialogue(1);program.op(0xf4);
    Fixture fixture(std::move(program),true);auto state=fixture.firstComplete();
    check(std::abs(number(state,"pixelOffsetX")+.5)<1e-8&&std::abs(number(state,"pixelOffsetY")+4.1)<1e-8,"native offset is divided without changing geometry");
    auto x=number(state,"x");state=fixture.command("save","2");check(state.find("\"notice\":\"Saved\"")!=std::string::npos,"save target pixel offsets");
    state=fixture.command("advance");check(number(state,"pixelOffsetX")==-9&&number(state,"pixelOffsetY")==8,"zero divisor becomes one");
    state=fixture.command("load","2");check(std::abs(number(state,"pixelOffsetX")+.5)<1e-8&&std::abs(number(state,"pixelOffsetY")+4.1)<1e-8,"restore native offsets");
    check(number(state,"x")==x,"render target offset never changes the logical pivot or position");fixture.passed=true;
}
static void nativePaletteSaveRestore(){
    for(int mode:{5,6,7,8,12,13,16,17,18,19,21,27,68,70,74,82}){
        Program program;program.image();program.push(0);program.push(mode);program.push(0);program.op(0x253);program.dialogue(0);program.op(0xf4);
        Fixture fixture(std::move(program),true);fixture.firstComplete();fixture.command("save","2");auto state=fixture.command("load","2");
        check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&number(state,"colorMode")==mode,"native palette mode survives save and restore");fixture.passed=true;
    }
    for(int mode:{-1,83}){Program p;p.image();for(int n:{0,mode,0})p.push(n);p.op(0x253);p.dialogue(0);Fixture f(std::move(p),true);std::string state=woh_command(f.handle,"action","start");check(state.find("Unsupported color mode")!=std::string::npos,"out-of-range color modes remain explicit errors");f.passed=true;}
}
static std::string acknowledge(Fixture& f,const std::string& state);
static void nativeColor7CloneCaptureRestore(){
    const std::string table="915 0 50 50 64 1000 1000 -100001 0 0 0 1000\n915 1000 100 50 255 1000 1000 -100001 0 0 0 1000\n";
    for(int color:{6,7}){
    Program p;p.image();for(int n:{0x123456,color,0})p.push(n);p.op(0x253);p.alpha(0,128);p.push(915);p.push(0);p.op(0x279);for(int n:{0,0,0,915,0})p.push(n);p.op(0x278);p.commit();p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);p.op(0xf4);
    const std::string windows="915 0 0 0x0 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x0 -32767 -32767 -32767 -32767 -32767 -32767\n";
    Fixture f(std::move(p),true,table,"fake 100 100\n","",windows);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));if(waiting(state,"frame"))state=f.command("tick","1000");auto original=visibleLayers(state);
    check(number(layerObject(state,0),"colorMode")==color&&number(layerObject(state,0),"rgb")==0x123456,"mode6/7 capture geometry without discarding native color configuration or raw RGB argument");
    f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(saved.hostExtra.begin(),saved.hostExtra.end());check(extra.find("EFFECTFRAMES1 1")!=std::string::npos,"color6/7 fixture exercises a real captured-frame override");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==original,"fresh Runtime restores mode6/7 alongside active effect/captured frame state");state=f.command("advance");check(number(layerObject(state,1),"colorMode")==color&&number(layerObject(state,1),"rgb")==0x123456,"native terminal clone retains color6/7 without changing its color parameter");f.passed=true;
    }
    for(int mode:{6,7,83}){
        Program effect;effect.image();for(int n:{0,0,0,915,0})effect.push(n);effect.op(0x278);effect.dialogue(0);effect.op(0xf4);
        const std::string propertiesWindows="915 0 0 0x200 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x200 -32767 -32767 -32767 -32767 -32767 -32767\n";
        Fixture candidate(std::move(effect),true,table,"fake 100 100\n","",propertiesWindows);woh_destroy(candidate.handle);auto path=candidate.game/"meta/effects-properties.tsv";candidate.extraFiles.push_back(path);{std::ofstream out(path);for(int i=0;i<2;++i){out<<"915 "<<i<<' '<<i<<" 0x200";for(int n=0;n<13;++n)out<<' '<<(n==3?mode:-32767);out<<'\n';}}candidate.handle=woh_create(candidate.game.string().c_str());candidate.command("textSpeed","0");auto result=std::string(woh_command(candidate.handle,"action","start"));
        if(mode==83)check(result.find("Unsupported effect color mode")!=std::string::npos,"mode6/7 support does not admit out-of-range mode83 through effect properties");
        else{check(number(layerObject(result,0),"colorMode")==mode,"native effect color property may select independently proved mode6/7");auto before=visibleLayers(result);candidate.command("save","2");woh_destroy(candidate.handle);candidate.handle=woh_create(candidate.game.string().c_str());result=candidate.command("load","2");check(visibleLayers(result)==before,"active color6/7 property state survives a fresh Runtime restore");}candidate.passed=true;
    }
}
static void nativeOriginCoordinates(){
    for(int scale:{1000,2000}){Program program;program.image(scale);program.origin(0,16,69);program.dialogue(0);program.op(0xf4);
        Fixture fixture(std::move(program),true);auto state=fixture.firstComplete();
        check(std::abs(number(state,"pivotX")-.63)<1e-8&&std::abs(number(state,"pivotY")-2.76)<1e-8,"250 preserves the native positive resource origin and integer coordinate grid");
        const double expectedX=scale==1000?150:87,expectedY=scale==1000?150:-126;
        check(std::abs(number(state,"x")-expectedX)<1e-8&&std::abs(number(state,"y")-expectedY)<1e-8,"250 leaves independent O unchanged; unit scale changes only rotation center");fixture.passed=true;
    }
}
static void delayedSoundAndPan(){
    Program program;for(auto n:{1300,800,13,-1,70,50,0})program.push(n);program.string("fake");program.push(18);program.op(0x197);program.dialogue(0);program.op(0xf4);
    Fixture fixture(std::move(program));const auto first=fixture.command("action","start");
    auto state=fixture.command("save","2");check(!fs::exists(fixture.root/"savedata/slot2.whs"),"pending sound cannot be lost by saving before its start");
    fixture.command("tick","1000");state=fixture.command("tick","2268");check(state.find("\"audio\":[]")!=std::string::npos,"sound remains silent until1300ms");
    state=fixture.command("tick","2284");check(number(state,"volume")==.5&&number(state,"pan")==13,"delayed sound starts once with native negative-pan replacement");auto id=number(state,"id");
    state=fixture.command("tick","2684");check(std::abs(number(state,"volume")-.6)<1e-8&&number(state,"id")==id,"800ms volume envelope uses same playback id");
    state=fixture.command("tick","3084");check(number(state,"volume")==.7&&number(state,"pan")==13&&number(state,"nextTickMs")==0,"completed sound envelope returns to idle rendering");
    check(number(state,"instructions")==number(first,"instructions"),"sound scheduling does not consume dialogue input");fixture.passed=true;
    Program pan;for(auto n:{0,1000,128,0,-1,-1,0})pan.push(n);pan.string("fake");pan.push(18);pan.op(0x197);pan.dialogue(0);pan.op(0xf4);
    Fixture stereo(std::move(pan));state=stereo.command("action","start");check(number(state,"pan")==0&&number(state,"volume")==1,"negative volume endpoints default to100percent");
    stereo.command("tick","1000");state=stereo.command("tick","1484");check(number(state,"pan")==64,"pan interpolates through center independently of gain");
    state=stereo.command("tick","1984");check(number(state,"pan")==128,"pan reaches final native endpoint");stereo.passed=true;
}
static void delayedExistingSoundVolume(){
    Program p;p.sound(19,20,100,0,128,8000);p.volumeFade(19,45,3000,4000,INT32_MAX,INT32_MIN);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start"),item=audioChannel(state,"se19");auto id=number(item,"id");
    check(number(item,"volume")==.2&&!boolean(state,"canSave")&&number(state,"nextTickMs")==16,"196 delays existing-volume fade and keeps ticks/save guard active");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"pending volume fade cannot be dropped through a slot save");
    f.command("tick","1000");state=f.command("tick","4983");item=audioChannel(state,"se19");check(number(item,"volume")>.599&&number(item,"volume")<.6,"previous volume animation continues during3999ms delay");
    state=f.command("tick","4984");item=audioChannel(state,"se19");check(number(item,"volume")==.6&&number(item,"pan")==64&&number(item,"id")==id,"due tick captures current60percent rather than the request-time20percent");
    state=f.command("tick","6484");item=audioChannel(state,"se19");check(number(item,"volume")==.52&&number(item,"pan")==88,"native integer volume interpolation coexists with previous pan timeline");
    state=f.command("tick","7984");item=audioChannel(state,"se19");check(number(item,"volume")==.45&&number(item,"pan")==112,"196 reaches45percent in3000ms without canceling pan");
    state=f.command("tick","8984");item=audioChannel(state,"se19");check(number(item,"volume")==.45&&number(item,"pan")==128&&number(item,"id")==id&&item.find("fake.hw")!=std::string::npos,"original playback identity/name survive both envelopes");
    check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"completed sound/volume envelopes return to safe idle");f.passed=true;
}
static void volumeDelayPauseAndMute(){
    Program p;p.sound(19,60,60,13,13,0);p.volumeFade(19,0,3000,4000);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");auto id=number(audioChannel(state,"se19"),"id");
    f.command("tick","1000");f.command("tick","2984");f.command("pause");state=f.command("tick","900000");check(number(audioChannel(state,"se19"),"volume")==.6&&number(state,"nextTickMs")==0,"pause freezes delayed fade clock");
    f.command("resume");f.command("tick","910000");state=f.command("tick","911983");check(number(audioChannel(state,"se19"),"volume")==.6,"inactive wall time cannot consume the remaining delay");
    state=f.command("tick","911984");check(number(audioChannel(state,"se19"),"volume")==.6,"fade starts after exactly4000ms of active time");
    state=f.command("tick","913484");check(number(audioChannel(state,"se19"),"volume")==.3,"mute transition uses the full3000ms after delayed start");
    state=f.command("tick","914984");auto item=audioChannel(state,"se19");check(number(item,"volume")==0&&number(item,"pan")==13&&number(item,"id")==id&&item.find("fake.hw")!=std::string::npos,"target0 mutes without stopping/restarting or moving pan");
    check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"finished delayed fade no longer blocks saving/idle");f.command("save","2");check(fs::exists(f.root/"savedata/slot2.whs"),"slot save succeeds after delayed envelope completes");f.passed=true;
}
static void volumeReservationReplacementAndRestart(){
    Program p;p.sound(19,100,100,64,64,0);p.volumeFade(19,80,1000,1000);p.volumeFade(19,20,1000,3000);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p));f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");auto state=f.command("tick","2484");
    check(number(audioChannel(state,"se19"),"volume")==1,"new channel reservation replaces the older pending fade only");
    f.command("tick","3984");state=f.command("tick","4484");check(number(audioChannel(state,"se19"),"volume")==.6,"replacement begins at its own delay and interpolates from current gain");
    state=f.command("tick","4984");check(number(audioChannel(state,"se19"),"volume")==.2&&boolean(state,"canSave"),"replaced reservation cannot replay later");f.passed=true;
    Program replacement;replacement.sound(19,50,50,13,13,0);replacement.volumeFade(19,0,1000,4000);replacement.dialogue(0);replacement.sound(19,80,80,32,32,0,500,"replacement");replacement.dialogue(1);replacement.op(0xf4);
    Fixture newer(std::move(replacement));newer.command("textSpeed","0");state=newer.command("action","start");auto old=number(audioChannel(state,"se19"),"id");newer.command("advance");newer.command("tick","1000");state=newer.command("tick","1484");auto item=audioChannel(state,"se19");
    check(number(item,"id")!=old&&number(item,"volume")==.8&&number(item,"pan")==32&&item.find("replacement.hw")!=std::string::npos,"delayed new sound cancels the old playback's volume reservation");
    check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"delayed SOUND and delayed FADE queues remain independent and clean after replacement");newer.passed=true;
}
static void volumeFadeLoopAndOperandContract(){
    Program p;p.image();p.sound190(19,1,"fake.hw",80,64);p.dialogue(0);p.push(17);p.volumeFade(64,99,3000,4000);p.volumeFade(19,-5,0,-1,999,-999);p.op(0x131);p.op(0x251);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,"","","fake.hw 48000 48000 2 0 2400 24000\n");f.command("textSpeed","0");f.command("action","start");f.command("save","2");auto state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos,"normal saved loop restored");auto id=number(audioChannel(state,"se19"),"id");
    state=f.command("advance");check(number(state,"z")==17,"196 consumes exactly6 words and does not reinterpret reserved fields or push a result");
    state=f.command("tick","1000");auto item=audioChannel(state,"se19");check(number(item,"volume")==0&&boolean(item,"loop")&&number(item,"id")==id,"invalid slot is inert; min1ms duration/negative delay/volume clamp preserve existing loop identity");
    check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"inactive invalid-slot request does not leave a phantom timer");f.passed=true;
}
static void backendOneSoundLoop(){
    Program p;p.sound(11,10,60,0,26,4000,0,"SE01024",1);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),false,"","","se01024.hw\t48001\t48000\t2\t0\t0\t0\n");f.command("textSpeed","0");auto state=f.command("action","start"),item=audioChannel(state,"se11");auto id=number(item,"id");
    check(boolean(item,"loop")&&number(item,"volume")==.1&&number(item,"pan")==0,"197 backend1 starts native logical whole-file loop at its requested endpoints");
    check(number(item,"startFrame")==0&&number(item,"endFrame")==48001,"whole-file ambience uses exact PCM loop bounds rather than MediaPlayer EOF seeks");
    f.command("tick","1000");state=f.command("tick","2984");item=audioChannel(state,"se11");check(number(item,"volume")==.35&&number(item,"pan")==13&&number(item,"id")==id,"backend1 shares native simultaneous volume/pan envelope without a restart");
    state=f.command("tick","4984");item=audioChannel(state,"se11");check(number(item,"volume")==.6&&number(item,"pan")==26&&boolean(item,"loop")&&number(item,"id")==id,"backend1 reaches both endpoints and continues looping");
    check(number(state,"nextTickMs")==0&&boolean(state,"canSave"),"an ongoing media loop does not busy-poll the runtime after envelope completion");
    f.command("save","2");state=f.command("load","2");check(boolean(audioChannel(state,"se11"),"loop")&&number(audioChannel(state,"se11"),"volume")==.6,"logical loop flag survives the existing portable audio save");f.passed=true;
    Program missing;missing.sound(11,10,60,0,26,4000,0,"SE01024",1);missing.dialogue(0);
    Fixture noMap(missing);state=woh_command(noMap.handle,"action","start");check(state.find("Missing prepared native audio loop metadata: se01024.hw")!=std::string::npos&&state.find("\"audio\":[]")!=std::string::npos,"backend1 cannot assume unknown header bounds");noMap.passed=true;
    Fixture partial(missing,false,"","","se01024.hw 48001 48000 2 1 1200 24000\n");state=partial.command("action","start");check(number(audioChannel(state,"se11"),"startFrame")==1200&&number(audioChannel(state,"se11"),"endFrame")==24000,"197 forwards partial HW bounds to the exact PCM loop backend");partial.passed=true;
    Program raw;raw.sound(11,100,100,64,64,0,0,"SE01024",0);raw.dialogue(0);Fixture once(std::move(raw),false,"","","se01024.hw 48001 48000 2 1 1200 24000\n");state=once.command("action","start");check(!boolean(audioChannel(state,"se11"),"loop"),"raw backend0 remains one-shot regardless of loop metadata");once.passed=true;
}
static void backendOneSound190(){
    Program p;p.sound190(9,1,"SE01042",60,26,3000,500);p.dialogue(0);p.sound190(12,1,"SE01043",100,13);p.dialogue(1);p.sound190(9,0,"",0,0);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),false,"","","se01042.hw 65485 48000 2 0 0 0\nse01043.hw 56045 48000 2 0 0 0\n");f.command("textSpeed","0");auto state=f.command("action","start");check(state.find("\"audio\":[]")!=std::string::npos&&!boolean(state,"canSave"),"190 delay stays independent of its backend1 fade and blocks unsafe saves");
    f.command("tick","1000");state=f.command("tick","1483");check(state.find("\"audio\":[]")!=std::string::npos,"190 does not start before its requested500ms delay");state=f.command("tick","1484");auto item=audioChannel(state,"se9");auto id=number(item,"id");
    check(boolean(item,"loop")&&number(item,"volume")==0&&number(item,"pan")==26,"190 backend1 starts a logical loop with preserved pan and zero fade-in endpoint");
    state=f.command("tick","2984");item=audioChannel(state,"se9");check(number(item,"volume")==.3&&number(item,"id")==id&&number(item,"pan")==26,"190 keeps its full3000ms fade after the start delay without playback restart");
    state=f.command("tick","4484");item=audioChannel(state,"se9");check(number(item,"volume")==.6&&boolean(item,"loop")&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"190 looping playback becomes idle when its envelope ends");
    state=f.command("advance");auto second=audioChannel(state,"se12");auto secondId=number(second,"id");check(boolean(second,"loop")&&number(second,"volume")==1&&number(second,"pan")==13&&number(audioChannel(state,"se9"),"id")==id,"190 immediate backend1 leaves other channels intact");
    f.command("advance");f.command("tick","10000");state=f.command("tick","10034");item=audioChannel(state,"se9");check(item.find("\"path\":\"\"")!=std::string::npos&&number(audioChannel(state,"se12"),"id")==secondId&&boolean(state,"canSave"),"190NULL name retains native50ms stop without stopping another loop");f.passed=true;
    Program raw;raw.image();raw.push(17);raw.sound190(3,0,"fake",45,128);raw.op(0x131);raw.op(0x251);raw.dialogue(0);raw.op(0xf4);Fixture once(std::move(raw),true);state=once.firstComplete();item=audioChannel(state,"se3");check(number(state,"z")==17&&!boolean(item,"loop")&&number(item,"volume")==.45&&number(item,"pan")==128,"190 consumes exactly7/pushesnone and preserves raw one-shot volume/pan");once.passed=true;
    for(int backend:{1,2}){Program invalid;invalid.sound190(9,backend,"SE01042",60,64);invalid.dialogue(0);Fixture rejected(std::move(invalid));state=woh_command(rejected.handle,"action","start");check(state.find(backend==1?"Missing prepared native audio loop metadata":"Unsupported sound effect backend")!=std::string::npos&&state.find("\"audio\":[]")!=std::string::npos,"190 unknown backend/header cannot silently become a one-shot");rejected.passed=true;}
    Program invalid;invalid.sound190(9,1,"SE01042",60,64);invalid.dialogue(0);Fixture partial(std::move(invalid),false,"","","se01042.hw 65485 48000 2 1 1200 24000\n");state=partial.command("action","start");check(number(audioChannel(state,"se9"),"startFrame")==1200&&number(audioChannel(state,"se9"),"endFrame")==24000,"190 and197 share native source-frame loop metadata");partial.passed=true;
}
static void inactiveAndSupersededVolumeCleanup(){
    Program inactive;inactive.volumeFade(19,20,3000,4000);inactive.dialogue(0);inactive.op(0xf4);
    Fixture none(std::move(inactive));none.command("textSpeed","0");auto state=none.command("action","start");check(boolean(state,"canSave")&&number(state,"nextTickMs")==0&&state.find("\"audio\":[]")!=std::string::npos,"inactive196 creates no phantom delay/save guard");none.passed=true;
    Program p;p.sound(19,60,60,13,13,0);p.volumeFade(19,10,10000,0);p.dialogue(0);p.volumeFade(19,50,100,0);p.dialogue(1);p.op(0xf4);
    Fixture replaced(std::move(p));replaced.command("textSpeed","0");replaced.command("action","start");replaced.command("advance");replaced.command("tick","1000");state=replaced.command("tick","1084");
    check(number(audioChannel(state,"se19"),"volume")==.5&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"superseded volume-only envelope leaves no inert10000ms timer");replaced.passed=true;
}
static std::string startGlobalPulse(Fixture& f){
    f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"global request is queued behind a scene-ready barrier");
    auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));f.command("transitionReady",id.c_str());return f.command("tick","1000");
}
static void globalShakeReadyAndGeometry(){
    Program p;p.image();p.image(1000,"WHITE",1);p.globalShake();p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(!boolean(state,"canSave")&&number(state,"nextTickMs")==0,"pending global shake blocks saves while asset-ready ticks remain disabled");
    state=f.command("tick","100000");check(number(layerObject(state,0),"x")==150&&waiting(state,"transitionReady"),"loading time cannot advance queued340");
    auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));f.command("transitionReady",id.c_str());f.command("tick","100000");state=f.command("tick","100034");
    check(std::abs(number(layerObject(state,0),"x")-174)<1e-6&&std::abs(number(layerObject(state,0),"y")-156)<1e-6,"50ms native internal8,2 becomes logical24,6 on image output only");
    check(number(layerObject(state,1),"x")==0&&number(layerObject(state,1),"y")==0,"native static WHITE metadata excludes global offset");
    auto text=state.substr(state.find("\"text\":"));check(number(text,"x")==300&&number(text,"y")==120,"NVL/native-density text is not translated with global image shake");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"active global pulse cannot leak its transient transform into a save");
    state=f.command("tick","100184");check(number(layerObject(state,0),"x")==150&&number(layerObject(state,0),"y")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"200ms pulse returns to exact original geometry and idle");
    f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");check(saved.layers[0].x==150&&saved.layers[0].y==150,"stored layer geometry remains unshaken");f.passed=true;
}
static void globalShakeClearDelayAndPause(){
    Program clear;clear.image();clear.globalShake(8,2,1000);clear.commit();clear.dialogue(0);clear.push(999);clear.op(0x341);clear.dialogue(1);clear.commit();clear.dialogue(0);clear.op(0xf4);
    Fixture f(std::move(clear),true);startGlobalPulse(f);auto state=f.command("tick","1034");double moving=number(layerObject(state,0),"x");check(moving>150,"initial pulse is active");
    state=f.command("advance");check(number(layerObject(state,0),"x")==moving&&!boolean(state,"canSave"),"341 queues clear without immediately removing the current offset");
    state=f.command("advance");f.command("tick","900000");auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));state=f.command("transitionReady",id.c_str());check(number(layerObject(state,0),"x")==150,"scene-ready commit applies queued341 cancellation");
    state=f.command("tick","900000");check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"committed clear leaves no active/pending timer");f.passed=true;
    Program delayed;delayed.image();delayed.push(123);delayed.op(0x341);delayed.globalShake(8,2,200,1,500);delayed.commit();delayed.dialogue(0);delayed.op(0xf4);
    Fixture later(std::move(delayed),true);startGlobalPulse(later);state=later.command("tick","1483");check(number(layerObject(state,0),"x")==150&&!boolean(state,"canSave"),"new340 after341 remains pending through its own499ms delay");
    state=later.command("tick","1534");check(std::abs(number(layerObject(state,0),"x")-174)<1e-6,"queued replacement starts after clear and delay");
    later.command("pause");state=later.command("tick","900000");check(std::abs(number(layerObject(state,0),"x")-174)<1e-6&&number(state,"nextTickMs")==0,"pause freezes the global transform");
    later.command("resume");state=later.command("tick","900000");check(!boolean(state,"canSave"),"resume uses an active16ms bootstrap rather than paused wall time");
    state=later.command("tick","900134");check(number(layerObject(state,0),"x")==150&&boolean(state,"canSave"),"delayed pulse completes after remaining active time");later.passed=true;
    Program canceled;canceled.image();canceled.push(77);canceled.globalShake();canceled.push(999);canceled.op(0x341);canceled.op(0x131);canceled.op(0x251);canceled.commit();canceled.dialogue(0);canceled.op(0xf4);
    Fixture before(std::move(canceled),true);state=startGlobalPulse(before);check(number(state,"z")==77&&number(layerObject(state,0),"x")==150&&boolean(state,"canSave"),"340 consumes5 and341 consumes1; a later clear cancels the earlier queued pulse at commit");before.passed=true;
}
static void globalShakeLoadAndTitleReset(){
    Program p;p.image();p.dialogue(0);p.globalShake();p.commit();p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");f.command("save","2");uint64_t clock=1000;commitNext(f,clock);auto state=f.command("tick","1050");check(number(layerObject(state,0),"x")!=150&&!boolean(state,"canSave"),"load-reset fixture has an active global transform");
    state=f.command("load","2");check(number(layerObject(state,0),"x")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"valid load cancels active/queued global effects and restores canonical coordinates");
    commitNext(f,clock);state=f.command("title");check(number(state,"nextTickMs")==0&&number(layerObject(state,0),"x")==0,"title clears transient global offsets before rendering native UI");
    state=f.command("action","start");check(number(layerObject(state,0),"x")==150&&boolean(state,"canSave"),"new play cannot inherit an old global pulse");f.passed=true;
}
static void layerShakeReadyGeometryAndCompletion(){
    Program p;p.image(1000,"fake",23);p.image(1000,"fake",24);p.push(77);p.layerShake(23);p.push(23);p.op(0x251);p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&!boolean(state,"canSave")&&number(state,"nextTickMs")==0,"mode2 pending waits for ready without consuming loading time");
    state=f.pulseTick("100000");check(number(layerObject(state,23),"y")==150,"uncommitted selected pulse cannot advance during loading");
    auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));f.command("transitionReady",id.c_str());f.pulseTick("100000");state=f.pulseTick("100059");
    check(number(layerObject(state,23),"z")==77,"270 consumes exactly7 words and pushes no result");
    check(std::abs(number(layerObject(state,23),"y")-167.5)<1e-5&&number(layerObject(state,23),"x")==150,"source605 mode2 uses native geometry-boosted finite75ms local dy");
    check(number(layerObject(state,24),"y")==150,"selected mode2 does not shake another layer");
    auto text=state.substr(state.find("\"text\":"));check(number(text,"x")==300&&number(text,"y")==120,"NVL text is independent of selected image offsets");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"active local mode2 cannot be saved without its transient clock");
    state=f.pulseTick("100284");check(number(layerObject(state,23),"y")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"finite mode2 returns to canonical rect and idle at300ms");
    f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");check(document.layers[0].x==150&&document.layers[0].y==150&&currentHostSchema(document),"local shake leaves stored geometry and current schema unchanged");f.passed=true;
}
static void layerShakeProjectedQuadInputs(){
    auto run=[&](Program p,double expectedX,double expectedY,const char* reason){p.layerShake(23,4,5);p.commit();p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);startGlobalPulse(f);auto state=f.pulseTick("1059");check(std::abs(number(layerObject(state,23),"x")-expectedX)<1e-4&&std::abs(number(layerObject(state,23),"y")-expectedY)<1e-4,reason);f.passed=true;};
    Program rotated;rotated.image(1000,"fake",23);for(int n:{0,0,0,1000,2000,INT32_MIN,INT32_MIN,23})rotated.push(n);rotated.op(0x246);rotated.position(23,40,30);rotated.rotate(23,9000);
    Program pivoted=rotated;for(int n:{INT32_MIN,0,0,23})pivoted.push(n);pivoted.op(0x250);
    run(rotated,71.25,78.75,"mode2 minimum comes from the rotated nonuniform200x100 quad rather than its unrotated rect");
    run(pivoted,115.625,78.75,"positive P0 leaves O intact and changes the rotated minimum used by independent shake boosts");
    Program threshold;threshold.image(1000,"fake",23);threshold.position(23,35,0);threshold.layerShake(23,4,0);threshold.commit();threshold.dialogue(0);threshold.op(0xf4);
    Fixture edge(threshold,true);startGlobalPulse(edge);auto state=edge.pulseTick("1059");check(std::abs(number(layerObject(state,23),"x")-95.625)<1e-5,"exact projectedX90 is not doubled");state=edge.pulseTick("1209");check(std::abs(number(layerObject(state,23),"x")-88.125)<1e-5,"the previous local output is excluded from the next geometry test");edge.passed=true;
    Program pixel;pixel.image(1000,"fake",23);pixel.position(23,35,0);pixel.targetOffset(23,1,0,100);pixel.layerShake(23,4,0);pixel.commit();pixel.dialogue(0);pixel.op(0xf4);
    Fixture shifted(std::move(pixel),true);startGlobalPulse(shifted);state=shifted.pulseTick("1059");check(std::abs(number(layerObject(state,23),"x")-101.25)<1e-5&&number(layerObject(state,23),"pixelOffsetX")==.01,"target-pixel offset affects projected minimum without moving canonical geometry");shifted.passed=true;
    Program changed;changed.image(1000,"fake",23);changed.position(23,35,0);changed.layerShake(23,4,0);changed.commit();changed.dialogue(0);changed.position(23,40,0);changed.dialogue(1);changed.op(0xf4);
    Fixture immediate(std::move(changed),true);startGlobalPulse(immediate);immediate.pulseTick("1059");state=immediate.command("advance");check(std::abs(number(layerObject(state,23),"x")-121.25)<1e-5,"same-tick script geometry changes refresh the projected branch before its first output frame");immediate.passed=true;
    Program moving;moving.image(1000,"fake",23);moving.position(23,35,0);moving.position(23,40,0,300);moving.layerShake(23,4,0);moving.commit();moving.dialogue(0);moving.op(0xf4);
    Fixture dynamic(std::move(moving),true);startGlobalPulse(dynamic);state=dynamic.pulseTick("1059");check(std::abs(number(layerObject(state,23),"x")-106.25)<1e-5,"animated current image position refreshes the geometry branch every frame");dynamic.passed=true;
    Program global;global.image(1000,"fake",23);global.image(1000,"fake",24);global.position(23,34,0);global.position(24,34,0);global.globalShake(8,0,300);global.layerShake(23,4,0);global.commit();global.dialogue(0);global.op(0xf4);
    Fixture inherited(std::move(global),true);startGlobalPulse(inherited);state=inherited.pulseTick("1059");check(std::abs(number(layerObject(state,23),"x")-number(layerObject(state,24),"x")-11.25)<1e-5,"global offset is included in mode2's unshaken projected minimum");inherited.passed=true;
    Program solid;solid.image(1000,"WHITE",23);solid.layerShake(23);solid.commit();solid.dialogue(0);solid.op(0xf4);
    Fixture white(std::move(solid));startGlobalPulse(white);state=white.pulseTick("1059");check(std::abs(number(layerObject(state,23),"y")-8.75)<1e-5,"global static-image opt-out does not suppress an explicit local mode2 request");white.passed=true;
}
static void layerShakeReplacementDelayAndCancel(){
    Program p;p.image(1000,"fake",23);p.layerShake(23,0,5,300,-1);p.commit();p.dialogue(0);p.layerShake(23,4,0,300,300,200);p.commit();p.dialogue(0);p.push(23);p.op(0x272);p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);startGlobalPulse(f);auto state=f.pulseTick("1059");double oldY=number(layerObject(state,23),"y");check(oldY>170&&!boolean(state,"canSave"),"negative duration is an active indefinite local pulse");
    state=f.command("advance");check(number(layerObject(state,23),"y")==oldY,"queueing a replacement alone preserves the current local job");
    state=f.command("advance");f.pulseTick("100000");auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(tail,"id")));state=f.command("transitionReady",id.c_str());check(number(layerObject(state,23),"y")==150,"commit cancels the old local job before the new delay begins");
    f.pulseTick("100000");state=f.pulseTick("100183");check(number(layerObject(state,23),"x")==150&&!boolean(state,"canSave"),"replacement waits its full200ms after scene readiness");
    f.pulseTick("100184");state=f.pulseTick("100259");check(std::abs(number(layerObject(state,23),"x")-161.25)<1e-5&&number(layerObject(state,23),"y")==150,"new local pulse starts at its own delayed timeline");
    state=f.command("advance");check(std::abs(number(layerObject(state,23),"x")-161.25)<1e-5,"272 selected cancel is pending until scene commit");uint64_t clock=100259;state=commitNext(f,clock);check(number(layerObject(state,23),"x")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"selected cancel commits zero local offset and releases guards");f.passed=true;
    Program all;all.image(1000,"fake",23);all.image(1000,"fake",24);all.layerShake(23,0,5,300,-1);all.layerShake(24,4,0,300,-1);all.globalShake(8,2,1000,10);all.commit();all.dialogue(0);all.push(23);all.op(0x272);all.commit();all.dialogue(1);all.push(-1);all.op(0x272);all.commit();all.dialogue(0);all.op(0xf4);
    Fixture group(std::move(all),true);startGlobalPulse(group);group.pulseTick("1059");clock=1059;state=commitNext(group,clock);check(number(layerObject(state,24),"x")!=150&&!boolean(state,"canSave"),"selected272 leaves other local/global jobs active");state=commitNext(group,clock);check(number(layerObject(state,23),"x")==number(layerObject(state,24),"x")&&!boolean(state,"canSave"),"negative272 cancels local270 jobs without cancelling the separate340 global pulse");group.passed=true;
    Program zero;zero.image(1000,"fake",23);zero.layerShake(23,0,5,300,-1);zero.commit();zero.dialogue(0);zero.layerShake(23,0,0,0,0,-7);zero.commit();zero.dialogue(1);zero.op(0xf4);
    Fixture unchanged(std::move(zero),true);startGlobalPulse(unchanged);unchanged.pulseTick("1059");clock=1059;state=commitNext(unchanged,clock);check(number(layerObject(state,23),"y")!=150&&!boolean(state,"canSave"),"both-zero non-clear factory preserves the old pulse before validating unused timing");unchanged.passed=true;
}
static void layerShakePauseResetDeleteAndModes(){
    Program p;p.image(1000,"fake",23);p.dialogue(0);p.layerShake(23,0,5,300,-1);p.commit();p.dialogue(1);p.push(23);p.op(0x241);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");f.command("action","start");f.command("save","2");uint64_t clock=1000;commitNext(f,clock);auto state=f.pulseTick("1075");double y=number(layerObject(state,23),"y");f.command("pause");state=f.pulseTick("900000");check(number(layerObject(state,23),"y")==y&&number(state,"nextTickMs")==0,"pause freezes local pulse and virtual time");f.command("resume");state=f.pulseTick("900000");check(!boolean(state,"canSave"),"local pulse resumes with16ms bootstrap");
    state=f.command("load","2");check(number(layerObject(state,23),"y")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"load clears active and pending local shake without serializing transient coordinates");commitNext(f,clock);state=f.command("advance");check(visibleLayers(state)=="[]"&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"deleting the target layer cleans its active/pending pulse");f.command("title");state=f.command("action","start");check(number(layerObject(state,23),"y")==150&&boolean(state,"canSave"),"title/newplay cannot inherit local shake jobs");f.passed=true;
    Program queued;queued.layerShake(23);queued.image(1000,"fake",23);queued.commit();queued.dialogue(0);queued.op(0xf4);Fixture created(std::move(queued),true);startGlobalPulse(created);state=created.pulseTick("1059");check(std::abs(number(layerObject(state,23),"y")-167.5)<1e-5,"new image creation retains the same commit's pending local request");created.passed=true;
    Program removed;removed.image(1000,"fake",23);removed.layerShake(23);removed.push(23);removed.op(0x241);removed.commit();removed.dialogue(0);removed.op(0xf4);Fixture gone(std::move(removed),true);state=startGlobalPulse(gone);check(visibleLayers(state)=="[]"&&boolean(state,"canSave"),"deletion also removes an uncommitted local request");gone.passed=true;
    for(auto mode:{7,8}){Program invalid;invalid.image(1000,"fake",23);invalid.layerShake(23,0,5,300,300,0,mode);invalid.dialogue(0);Fixture unsupported(std::move(invalid),true);state=woh_command(unsupported.handle,"action","start");check(state.find("Unsupported advanced shake variant")!=std::string::npos,"unreviewed selected modes cannot fall back to mode2 math");unsupported.passed=true;}
    for(int mode:{0,1}){Program random;random.image();random.image(1000,"fake",23);random.layerShake(-1,2,3,100,200,0,mode);random.commit();random.dialogue(0);random.op(0xf4);Fixture old(std::move(random),true);state=startGlobalPulse(old);check(std::abs(number(layerObject(state,0),"x")-152.58)<1e-5&&std::abs(number(layerObject(state,23),"x")-(mode?152.58:153.42))<1e-5,"script0 uses independent pairs while script1 followers use primary cache and their own geometry");state=old.pulseTick("1184");check(number(layerObject(state,0),"x")==150&&boolean(state,"canSave"),"both native random modes complete at their capped active duration");old.passed=true;}
}
static void layerRandomReadyHeldAndFinite(){
    Program p;p.image(1000,"fake",13);p.image(1000,"fake",14);p.push(77);p.layerShake(13,0,18,180,1500,0,0);p.push(13);p.op(0x251);p.commit();p.dialogue(0);p.op(0xf4);Fixture f(p,true),reference(p,true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0&&!boolean(state,"canSave"),"selected mode0 waits for graphics readiness without a running random clock");state=f.pulseTick("900000");check(number(layerObject(state,13),"y")==150,"loading cannot consume the first random event");f.command("transitionReady","1");auto initial=layerObject(f.pulseTick("100000"),13);auto other=startGlobalPulse(reference);check(std::abs(number(initial,"y")-203.520000458)<1e-4&&number(initial,"x")==150&&number(initial,"z")==77,"actual13 mode0 consumes7/noPush and applies seed1 native boosted internalY8.92 once");
    for(int i=0;i<20;++i){state=f.command("autoDelay","2400");check(sameLayerXY(state,initial,13),"same-tick host updates do not redraw randomness");state=woh_snapshot(f.handle);check(sameLayerXY(state,initial,13),"snapshot observations cannot advance RNG");}
    state=f.pulseTick("100163");check(number(layerObject(state,13),"y")==number(initial,"y"),"mode0 holds its exact local offset through179ms without envelope drift");state=f.pulseTick("100164");other=reference.pulseTick("1164");check(std::abs(number(layerObject(state,13),"y")-88.540798187)<1e-4&&sameLayerXY(state,other,13),"180ms draws the next two sharedCRT values only, unaffected by snapshots or skipped loading time");
    check(number(layerObject(state,14),"x")==150&&number(layerObject(state,14),"y")==150,"selected random pulse does not affect another image");auto text=state.substr(state.find("\"text\":"));check(number(text,"x")==300&&number(text,"y")==120,"random layer pulse leaves native-density NVL coordinates fixed");f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"random clock/RNG job blocks incomplete transient saves");state=f.pulseTick("100344");check(std::abs(number(layerObject(state,13),"y")-185.294397354)<1e-4,"360ms finite envelope is sampled only when a new random event is due");state=f.pulseTick("101484");check(number(layerObject(state,13),"y")==150&&number(layerObject(state,13),"x")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"1500ms finite endpoint clears the held offset and restores idle");f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");check(document.layers[0].x==150&&document.layers[0].y==150,"completed mode0 stores canonical unshaken coordinates");f.passed=reference.passed=true;
}
static void layerRandomDelayPauseAndIndefiniteCancel(){
    Program delayed;delayed.image(1000,"fake",13);delayed.layerShake(13,0,18,180,1500,200,0);delayed.commit();delayed.dialogue(0);delayed.op(0xf4);Fixture later(std::move(delayed),true);startGlobalPulse(later);auto state=later.pulseTick("1183");check(number(layerObject(state,13),"y")==150&&!boolean(state,"canSave"),"mode0 initial199ms delay consumes neither offset nor RNG");state=later.pulseTick("1184");check(std::abs(number(layerObject(state,13),"y")-203.520000458)<1e-4,"initial delay starts elapsed0 with the untouched first sharedCRT event");later.passed=true;
    Program paused;paused.image(1000,"fake",13);paused.layerShake(13,0,18,180,1500,0,0);paused.commit();paused.dialogue(0);paused.op(0xf4);Fixture f(paused,true),reference(paused,true);startGlobalPulse(f);startGlobalPulse(reference);state=f.pulseTick("1084");double y=number(layerObject(state,13),"y");f.command("pause");state=f.pulseTick("9000000");check(number(layerObject(state,13),"y")==y&&number(state,"nextTickMs")==0,"pause freezes held random state and removes ticking");f.command("resume");state=f.pulseTick("9000000");check(number(layerObject(state,13),"y")==y,"resume16ms bootstrap stays in the same held random period");state=f.pulseTick("9000064");auto control=reference.pulseTick("1164");check(sameLayerXY(state,control,13),"paused wall time cannot consume extra RNG or finite envelope duration");state=f.pulseTick("9001384");check(number(layerObject(state,13),"y")==150&&number(state,"nextTickMs")==0,"finite mode0 completes after remaining active time only");f.passed=reference.passed=true;
    Program forever;forever.image(1000,"fake",26);forever.layerShake(26,0,2,34,-1,0,0);forever.commit();forever.dialogue(0);forever.push(26);forever.op(0x272);forever.dialogue(1);forever.commit();forever.dialogue(0);forever.op(0xf4);Fixture indefinite(std::move(forever),true);auto first=startGlobalPulse(indefinite);check(std::abs(number(layerObject(first,26),"y")-152.253333449)<1e-4&&number(layerObject(first,26),"x")==150,"actual26 indefinite mode0 begins with native seed1 internalY.3755556 and geometry boost");state=indefinite.pulseTick("1017");check(sameLayerXY(state,first,26),"actual26 indefinite mode0 retains one offset inside34ms period");state=indefinite.pulseTick("1018");check(number(state,"nextTickMs")==16&&!boolean(state,"canSave")&&std::abs(number(layerObject(state,26),"y")-143.013333321)<1e-4,"negative duration samples the native next pair at34ms without envelope attenuation");double moving=number(layerObject(state,26),"y");state=indefinite.command("advance");check(number(layerObject(state,26),"y")==moving&&!boolean(state,"canSave"),"272 queues random cancellation instead of removing the current offset immediately");uint64_t clock=1018;state=commitNext(indefinite,clock);check(number(layerObject(state,26),"x")==150&&number(layerObject(state,26),"y")==150&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"scene-ready272 commit clears indefinite random state and returns to idle");indefinite.passed=true;
}
static void layerRandomIndependentReplacementAndZero(){
    {
        Program p;p.image(1000,"fake",13);p.position(13,50,0);p.layerShake(13,0,18,180,1500,0,0);p.commit();p.dialogue(0);p.position(13,50,100);p.dialogue(1);p.op(0xf4);Fixture f(std::move(p),true);auto state=startGlobalPulse(f);check(std::abs(number(layerObject(state,13),"y")+23.239999771)<1e-4,"first random event below the projected threshold is not geometry-boosted");state=f.command("advance");check(std::abs(number(layerObject(state,13),"y")-376.760000229)<1e-4,"moving the canonical image during a held period does not resample or double its old local offset");state=f.pulseTick("1164");check(std::abs(number(layerObject(state,13),"y")-288.540798187)<1e-4,"next random event applies the new projected geometry once at its own deadline");f.passed=true;
    }
    auto make=[](bool sine){Program p;p.image(1000,"fake",13);p.image(1000,"fake",23);p.layerShake(13,0,18,180,-1,0,0);if(sine)p.layerShake(23,4,0,300,-1,0,2);p.commit();p.dialogue(0);if(sine)p.layerShake(13,0,0,0,0,-7,0);p.commit();p.dialogue(1);p.op(0xf4);return p;};Fixture mixed(make(true),true),plain(make(false),true);startGlobalPulse(mixed);startGlobalPulse(plain);auto a=mixed.pulseTick("1164"),b=plain.pulseTick("1164");check(sameLayerXY(a,b,13)&&number(layerObject(a,23),"x")!=150,"mode2 activity uses no random values and remains independent of mode0");uint64_t ca=1164,cb=1164;a=commitNext(mixed,ca);b=commitNext(plain,cb);check(sameLayerXY(a,b,13),"both-zero random request does not replace the old job or validate unused bad timing");a=mixed.pulseTick("1344");b=plain.pulseTick("1344");check(sameLayerXY(a,b,13),"zero-amplitude request preserves RNG stream and original next-event deadline");mixed.passed=plain.passed=true;
    Program replace;replace.image(1000,"fake",13);replace.layerShake(13,0,18,180,-1,0,0);replace.commit();replace.dialogue(0);replace.layerShake(13,4,0,300,300,200,2);replace.commit();replace.dialogue(0);replace.layerShake(13,0,2,34,-1,0,0);replace.commit();replace.dialogue(1);replace.push(13);replace.op(0x272);replace.commit();replace.dialogue(0);replace.op(0xf4);Fixture f(std::move(replace),true);startGlobalPulse(f);auto state=f.pulseTick("1084");double y=number(layerObject(state,13),"y");state=f.command("advance");check(number(layerObject(state,13),"y")==y,"pending mode2 replacement leaves the old held random offset until commit");state=f.command("advance");f.command("transitionReady","2");state=f.pulseTick("100000");check(number(layerObject(state,13),"x")==150&&number(layerObject(state,13),"y")==150,"new delayed mode2 clears old mode0 immediately at commit rather than carrying its random offset through delay");state=f.pulseTick("100183");check(number(layerObject(state,13),"x")==150,"new mode2 remains still until its own delay expires");f.pulseTick("100184");state=f.pulseTick("100259");check(std::abs(number(layerObject(state,13),"x")-161.25)<1e-4&&number(layerObject(state,13),"y")==150,"mode2 replacement starts with independent analytic phase after200ms delay");state=f.pulseTick("100484");check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"replacement's finite endpoint releases random and analytic save guards");uint64_t clock=100484;state=commitNext(f,clock);double randomY=number(layerObject(state,13),"y");check(number(layerObject(state,13),"x")==150&&number(state,"nextTickMs")==16&&!boolean(state,"canSave"),"a later mode0 replacement has only its own random axes and scheduling");state=f.pulseTick(std::to_string(clock+10).c_str());check(number(layerObject(state,13),"y")==randomY,"replacement mode0 keeps its first new event for a complete period");clock+=10;state=commitNext(f,clock);check(number(layerObject(state,13),"y")==150&&boolean(state,"canSave"),"272 also cancels a mode0 replacement without retaining previous mode2 state");f.passed=true;
}
static void global270NativeGroupsAndScheduler(){
    Program p;for(int id:{0,1,2,63})p.image(1000,"fake",id);p.layerShake(-1,0,20,34,1000,400,1);for(int n:{0,1,100000})p.push(n);p.op(0x110);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");
    check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0,"normal110 prepares all pending local requests through the existing GPU acknowledgement");
    state=f.command("tick","900000");check(number(layerObject(state,2),"y")==150,"110 loading cannot consume delay or shared RNG");
    state=f.command("transitionReady","1");check(waiting(state,"frame"),"110 resumes its own duration after prepared commit");
    f.command("tick","1000");for(int n=2;n<=8;++n){state=f.command("tick",std::to_string(n*1000).c_str());check(number(layerObject(state,0),"y")==150,"coarse frames consume at most50ms of local delay each");}
    state=f.command("tick","9000");
    check(std::abs(number(layerObject(state,0),"y")-81.213335037)<1e-4&&std::abs(number(layerObject(state,1),"y")-237.573331833)<1e-4,"native primary0/1 draw separate pairs at first elapsed0 after late delay");
    check(number(layerObject(state,2),"y")==number(layerObject(state,1),"y")&&number(layerObject(state,63),"y")==number(layerObject(state,1),"y"),"followers use primary1 pre-geometry cache without extra RNG or envelope");
    auto held=state;for(int i=0;i<8;++i){state=f.command("autoDelay","2400");check(visibleLayers(state)==visibleLayers(held),"same-tick UI updates cannot advance local clocks or consume RNG");}f.passed=true;
    Program wait;wait.image(1000,"fake",2);wait.layerShake(2,0,20,34,200,400,0);for(int n:{1,128,2})wait.push(n);wait.op(0x27f);wait.dialogue(0);wait.op(0xf4);
    Fixture w(std::move(wait),true);w.command("textSpeed","0");state=w.command("action","start");state=w.command("transitionReady","1");check(waiting(state,"animation"),"mask128 waits on actual finite native local jobs");w.command("tick","1000");
    for(int n=2;n<=12;++n){state=w.command("tick",std::to_string(n*1000).c_str());check(waiting(state,"animation"),"global deadline cannot bypass capped local delay or active time");}
    state=w.command("tick","13000");check(waiting(state,"input")&&number(layerObject(state,2),"y")==150&&boolean(state,"canSave"),"finite mask128 releases only after the local callback actually completes");w.passed=true;
    Program loop;loop.image(1000,"fake",23);loop.layerShake(23,0,2,34,-1,0,1);for(int n:{1,128,23})loop.push(n);loop.op(0x27f);loop.dialogue(0);loop.op(0xf4);
    Fixture l(std::move(loop),true);l.command("textSpeed","0");state=l.command("action","start");state=l.command("transitionReady","1");check(waiting(state,"input")&&!boolean(state,"canSave")&&number(state,"nextTickMs")==16,"selected script1 uses native random math and indefinite mask128 does not wait");l.passed=true;
}
static void global270IdleCacheSaveAndLegacy(){
    Program p;p.image();p.image(1000,"fake",23);p.layerShake(0,0,20,34,-1,0,0);p.commit();p.dialogue(0);p.push(0);p.op(0x241);p.dialogue(1);p.layerShake(-1,0,20,34,-1,0,1);p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);startGlobalPulse(f);auto state=f.command("advance");check(boolean(state,"canSave")&&number(state,"nextTickMs")==0,"missing primary removal leaves idle cache without permanently denying saves");
    f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(good.hostExtra.begin(),good.hostExtra.end());const auto at=extra.find("LOCAL270CACHE1 ");check(currentHostSchema(good)&&at!=std::string::npos,"WH14 appends only the portable two-float local cache footer");
    std::istringstream values(extra.substr(at));std::string tag;float x,y;values>>tag>>x>>y;check(x==0&&std::abs(y+11.4644441605f)<1e-6,"idle cache stores pre-geometry native offset rather than visible doubled output");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.testPlatformAnchor=false;f.command("textSpeed","0");state=f.command("load","2");check(boolean(state,"canSave"),"fresh Runtime instance restores stable WH14 cache");
    uint64_t clock=500000;state=commitNext(f,clock);check(std::abs(number(layerObject(state,23),"y")-(150+double(y)*6))<1e-5,"fresh follower-only job uses restored idle cache without a fabricated missing primary");auto live=visibleLayers(state);
    for(const std::string tail:{"LOCAL270CACHE1 0\n","LOCAL270CACHE1 nan 0\n","LOCAL270CACHE1 0 inf\n","LOCAL270CACHE1 1e40 0\n","WRONG 0 0\n","LOCAL270CACHE1 1 2\nLOCAL270CACHE1 3 4\n"}){auto bad=good;auto bytes=extra.substr(0,at)+tail;bad.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==live,"malformed WH14 footer cannot partly replace live VM, geometry or cache");}
    auto old=good;auto legacy=extra.substr(0,at);legacy.replace(0,9,"WOHHOST13");old.hostExtra.assign(legacy.begin(),legacy.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",old);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&boolean(state,"canSave"),"actual WH13 without the footer still loads with default zero cache");clock=600000;state=commitNext(f,clock);check(number(layerObject(state,23),"y")==150,"legacy13 migration does not invent retained random offsets");f.passed=true;
}
static void capturedSceneTransition(){
    Program program;program.image();program.dialogue(0);for(auto n:{0,1,0})program.push(n);program.op(0x253);
    for(auto n:{0,256,0,0,1000,0})program.push(n);program.op(0x200);program.dialogue(1);program.op(0xf4);
    Fixture fixture(std::move(program),true);fixture.firstComplete();auto state=fixture.command("advance");
    check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0&&number(state,"oldOpacity")==1,"new scene waits for a prepared old-frame capture");
    auto transition=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(transition,"id")));auto old=transition.substr(transition.find("\"old\":"));
    check(number(state,"colorMode")==1&&number(old,"colorMode")==0&&old.find("abcdefghij")!=std::string::npos,"old image state and native text are frozen before the new scene commit");
    state=fixture.command("tick","50000");check(waiting(state,"transitionReady")&&number(state.substr(state.find("\"transition\":")),"elapsedMs")==0,"asset loading time cannot consume a transition");
    state=fixture.command("transitionReady","9999");check(waiting(state,"transitionReady"),"stale preparation acknowledgement is ignored");
    fixture.command("transitionReady",id.c_str());fixture.pulseTick("100000");state=fixture.pulseTick("100484");
    check(waiting(state,"transition")&&number(state.substr(state.find("\"transition\":")),"elapsedMs")==500&&number(state,"oldOpacity")==.5,"native captured frame fades over live new scene");
    state=fixture.pulseTick("100984");check(waiting(state,"input")&&state.find("\"transition\":null")!=std::string::npos,"completed transition releases its capture and resumes CHS");fixture.passed=true;
}
static void immediateCommitWaitsForAssets(){
    Program program;program.image();for(int i=0;i<5;++i)program.push(0);program.op(0x245);
    for(auto n:{0,256,0,0,1200,1})program.push(n);program.op(0x200);for(auto n:{0,100,255,0,0})program.push(n);program.op(0x245);program.dialogue(0);program.op(0xf4);
    Fixture fixture(std::move(program),true);auto state=fixture.command("action","start");
    check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0,"cut commit waits for new textures rather than consuming a short later fade");
    auto t=state.substr(state.find("\"transition\":"));auto id=std::to_string(int(number(t,"id")));check(number(t,"mode")==1&&number(t,"oldOpacity")==0&&t.find("\"layers\":[]")!=std::string::npos&&boolean(state,"preload"),"cut commit needs no old-frame capture or fade duration");
    fixture.command("tick","900000");state=fixture.command("transitionReady",id.c_str());check(waiting(state,"frame")&&boolean(state,"preload"),"prepared cut pins even hidden textures until CHS resumes without its unused duration");
    state=fixture.command("tick","900016");check(waiting(state,"input")&&state.find("\"transition\":null")!=std::string::npos&&number(state,"alpha")==0&&boolean(state,"preload"),"short fade begins with its hidden texture already prepared");
    state=fixture.command("tick","900116");check(number(state,"alpha")==1,"entire short fade runs after texture preparation");fixture.passed=true;
}
static void idleAppendClock(){
    Fixture fixture(appendProgram());const auto first=fixture.firstComplete();
    const auto next=fixture.command("advance");check(number(next,"instructions")>number(first,"instructions")&&boolean(next,"revealing"),"next manual tap starts appended typewriter text");
    const auto frame=fixture.command("tick","11516");
    check(boolean(frame,"revealing")&&number(frame,"visibleCharacters")<21,"idle reading gap must not instantly reveal next text");
    check(number(frame,"instructions")==number(next,"instructions"),"first typewriter tick cannot consume next page");fixture.passed=true;
}
static void idleAutoClock(){
    Fixture fixture(appendProgram());const auto first=fixture.firstComplete();fixture.command("autoDelay","500");fixture.command("auto");
    const auto beforePreview=std::string(woh_snapshot(fixture.handle));const auto preview=fixture.command("historyPreview");
    check(preview.find("\"scene\":\"play\"")!=std::string::npos&&preview.find("\"history\":[]")==std::string::npos,"history preview exposes rows without entering history mode");
    check(std::string(woh_snapshot(fixture.handle))==beforePreview,"history preview leaves runtime version, clock, mode and dialogue unchanged");
    auto state=fixture.command("tick","11516");check(number(state,"instructions")==number(first,"instructions"),"enabling auto after idle must not immediately advance");
    state=fixture.command("tick","11916");check(number(state,"instructions")==number(first,"instructions"),"auto waits its configured new delay");
    state=fixture.command("tick","12028");check(number(state,"instructions")>number(first,"instructions"),"auto advances after elapsed configured delay");fixture.passed=true;
    Fixture held(appendProgram());held.firstComplete();
    check(boolean(held.command("skip","1"),"skipping"),"long press explicitly starts skip");
    check(boolean(held.command("skip","1"),"skipping"),"repeated hold start cannot toggle skip off");
    auto stopped=held.command("skip","0");check(!boolean(stopped,"skipping"),"release explicitly stops skip");
    check(!boolean(held.command("skip","0"),"skipping"),"late duplicate release cannot restart skip");
    state=held.command("tick","999999");check(number(state,"instructions")==number(stopped,"instructions"),"released hold does not continue advancing on the next tick");held.passed=true;
    Program voiced;voiced.push(1);voiced.string("fake");voiced.op(0x1a0);voiced.dialogue(0);voiced.dialogue(1);voiced.op(0xf4);
    Fixture speaking(voiced);const auto spoken=speaking.firstComplete();auto voiceId=uint64_t(number(audioChannel(spoken,"voice"),"id"));speaking.command("autoDelay","500");speaking.command("auto");
    const auto voiceBefore=std::string(woh_snapshot(speaking.handle));speaking.command("historyPreview");check(std::string(woh_snapshot(speaking.handle))==voiceBefore,"read-only preview preserves voice channel and auto timer");
    speaking.command("tick","999999");state=speaking.command("tick","1000600");check(number(state,"instructions")==number(spoken,"instructions"),"auto deadline cannot cut off current voice");
    check(number(state,"nextTickMs")==0,"static dialogue sleeps while waiting only for voice completion");
    speaking.command("audioEnded",("voice "+std::to_string(voiceId-1)).c_str());state=speaking.command("tick","1000616");check(number(state,"instructions")==number(spoken,"instructions"),"stale completion cannot unblock current voice");
    speaking.command("audioEnded",("voice "+std::to_string(voiceId)).c_str());state=speaking.command("tick","1000632");check(number(state,"instructions")>number(spoken,"instructions"),"auto advances when voice and delay have both completed");speaking.passed=true;
    Fixture early(voiced);const auto initial=early.firstComplete();voiceId=uint64_t(number(audioChannel(initial,"voice"),"id"));early.command("autoDelay","10000");early.command("auto");early.command("audioEnded",("voice "+std::to_string(voiceId)).c_str());
    state=early.command("tick","11516");check(number(state,"instructions")==number(initial,"instructions"),"early voice completion still waits for configured auto delay");early.passed=true;
}
static void revealDoesNotReplaceFrameDeadline(){
    Program program;program.dialogue(0,false);program.push(0);program.push(0);program.push(1000);program.op(0x110);program.dialogue(1);program.op(0xf4);
    Fixture fixture(std::move(program));const auto first=fixture.command("action","start");check(waiting(first,"frame")&&boolean(first,"revealing"),"nonblocking text overlaps frame wait");
    fixture.command("tick","1000");auto state=fixture.command("tick","1400");check(waiting(state,"frame")&&!boolean(state,"revealing"),"text finishes during original frame wait");
    state=fixture.command("tick","2000");check(waiting(state,"input")&&number(state,"instructions")>number(first,"instructions"),"reveal completion preserves original1000ms frame deadline");fixture.passed=true;
}
static void disabledLayerSaveRestore(){
    Program program;for(int i=0;i<9;++i)program.push(i==6?-1:0);program.string("CC");program.push(0);program.push(0);program.push(255);program.push(1000);program.push(136);program.push(240);program.string("BLACK");program.push(0);program.op(0x240);
    program.push(0);program.push(0);program.op(0x25a);program.dialogue(0);
    program.push(1);program.push(0);program.op(0x25a);program.dialogue(1);program.op(0xf4);
    Fixture fixture(std::move(program));fixture.command("action","start");fixture.command("advance");
    auto state=fixture.command("save","2");check(state.find("\"notice\":\"Saved\"")!=std::string::npos,"save disabled layer at input boundary");
    auto original=fixture.command("advance");check(number(original,"alpha")==1,"original enable reveals preserved layer alpha");
    state=fixture.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos,"load disabled layer state");
    state=fixture.command("advance");check(number(state,"alpha")==number(original,"alpha"),"re-enabled layer preserves alpha after save and load");fixture.passed=true;
}
static void simultaneousMotionAndScale(){
    Program program;program.image();
    for(auto value:{0,0,1000,1000,2000,1000,1000,0})program.push(value);program.op(0x246);
    program.string("CC");for(auto value:{0,1000,50,100,INT32_MIN,INT32_MIN,0,0})program.push(value);program.op(0x243);program.dialogue(0);program.op(0xf4);
    Fixture split(program,true),direct(program,true);split.command("action","start");direct.command("action","start");split.command("tick","1000");direct.command("tick","1000");
    split.command("tick","1484");const auto a=split.command("tick","1734"),b=direct.command("tick","1734");
    check(std::abs(number(a,"w")-175)<1e-7&&std::abs(number(b,"w")-175)<1e-7,"scale reaches same750ms geometry");
    check(std::abs(number(a,"x")-number(b,"x"))<1e-7,"simultaneous motion and scale are frame subdivision invariant");
    check(std::abs(number(a,"x")-262.5)<1e-7&&std::abs(number(a,"y")-150)<1e-7,"latest scaled dimensions preserve interpolated center anchor");split.passed=true;direct.passed=true;
}
static void effectAbsoluteScaleAndCompletion(){
    Program program;program.image(1400);for(auto value:{0,0,0,28,0})program.push(value);program.op(0x278);program.dialogue(0);program.op(0xf4);
    const std::string table="28 0 -32767 -32767 -32767 1400 1400 -100001 0 0 0 1000\n28 1000 -32767 -32767 -32767 1400 1400 -100001 0 0 0 1000\n";
    Fixture fixture(std::move(program),true,table);fixture.command("action","start");auto state=fixture.command("tick","1000");
    check(std::abs(number(state,"w")-196)<1e-7&&std::abs(number(state,"h")-196)<1e-7,"effect1400 axis scale multiplies independent uniform1400 scale");
    check(std::abs(number(state,"x")-102)<1e-7&&std::abs(number(state,"y")-102)<1e-7,"axis effect preserves the separate world anchor and O/P");
    state=fixture.command("tick","2500");check(!boolean(state,"revealing")&&waiting(state,"input")&&number(state,"nextTickMs")==0,"loopStart0 effect completes and stops idle rendering ticks");
    check(std::abs(number(state,"w")-196)<1e-7,"completed one-shot effect preserves both uniform and final axis scales");fixture.passed=true;
}
static void textControlSaveMigration(){
    Program program;program.push(1);program.push(1);program.push(0);program.push(0);program.string("@x$000000");program.op(0x140);program.op(0xf4);
    Fixture fixture(std::move(program));auto state=fixture.command("action","start");
    check(state.find("@x")==std::string::npos&&state.find("abcdefghij")!=std::string::npos,"native text control is consumed before rendering");
    fixture.command("advance");fixture.command("save","2");const auto path=fixture.root/"savedata/slot2.whs";
    auto previous=mahoyo::save::read(path);{std::string extra(previous.hostExtra.begin(),previous.hostExtra.end());auto at=extra.find("LANGUAGETEXT1 ");check(at!=std::string::npos,"new language footer exists");extra.resize(at);extra.replace(0,9,"WOHHOST22");previous.hostExtra.assign(extra.begin(),extra.end());}previous.text="before @xafter。　続き、　次";previous.history={"@xold history","before @xafter。　続き"};
    {std::string extra(previous.hostExtra.begin(),previous.hostExtra.end());const auto clockFooter=extra.substr(extra.find("WAITBASELINE1 "));extra.resize(extra.find("HISTORYMETA1 "));mahoyo::history::State metadata;metadata.legacy(previous.history.size());std::ostringstream out;mahoyo::history::write(out,metadata,previous.history.size());extra+=out.str()+clockFooter;previous.hostExtra.assign(extra.begin(),extra.end());}
    mahoyo::save::writeAtomic(path,previous);
    state=fixture.command("load","2");check(state.find("before after")!=std::string::npos&&state.find("@x")==std::string::npos,"current-version saved paragraph is repaired in place");
    check(state.find("after。続き、次")!=std::string::npos,"old saved dialogue spacing is compacted before reveal restore");
    state=fixture.command("history");check(state.find("old history")!=std::string::npos&&state.find("@x")==std::string::npos,"saved history controls do not leak into the native UI");fixture.passed=true;
}
static void fontText(Program& p,const std::string& text,int wait=0,int page=0){for(int n:{page,wait,0,0})p.push(n);p.string(text);p.op(0x140);}
static void continuationSpacing(){
    Program p;fontText(p,"　始め、");fontText(p,"　続き。　次\n　段落",1,1);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");
    for(int i=1;i<=6&&!waiting(state,"input");++i)state=f.command("tick",std::to_string(i*1000).c_str());
    check(state.find("　始め、続き。次")!=std::string::npos&&state.find("　段落")!=std::string::npos,"140 continuation compacts punctuation spaces across producers while retaining new paragraph indentation");
    f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");check(saved.text=="　始め、続き。次\n　段落","saved body uses the same canonical text as display and history");f.passed=true;
}
static void completedDialogueLineBreaks(){
    for(bool explicitBreak:{false,true}){
        Program p;fontText(p,"FIRST",1,1);if(explicitBreak)p.op(0x142);fontText(p,"SECOND",1,1);p.op(0xf4);
        Fixture f(std::move(p));f.command("textSpeed","0");f.firstComplete();f.command("save","2");f.command("load","2");
        auto state=f.command("advance");check(state.find("FIRST\\u000aSECOND")!=std::string::npos,"completed dialogue starts next line after save/load without duplicating explicit142");
        check(number(state,"visibleCharacters")==11,"implicit line break consumes no reveal character");check(number(state,"activeFrom")==5,"only the current completed dialogue is opaque");check(boolean(state,"cursor"),"completed input wait exposes original cursor");check(state.find("\"x\":300,\"y\":120,\"w\":1320")!=std::string::npos&&state.find("\"fontSize\":52")!=std::string::npos,"fullscreen text keeps centered reading margins and larger font");f.passed=true;
    }
    for(const auto& item:std::vector<std::tuple<int,int,std::string,bool>>{{0,0,"",false},{0,1,"",true},{1,1,"@n",false},{1,1,"@e",true}}){
        auto [wait,page,suffix,lineBreak]=item;Program p;fontText(p,"FIRST"+suffix,wait,page);fontText(p,"SECOND",1,1);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");
        auto state=f.firstComplete();check(state.find(lineBreak?"FIRST\\u000aSECOND":"FIRSTSECOND")!=std::string::npos,"effective wait/page controls inline continuation versus native next line");check(number(state,"activeFrom")==size_t(lineBreak?5:0),"inline runs share the same active highlight boundary");f.passed=true;
    }
}
static void exactVariantOriginAndLegacyRestore(){
    for(int scale:{1000,1500}){
        Program p;p.image(scale,"portrait",12);p.clone(11,12,"portrait");p.push(1);p.push(1);p.push(11);p.op(0x256);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),false,"","portrait 144 370\n");f.imageHeader("portrait",506,1272);f.imageHeader("portrait_01_01",512,1526);f.command("textSpeed","0");auto old=f.firstComplete();
        check(std::abs(number(layerObject(old,11),"pivotX")-216./512)<1e-7,"legacy generated variant inherited base origin");f.command("save","2");auto legacy=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string bytes(legacy.hostExtra.begin(),legacy.hostExtra.end());bytes.replace(0,9,"WOHHOST21");bytes.resize(bytes.find("LANGUAGETEXT1 "));auto marker=bytes.find("EXACTLAYOUT1 ");check(marker!=std::string::npos,"current save has exact origin marker");bytes.erase(marker,bytes.find('\n',marker)-marker+1);legacy.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",legacy);
        auto path=f.game/"meta/native-exact-layouts.tsv";f.extraFiles.push_back(path);{std::ofstream out(path);out<<"WOHEXACTLAYOUTS1 1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b 2\nportrait 144 370 72 185\nportrait_01_01 154 380 77 190\n";}
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto migrated=f.command("load","2");auto layer=layerObject(migrated,11);check(std::abs(number(layer,"pivotX")-231./512)<1e-7&&std::abs(number(layer,"pivotY")-570./1526)<1e-7,"exact blur origin includes 15px padding on both axes");
        f.command("save","2");auto modern=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string metadata(modern.hostExtra.begin(),modern.hostExtra.end());check(metadata.find("EXACTLAYOUT1 1\n")!=std::string::npos,"origin migration records its authority");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto again=f.command("load","2");check(visibleLayers(again)==visibleLayers(migrated),"exact-origin save reload is idempotent");f.command("title");auto fresh=f.firstComplete();check(visibleLayers(fresh)==visibleLayers(migrated),"legacy repair matches a fresh execution at both scales");f.passed=true;
    }
}
static bool savedFontCarry(const mahoyo::save::Document& document){std::string extra(document.hostExtra.begin(),document.hostExtra.end());auto at=extra.find("FONTCARRY1 ");check(at!=std::string::npos,"current schema stores explicit next-font state");return extra.substr(at,extra.find('\n',at)-at)=="FONTCARRY1 1";}
static std::string reachFontModal(Fixture& f,std::string state,uint64_t& platform){
    for(int i=0;i<12&&(!waiting(state,"teatime")||!modalPhase(state,"ready"));++i){if(waiting(state,"input"))state=f.command("advance");else{platform+=1000;state=f.command("tick",std::to_string(platform).c_str());}}
    check(waiting(state,"teatime")&&modalPhase(state,"ready")&&boolean(state,"canSave"),"font fixture reaches a safe Ready modal");return state;
}
static void nativeFontReset14b(){
    {
        Program p;p.image();p.push(777);p.op(0x14b);p.push(0);p.op(0x251);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);auto state=f.firstComplete();
        check(number(layerObject(state,0),"z")==777&&state.find("\"text\":\"abcdefghij\"")!=std::string::npos,"14b consumes0/pushes0 and adds no yield, text or font marker when already default");
        check(number(state,"nextTickMs")==0&&boolean(state,"canSave"),"default font reset schedules no job and does not change save readiness");f.passed=true;
    }
    Program p;p.image();p.clone(1,0,"fake");p.sound190(3,0,"fake",70,13);fontText(p,"@iA");
    for(int n:{0,1,100})p.push(n);p.op(0x110);p.op(0x14b);p.op(0x14b);
    for(int n:{0,1,100})p.push(n);p.op(0x110);p.push(2);p.op(0x13c);fontText(p,"B",1,1);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto before=f.command("action","start");auto layers=visibleLayers(before),audio=audioChannel(before,"se3");
    f.command("tick","1000");auto reset=f.command("tick","1084");
    check(waiting(reset,"frame")&&reset.find("\"text\":\"@iA\"")!=std::string::npos,"14b leaves the current raw paragraph and its italic glyphs unchanged");
    check(visibleLayers(reset)==layers&&audioChannel(reset,"se3")==audio&&number(reset,"visibleCharacters")==1&&!boolean(reset,"revealing"),"14b leaves cloned images, current sound, reveal and current font cache unchanged");
    uint64_t platform=1084;auto ready=reachFontModal(f,reset,platform);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");
    check(good.text=="@iA"&&good.history==std::vector<std::string>{"@iA"}&&!savedFontCarry(good),"14b saves the future default state separately without rewriting current text or history");
    for(const auto& footer:{std::string("FONTCARRY1 2\n"),std::string("FONTCARRY1 -1\n"),std::string("FONTCARRY1\n"),std::string("FONTCARRY1 0\nextra\n"),std::string()}){
        auto bad=good;std::string extra(bad.hostExtra.begin(),bad.hostExtra.end());extra.resize(extra.find("FONTCARRY1 "));extra+=footer;bad.hostExtra.assign(extra.begin(),extra.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);
        auto rejected=f.command("load","2");check(rejected.find("\"notice\":\"Loaded\"")==std::string::npos&&modalObject(rejected)==modalObject(ready)&&visibleLayers(rejected)==layers&&number(rejected,"instructions")==number(ready,"instructions"),"malformed WH15 font footer rejects atomically without replacing the live modal, VM or layers");
    }
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");ready=f.command("load","2");
    auto after=finishTeatime(f,ready,platform+100000);check(after.find("@iA@rB")!=std::string::npos,"fresh Runtime applies the pending default font only when next140 appends B");
    f.command("save","2");check(mahoyo::save::read(f.root/"savedata/slot2.whs").history==std::vector<std::string>{"@iA","B"},"14b never rewrites earlier raw history or invents its own entry");f.passed=true;
}
static void nativeFontCarryCompletionClearAndLegacy(){
    for(const auto& item:std::vector<std::tuple<int,int,std::string,bool>>{{0,0,"",true},{1,0,"",false},{0,1,"",false},{1,1,"",false},{1,1,"@n",true},{1,1,"@e",false},{1,0,"@e",true},{0,0,"@k",true},{0,0,"@rB@iC",true}}){
        auto [wait,page,suffix,expected]=item;Program p;fontText(p,"@iA"+suffix,wait,page);p.push(2);p.op(0x13c);fontText(p,"Z",1,1);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");uint64_t platform=1000;auto ready=reachFontModal(f,f.command("action","start"),platform);f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");check(savedFontCarry(document)==expected,"140 completion applies Native effective wait/page and inline font reset semantics");
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");ready=f.command("load","2");auto state=finishTeatime(f,ready,platform+100000);check(state.find(expected?"@iAZ":"@iA\\u000a@rZ")!=std::string::npos||suffix=="@rB@iC"&&state.find("@iA@rB@iCZ")!=std::string::npos,"saved carry affects only the next appended text run");
        if(wait==0&&page==0&&suffix.empty()){
            std::string old(document.hostExtra.begin(),document.hostExtra.end());old.replace(0,9,"WOHHOST14");old.resize(old.find("FONTCARRY1 "));document.hostExtra.assign(old.begin(),old.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",document);ready=f.command("load","2");state=finishTeatime(f,ready,platform+200000);check(state.find("@iA@rZ")!=std::string::npos,"actual WH14 footer loads with default-false carry while preserving its displayed paragraph");
        }
        f.passed=true;
    }
    for(int clear:{0x141,0x151,0x158}){
        Program p;fontText(p,"@iA");p.op(clear);fontText(p,"B",1,1);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"frame"),"glyph clear has its existing independent frame boundary");state=f.command("tick","1000");check(state.find("\"text\":\"@iB\"")!=std::string::npos,"141/151/158 clear displayed glyphs without implicitly discarding persistent font carry");f.passed=true;
    }
}
static void nativeMetadataAnchor(){
    Program program;program.image();program.dialogue(0);program.op(0xf4);
    Fixture fixture(std::move(program),true,"","fake\t20\t20\n");fixture.command("action","start");auto state=fixture.command("advance");
    check(std::abs(number(state,"x")-170)<1e-8&&std::abs(number(state,"pivotX")-.3)<1e-8,"CC uses native metadata*3 rather than padded PNG dimensions");
    fixture.command("save","2");auto document=mahoyo::save::read(fixture.root/"savedata/slot2.whs");document.layers[0].x=150;document.layers[0].y=150;
    std::string previous="WOHHOST4\n1\n0 0 .5 .5 1 1 \"fake.mzp\" 0 0 0 0 0 0 0 0 0\n0\n";document.hostExtra.assign(previous.begin(),previous.end());mahoyo::save::writeAtomic(fixture.root/"savedata/slot2.whs",document);
    state=fixture.command("load","2");check(std::abs(number(state,"x")-170)<1e-8&&std::abs(number(state,"y")-170)<1e-8,"old save anchor is repaired without replaying the scene");
    fixture.command("save","2");state=fixture.command("load","2");check(std::abs(number(state,"x")-170)<1e-8,"new save is not migrated twice");fixture.passed=true;
}
static void geometryNear(const std::string& state,int id,double x,double y,double w,double h,const char* reason){auto l=layerObject(state,id);check(std::abs(number(l,"x")-x)<1e-5&&std::abs(number(l,"y")-y)<1e-5&&std::abs(number(l,"w")-w)<1e-5&&std::abs(number(l,"h")-h)<1e-5,reason);}
static const std::string cloneLayouts="source\t21\t11\nreplacement\t41\t31\n";
static void cloneIntrinsicScaleAndFreshSave(){
    Program p;p.image(1500,"source");p.axisScale(0,2000,500);for(int n:{3,3,0})p.push(n);p.op(0x256);
    p.clone(1,0,"replacement");p.clone(2,0,"replacement",500);p.clone(3,0,"replacement",INT32_MIN,"LT");p.commit();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),false,"",cloneLayouts);for(const auto& name:{"source","source_03_03"})f.imageHeader(name,100,80);f.imageHeader("replacement_03_03",200,120);
    f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(state,"nextTickMs")==0,"cloned new intrinsic/variant waits for actual graphics preparation");
    const auto prepared=visibleLayers(state);state=f.command("tick","99999");check(visibleLayers(state)==prepared&&waiting(state,"transitionReady"),"loading time does not alter the clone or execute the pending paragraph");
    geometryNear(state,0,110,188.75,300,60,"246 retains source U1.5 and odd native layout uses integer half before3x projection");
    geometryNear(state,1,20,166.25,600,90,"242 recomputes replacement intrinsic with inherited U and separate S axes");
    geometryNear(state,2,140,188.75,200,30,"explicit clone U.5 preserves inherited S2/.5");
    check(layerObject(state,1).find("replacement_03_03.mzp")!=std::string::npos&&layerObject(state,2).find("replacement_03_03.mzp")!=std::string::npos,"new basename inherits both source variants instead of resetting them");
    check(number(layerObject(state,1),"pivotX")==.3&&number(layerObject(state,1),"pivotY")==.375,"both default origin axes move to replacement native origin");
    geometryNear(state,3,20,166.25,600,90,"clone LT anchor bypasses normal creation anchor processing just like CC");
    auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(uint64_t(number(tail,"id")));f.command("transitionReady",id.c_str());state=f.command("tick","100000");check(waiting(state,"input"),"asset acknowledgement releases a single cut boundary into text");
    f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");check(currentHostSchema(document),"clone uses the current WH12 schema");auto row=savedLayerFields(document,2);check(row.size()==31&&std::stod(row[26])==-60&&std::stod(row[27])==-45&&std::stod(row[28])==.5&&std::stod(row[29])==2&&std::stod(row[30])==.5,"WH12 serializes independent clone O/U/S rather than inferring them from output dimensions");
    auto before=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&visibleLayers(state)==before,"fresh WH12 load restores the variant, intrinsic, O/P and U/S without replay");f.passed=true;
}
static void cloneCustomOriginsAreAllOrNothing(){
    struct Origin {int x,y;double pivotX,pivotY;};
    for(const auto& origin:std::array<Origin,4>{{{8,4,.3,.375},{8,0,.15,0},{0,4,0,.125},{16,69,.315,2.3}}}){
        Program p;p.image(1000,"source");p.origin(0,origin.x,origin.y);p.clone(1,0,"replacement");p.clone(2,0,"replacement",INT32_MIN,"LT");p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),false,"",cloneLayouts);f.imageHeader("source",100,80);f.imageHeader("replacement",200,120);f.command("textSpeed","0");auto state=f.command("action","start");
        geometryNear(state,0,170,185,100,80,"unit scale 250 changes only P and leaves source O/image position unchanged");
        geometryNear(state,1,140,155,200,120,"clone applies the old/new default-origin delta only to independent O");
        geometryNear(state,2,140,155,200,120,"explicit LT does not reset clone custom origins");
        auto l=layerObject(state,1);check(std::abs(number(l,"pivotX")-origin.pivotX)<1e-8&&std::abs(number(l,"pivotY")-origin.pivotY)<1e-8,"clone replaces P only when both source axes equal their defaults");
        f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");auto row=savedLayerFields(document,1);check(std::stod(row[26])==-60&&std::stod(row[27])==-45,"custom P never changes the separately saved clone O delta");
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());auto loaded=f.command("load","2");check(visibleLayers(loaded)==visibleLayers(state),"fresh process retains custom positive P including partially default axes");f.passed=true;
    }
}
static void cloneActiveLoopTerminalAndWh12Baseline(){
    Program p;p.image(1400,"source");for(int n:{0,0,0,28,0})p.push(n);p.op(0x278);p.dialogue(0);p.clone(1,0,"replacement");p.commit();p.dialogue(1);p.op(0xf4);
    const std::string table="28 0 -32767 -32767 -32767 1000 1000 -100001 0 0 200 1000\n28 1000 -32767 -32767 -32767 2000 500 -100001 0 0 200 1000\n";
    Fixture f(std::move(p),false,table,cloneLayouts);f.imageHeader("source",100,80);f.imageHeader("replacement",200,120);f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");auto state=f.command("advance");
    check(waiting(state,"transitionReady"),"active-loop clone still uses the graphics-ready cut barrier");
    //Native16BED0 asks for duration+1, so a1000/200 loop samples201ms.
    geometryNear(state,1,99.116,143.3315,336.28,151.116,"clone previews active loop at duration+1 wrap201, not live16/end1000/loopStart200");
    auto tail=state.substr(state.find("\"transition\":"));auto id=std::to_string(uint64_t(number(tail,"id")));auto prepared=visibleLayers(state);state=f.command("tick","999999");check(visibleLayers(state)==prepared,"graphics loading cannot consume the source loop timeline");f.command("transitionReady",id.c_str());state=f.command("tick","1000000");
    geometryNear(state,0,156.656,179.336,144.48,110.208,"source live loop continues independently at32ms after clone preparation");
    f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");std::istringstream input(std::string(saved.hostExtra.begin(),saved.hostExtra.end()));std::string row;size_t count;std::getline(input,row);check(row=="WOHHOST23","active clone fixture writes current WH20");input>>count;std::getline(input,row);for(size_t n=0;n<count;++n)std::getline(input,row);input>>count;std::getline(input,row);check(count==1,"cloning a terminal preview does not clone its playback job");std::getline(input,row);auto fields=hostFields(row);check(fields.size()==35&&std::stod(fields[29])==-30&&std::stod(fields[30])==-15&&std::stod(fields[31])==1.4&&std::stod(fields[32])==1&&std::stod(fields[33])==1&&fields[34]=="0","WH19 retains independent initial O/U/S and appends only decimal");
    auto before=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"fresh process restores active loop and static clone with distinct geometry baselines");f.command("tick","2000000");state=f.command("tick","2000484");
    geometryNear(state,0,135.656,184.586,214.48,82.208,"WH12 restored source loop applies U1.4 exactly once at532ms");
    geometryNear(state,1,99.116,143.3315,336.28,151.116,"terminal clone remains static while its source loop resumes");f.passed=true;
}
static std::string waveObject(const std::string& state,int id){auto layer=layerObject(state,id);auto at=layer.find("\"wave\":");check(at!=std::string::npos,"wave payload exists");return layer.substr(at+7);}
static bool hasWave(const std::string& state,int id){return layerObject(state,id).find("\"wave\":")!=std::string::npos;}
static std::string acknowledge(Fixture& f,const std::string& state){auto tail=state.substr(state.find("\"transition\":"));return f.command("transitionReady",std::to_string(uint64_t(number(tail,"id"))).c_str());}
static void waveBatchReadyAndCleanup(){
    Program p;for(int id:{26,27,17,18})p.image(1000,"fake",id);p.wave(26,2);p.wave(27,3);p.commit();p.dialogue(0);p.wave(17,4);p.wave(18,5);p.commit();p.dialogue(1);for(int id:{17,18}){p.push(id);p.op(0x241);}p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&!hasWave(state,26)&&!hasWave(state,27)&&number(state,"nextTickMs")==0,"258 remains pending before GPU acknowledgement");
    state=f.command("tick","999999");check(!hasWave(state,26),"loading wall time cannot apply a pending wave");state=acknowledge(f,state);
    check(number(waveObject(state,26),"preset")==2&&number(waveObject(state,27),"preset")==3&&number(waveObject(state,26),"strength")==1&&number(waveObject(state,26),"elapsedMs")==0,"presets2+3 coexist and begin with first draw phase0");
    state=f.command("tick","1000");check(waiting(state,"input")&&number(waveObject(state,26),"elapsedMs")==16&&boolean(state,"canSave")&&number(state,"nextTickMs")==16,"steady wave saves and schedules dynamic16ms samples");
    auto original=layerObject(state,26);check(number(original,"x")==150&&number(original,"y")==150&&number(original,"w")==100,"wave payload does not alter canonical layer geometry");
    state=f.command("advance");check(waiting(state,"transitionReady")&&hasWave(state,26)&&!hasWave(state,17),"new positive batch preserves old graphics until ready");state=acknowledge(f,state);
    check(!hasWave(state,26)&&!hasWave(state,27)&&number(waveObject(state,17),"preset")==4&&number(waveObject(state,18),"preset")==5,"positive replacement clears old active jobs but keeps new4+5 siblings");
    state=f.command("tick","1016");state=f.command("advance");check(!hasWave(state,26)&&!hasWave(state,27)&&state.find("\"wave\":")==std::string::npos&&number(state,"nextTickMs")==0,"deleting wave owners clears jobs and dynamic polling");f.passed=true;
}
static void waveWaitFadeAndPause(){
    Program p;p.image();p.wave(0,2,500);p.waitWave(0);p.dialogue(0);p.wave(0,0,500);p.waitWave(0);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"27fbit64 has a GPU-ready barrier");state=acknowledge(f,state);
    check(waiting(state,"input")&&number(waveObject(state,0),"strength")==0&&!boolean(state,"canSave"),"normal persistent wave is skipped by27f even during fade-in");f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"finite strength fade is unsafe to save");
    f.command("tick","1000");state=f.command("tick","1484");check(number(waveObject(state,0),"strength")==1&&number(waveObject(state,0),"elapsedMs")==500&&boolean(state,"canSave"),"500ms strength phase completes without ending wave animation");
    auto phase=waveObject(state,0);f.command("pause");state=f.command("tick","1000000");check(waveObject(state,0)==phase&&number(state,"nextTickMs")==0,"pause freezes wave strength and draw phase");f.command("resume");state=f.command("tick","1000000");check(number(waveObject(state,0),"elapsedMs")==516,"resume advances one logical frame, not paused wall time");
    f.command("history");state=f.command("tick","2000000");check(number(waveObject(state,0),"elapsedMs")==516,"history sampling rebases without advancing wave phase");state=f.command("history");state=f.command("tick","2000000");check(number(waveObject(state,0),"elapsedMs")==532,"history return cannot jump by its wall-time gap");
    state=f.command("advance");auto old=waveObject(state,0);state=f.command("tick","3000000");check(waiting(state,"transitionReady")&&waveObject(state,0)==old,"stop also stays pending during GPU-ready wait");state=acknowledge(f,state);check(waiting(state,"animation")&&number(waveObject(state,0),"strength")==1,"nonpersistent stop fade is waited by27fbit64");
    f.command("tick","3000000");state=f.command("tick","3000234");check(waiting(state,"animation")&&number(waveObject(state,0),"strength")==.5&&!boolean(state,"canSave"),"stop uses linear1to0 over500ms");state=f.command("tick","3000483");check(waiting(state,"animation"),"stop does not end at499ms");state=f.command("tick","3000484");check(waiting(state,"input")&&!hasWave(state,0)&&number(state,"nextTickMs")==0,"500ms stop emits NULL-equivalent cleanup and resumes script");f.passed=true;
}
static void waveSaveCloneMalformedAndLegacy(){
    Program p;p.image();p.wave(0,6);p.commit();p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));f.command("tick","1000");state=f.command("tick","1250");auto before=visibleLayers(state);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(good.hostExtra.begin(),good.hostExtra.end());check(currentHostSchema(good)&&extra.find("WAVES1 1\n")!=std::string::npos,"WH13 adds counted wave footer without changing layer/effect row schemas");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before&&boolean(state,"canSave"),"new Runtime rebases saved logical wave phase exactly");state=f.command("advance");check(hasWave(state,0)&&!hasWave(state,1),"clone242 copies ordinary geometry but never duplicates wave RT/state");auto live=visibleLayers(state);
    const auto marker=extra.rfind("WAVES1 ");check(marker!=std::string::npos,"wave footer boundary");
    for(const auto& tail:std::vector<std::string>{"WAVES1 65\n","WAVES1 1\n0 9 0 0 0 1 12\n","WAVES1 2\n0 2 0 0 0 1 12\n0 3 0 0 0 1 12\n","WAVES1 1\n1 2 0 0 0 1 12\n","WAVES1 1\n0 2 0 1 0 1 12\n","WAVES1 1\n0 2 0 0 0 0 12\n","WAVES1 1\n0 2 -1 0 0 1 0\n","WAVES1 1\n0 2 0 0 0 1 4294967296\n","WAVES1 1\n0 2\n","WAVES1 1\n0 2 0 0 2 1 0\n"}){auto bad=good;auto bytes=extra.substr(0,marker)+tail;bad.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==live,"malformed wave restore cannot partly replace live VM/layers/waves");}
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);state=f.command("load","2");check(visibleLayers(state)==before,"last good wave checkpoint remains loadable");state=f.command("title");check(state.find("\"wave\":")==std::string::npos&&number(state,"nextTickMs")==0,"title cleans active wave state and polling");f.passed=true;
    Program plain;plain.image();plain.dialogue(0);plain.op(0xf4);Fixture old(std::move(plain),true);old.command("textSpeed","0");auto original=old.command("action","start");old.command("save","2");auto document=host18Fixture(mahoyo::save::read(old.root/"savedata/slot2.whs"));std::string bytes(document.hostExtra.begin(),document.hostExtra.end());bytes.resize(bytes.find("WAVES1 "));bytes.replace(0,9,"WOHHOST12");document.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(old.root/"savedata/slot2.whs",document);state=old.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&!hasWave(state,0)&&visibleLayers(state)==visibleLayers(original),"existingWH12 with geometry footer migrates to emptywave");old.passed=true;
}
static void waveCapturedOldScenePhase(){
    Program p;p.image();p.wave(0,2);p.waitWave(0);p.dialogue(0);p.wave(0,3);for(int n:{0,256,0,0,500,0})p.push(n);p.op(0x200);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));f.command("tick","1000");state=f.command("tick","1250");auto original=waveObject(state,0);
    state=f.command("advance");check(waiting(state,"transitionReady"),"wave replacement is staged behind a fading scene barrier");auto old=state.substr(state.find("\"old\":{\"scene\""));check(waveObject(old,0)==original,"old transition captures exact preset2 phase rather than a live reference");
    state=acknowledge(f,state);f.command("tick","900000");state=f.command("tick","900234");old=state.substr(state.find("\"old\":{\"scene\""));check(waveObject(old,0)==original&&number(waveObject(state,0),"preset")==3&&number(waveObject(state,0),"elapsedMs")==250,"old wave phase stays frozen while replacement3 animates through transition");f.passed=true;
}
static void wavePopBoundsAndImplicitTextBarrier(){
    Program ignored;ignored.image();ignored.push(12345);ignored.wave(64,999,-1);ignored.op(0x131);ignored.op(0x251);ignored.dialogue(0);ignored.op(0xf4);Fixture f(std::move(ignored),true);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"input")&&number(state,"z")==12345&&!hasWave(state,0),"258 consumes3/no push and ignores invalid args forid>=64");f.passed=true;
    Program implicit;implicit.image();implicit.wave(0,3);implicit.dialogue(0);implicit.op(0xf4);Fixture text(std::move(implicit),true);text.command("textSpeed","0");state=text.command("action","start");check(waiting(state,"transitionReady")&&!hasWave(state,0),"140 native prepare also defers pending wave to a real resource acknowledgement");state=acknowledge(text,state);check(waiting(state,"input")&&number(waveObject(state,0),"elapsedMs")==0,"140 barrier resumes at the same dialogue with first phase0");text.passed=true;
    for(int kind=0;kind<3;++kind){Program bad;bad.image(1000,"fake",0,kind==2?std::array<int32_t,6>{{0,0,1920,1080,0,0}}:std::array<int32_t,6>{});bad.wave(kind==1?-1:0,kind==0?9:2);bad.commit();bad.dialogue(0);Fixture invalid(std::move(bad),true);std::string error=woh_command(invalid.handle,"action","start");if(kind==2){auto tail=error.substr(error.find("\"transition\":"));error=woh_command(invalid.handle,"transitionReady",std::to_string(uint64_t(number(tail,"id"))).c_str());}check(error.find("\"error\":\"\"")==std::string::npos,"unreviewed preset9/globalpositive/window context fails explicitly");invalid.passed=true;}
}
template<class F>static mahoyo::save::Document editSavedLayer(mahoyo::save::Document document,int id,F edit){
    std::istringstream input(std::string(document.hostExtra.begin(),document.hostExtra.end()));std::ostringstream output;std::string row;size_t count;
    std::getline(input,row);output<<row<<'\n';input>>count;std::getline(input,row);output<<count<<'\n';bool found=false;
    for(size_t i=0;i<count;++i){std::getline(input,row);auto fields=hostFields(row);if(std::stoi(fields[0])==id){edit(fields);hostRow(output,fields,fields.size(),6);found=true;}else output<<row<<'\n';}
    check(found,"private saved layer exists for malformed test");output<<input.rdbuf();auto bytes=output.str();document.hostExtra.assign(bytes.begin(),bytes.end());return document;
}
static void parentedWaveSavePhaseAndDetach(){
    Program p;p.image(1000,"fake",18,{{870,100,889,800,0,1}});p.alpha(18,0);p.image(1000,"fake",19,{},18);p.blend(19,19);p.wave(19,6);p.waitWave(19);p.dialogue(0);
    p.push(18);p.op(0x241);p.dialogue(1);p.push(19);p.op(0x241);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto pending=f.command("action","start");check(waiting(pending,"transitionReady")&&!hasWave(pending,19),"actual parented wave6 remains pending until graphics ready");
    auto state=acknowledge(f,pending);check(waiting(state,"input")&&number(layerObject(state,18),"alpha")==0&&number(layerObject(state,19),"parent")==18&&number(layerObject(state,19),"blend")==19,"child19 retains native parent/blend while parent global alpha is0");
    f.command("tick","1000");state=f.command("tick","1250");check(number(waveObject(state,19),"elapsedMs")==266&&number(state,"nextTickMs")==16&&boolean(state,"canSave"),"parent alpha0 does not freeze visible child wave phase or steady save");
    auto child=layerObject(state,19);check(number(child,"x")==1020&&std::abs(number(child,"y")-249.264701)<1e-5,"parent window offset is emitted once on child source geometry");
    f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");auto fields=savedLayerFields(good,19);check(fields.size()==31&&std::stod(fields[25])==18&&good.layers[1].x==150,"WH13 saves canonical child local geometry and parent handle without baking output offset");
    auto visible=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==visible&&boolean(state,"canSave"),"fresh Runtime restores parented wave6 exact state without replay");
    state=f.command("tick","900000");check(number(waveObject(state,19),"elapsedMs")==282,"loaded child phase advances one frame despite hidden parent");
    state=f.command("advance");check(number(layerObject(state,19),"parent")==-1&&hasWave(state,19)&&number(waveObject(state,19),"elapsedMs")==282,"parent deletion detaches child but preserves child wave state");
    geometryNear(state,19,150,150,100,100,"detach drops parent output offset without changing saved local geometry");state=f.command("tick","900016");check(number(waveObject(state,19),"elapsedMs")==298,"detached wave continues as standalone");
    state=f.command("advance");check(state.find("\"wave\":")==std::string::npos&&number(state,"nextTickMs")==0,"child deletion removes its wave and dynamic tick");
    state=f.command("load","2");check(visibleLayers(state)==visible,"load after deletion recreates parented wave from the same WH13 slot");state=f.command("title");check(state.find("\"wave\":")==std::string::npos,"title clears restored parented wave");f.passed=true;
}
static void parentedWaveRestoreRejectsUnsupported(){
    Program p;p.image(1000,"fake",18,{{0,0,1920,1080,0,1}});p.image(1000,"fake",19,{},18);p.blend(19,19);p.wave(19,6);p.waitWave(19);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));f.command("tick","1000");state=f.command("tick","1250");f.command("save","2");const auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");const auto live=visibleLayers(state);
    std::vector<mahoyo::save::Document> invalid;
    invalid.push_back(editSavedLayer(good,18,[](auto& row){row[21]="0";}));
    invalid.push_back(editSavedLayer(good,18,[](auto& row){row[25]="19";}));
    invalid.push_back(editSavedLayer(good,19,[](auto& row){row[21]="100";row[22]="100";}));
    invalid.push_back(editSavedLayer(good,19,[](auto& row){row[25]="63";}));
    invalid.push_back(editSavedLayer(good,19,[](auto& row){row[13]="13";}));
    invalid.push_back(editSavedLayer(good,19,[](auto& row){row[13]="0";}));
    invalid.push_back(editSavedLayer(good,19,[](auto& row){row[14]="1";}));
    {auto bad=good;std::string bytes(bad.hostExtra.begin(),bad.hostExtra.end());const std::string from="WAVES1 1\n19 6 ";auto at=bytes.find(from);check(at!=std::string::npos,"parented-wave footer exists");bytes.replace(at,from.size(),"WAVES1 1\n19 2 ");bad.hostExtra.assign(bytes.begin(),bytes.end());invalid.push_back(std::move(bad));}
    for(int kind=0;kind<4;++kind){auto bad=good;for(auto& l:bad.layers)if(l.id==(kind%2?18:19)){if(kind<2){l.hasCrop=true;l.crop={0,0,100,100};}else l.kind="ui";}invalid.push_back(std::move(bad));}
    for(const auto& bad:invalid){mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==live,"malformed parent/window/crop/UI/child blend restore preserves current VM and wave before mutation");}
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);state=f.command("load","2");check(visibleLayers(state)==live,"last good parented-wave slot remains loadable after rejected records");f.passed=true;
}
static void parentedWaveKnownBlendAndContextGates(){
    for(int blend:{0,1,11,15,16,17,19,22,24}){Program p;p.image(1000,"fake",18,{{0,0,1920,1080,0,1}});p.image(1000,"fake",19,{},18);p.blend(19,blend);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=f.command("action","start");check(number(layerObject(state,19),"blend")==blend&&!hasWave(state,19),"ordinary reviewed child blends remain supported independently of the narrower wave gate");f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());auto loaded=f.command("load","2");check(visibleLayers(loaded)==visibleLayers(state),"all reviewed ordinary child blends preserve their graph across fresh Runtime load");f.passed=true;
    }
    std::vector<std::array<int,3>> unreviewed;
    for(int preset:{2,3,4,5,7,8,10,11})unreviewed.push_back({preset,19,0});
    for(int blend:{0,1,11,15,16,17,22,24})unreviewed.push_back({6,blend,0});
    unreviewed.push_back({6,19,1});
    for(const auto& args:unreviewed){Program p;p.image(1000,"fake",18,{{0,0,1920,1080,0,1}});p.image(1000,"fake",19,{},18);p.blend(19,args[1],args[2]);p.wave(19,args[0]);p.waitWave(19);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true);std::string state=woh_command(f.handle,"action","start");check(waiting(state,"transitionReady"),"unreviewed child wave still waits for graphics-ready before commit validation");auto tail=state.substr(state.find("\"transition\":"));state=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(tail,"id"))).c_str());check(state.find("\"error\":\"Unsupported parented wave:")!=std::string::npos,"C++ cannot pass a parented preset/blend/flag that the current GPU path rejects");f.passed=true;
    }
    for(int preset:{2,3,4,5,6,7,8}){Program p;p.image(1000,"fake",19);p.blend(19,19);p.wave(19,preset);p.waitWave(19);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));check(number(waveObject(state,19),"preset")==preset&&number(layerObject(state,19),"parent")==-1,"the narrow child-wave gate does not restrict existing standalone2..8 blend19");if(preset==7||preset==8){auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"WH13 admits reviewed root presets7/8 while malformed9 stays rejected");}f.passed=true;}
    for(int kind=0;kind<7;++kind){Program p;if(kind==2)p.image(1000,"fake",17,{{0,0,1920,1080,0,1}});p.image(1000,"fake",18,kind==1?std::array<int32_t,6>{}:std::array<int32_t,6>{{0,0,1920,1080,0,1}},kind==2?17:-1);
        p.image(1000,kind==4?"BLACK":"fake",19,kind==0?std::array<int32_t,6>{{0,0,1920,1080,0,1}}:std::array<int32_t,6>{},18);p.blend(19,kind>=5?(kind==5?15:17):19,kind>=5?1:-1);if(kind==3)p.image(1000,"fake",20,{},19);p.wave(19,6);p.waitWave(19);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true);std::string rejected=woh_command(f.handle,"action","start");if(waiting(rejected,"transitionReady")){auto tail=rejected.substr(rejected.find("\"transition\":"));rejected=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(tail,"id"))).c_str());}check(rejected.find("\"error\":\"\"")==std::string::npos,"own window/no native parent window/nesting/solid/special flag1 remain explicit unsupported");f.passed=true;
    }
}
static void waveElevenNativeAndLazyClock(){
    Program p;p.image(1000,"img0608",21);p.blend(21,14,0);for(int n:{0,52,21})p.push(n);p.op(0x253);p.wave(21,11);p.waitWave(21);p.dialogue(0);p.alpha(21,0);p.dialogue(1);p.alpha(21,255);p.clone(22,21,"img0608");p.dialogue(0);p.wave(21,11);for(int n:{0,256,0,0,500,0})p.push(n);p.op(0x200);p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),false,"","img0608 100 100\n");f.imageHeader("img0608",2552,1272);f.command("textSpeed","0");woh_wave_clock(f.handle,1000);auto state=f.command("action","start");woh_wave_clock(f.handle,2000);state=acknowledge(f,state);check(number(waveObject(state,21),"preset")==11&&number(waveObject(state,21),"elapsedMs")==0&&number(layerObject(state,21),"blend")==14&&number(layerObject(state,21),"colorMode")==52,"actual5B1 root11/14flag0/color52 starts25cell image atphase0");
    woh_wave_clock(f.handle,2016);state=f.command("tick","2016");check(number(waveObject(state,21),"elapsedMs")==16,"externalandlogical clocks never doublecount");state=f.command("advance");check(number(layerObject(state,21),"alpha")==0&&number(state,"nextTickMs")==0,"hidden11 keeps timer idle");auto version=number(state,"version"),instructions=number(state,"instructions");woh_wave_clock(f.handle,2266);state=woh_snapshot(f.handle);check(number(waveObject(state,21),"elapsedMs")==266&&number(state,"version")==version&&number(state,"instructions")==instructions&&number(state,"nextTickMs")==0,"hidden250ms is accounted lazily without VM/version/timer work");
    woh_wave_clock(f.handle,2266);woh_wave_clock(f.handle,2200);state=f.command("advance");check(number(waveObject(state,21),"elapsedMs")==266&&!hasWave(state,22)&&number(layerObject(state,21),"alpha")==1,"duplicate/backwardtimestamps cannot add debt;visible resume andclone preserve exactphase");
    woh_wave_clock(f.handle,2300);f.command("pause");woh_wave_clock(f.handle,9900000);state=woh_snapshot(f.handle);check(number(waveObject(state,21),"elapsedMs")==300,"background pause excludes walltime");woh_wave_clock(f.handle,9900010);f.command("resume");woh_wave_clock(f.handle,9900026);state=woh_snapshot(f.handle);check(number(waveObject(state,21),"elapsedMs")==316,"resume advances only16ms after rebase");
    f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");auto visible=visibleLayers(state);check(currentHostSchema(good),"11 uses currentWAVES1 host schema");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");woh_wave_clock(f.handle,100000000);state=f.command("load","2");check(visibleLayers(state)==visible,"freshload preserves11 phase without externalabsoluteclock");woh_wave_clock(f.handle,100001000);state=woh_snapshot(f.handle);check(number(waveObject(state,21),"elapsedMs")==316,"firstpostload clock has no inheriteddebt");woh_wave_clock(f.handle,100001016);state=woh_snapshot(f.handle);check(number(waveObject(state,21),"elapsedMs")==332,"postload next16ms resumesphase");
    woh_wave_clock(f.handle,100001032);f.command("history");woh_wave_clock(f.handle,200000000);state=woh_snapshot(f.handle);check(number(waveObject(state,21),"elapsedMs")==348,"history excludes idlewalltime");woh_wave_clock(f.handle,200000010);f.command("history");woh_wave_clock(f.handle,200000026);state=f.command("advance");check(waiting(state,"transitionReady"),"11 replacement waits forgraphics");auto old=state.substr(state.find("\"old\":{\"scene\""));auto captured=waveObject(old,21),currentBeforeBarrier=waveObject(state,21);woh_wave_clock(f.handle,300000000);state=woh_snapshot(f.handle);old=state.substr(state.find("\"old\":{\"scene\""));check(waveObject(state,21)==currentBeforeBarrier&&waveObject(old,21)==captured,"assetbarrier freezescurrentwaveandlastpresentedoldcapture independently");state=acknowledge(f,state);check(number(waveObject(state,21),"elapsedMs")==0,"replacement11 starts its ownphase");woh_wave_clock(f.handle,300000016);state=woh_snapshot(f.handle);old=state.substr(state.find("\"old\":{\"scene\""));check(number(waveObject(state,21),"elapsedMs")==16&&waveObject(old,21)==captured,"oldtransition remains immutable while externalnew11phase advances");f.passed=true;
    woh_wave_clock(0,1);woh_wave_clock(UINT64_MAX,1); //Invalid handles are safe no-ops.
}
static void waveTenVariantCaptureCloneAndRestore(){
    Program p;p.image(1000,"img0608",24);for(int n:{2,2,24})p.push(n);p.op(0x256);p.blend(24,17,1);p.wave(24,10);p.waitWave(24);p.dialogue(0);
    p.alpha(24,0);p.dialogue(1);p.alpha(24,255);p.clone(25,24,"img0608");p.dialogue(0);
    p.wave(24,10);for(int n:{0,256,0,0,500,0})p.push(n);p.op(0x200);p.dialogue(1);p.wave(24,0,500);p.waitWave(24);p.dialogue(0);p.op(0xf4);
    //Actual variant dimensions and binding, synthetic origin only; no pixels.
    Fixture f(std::move(p),false,"","img0608 100 100\n");f.imageHeader("img0608",2552,1272);f.imageHeader("img0608_02_02",2552,1526);f.command("textSpeed","0");
    auto state=acknowledge(f,f.command("action","start"));check(layerObject(state,24).find("img0608_02_02.mzp")!=std::string::npos&&number(waveObject(state,24),"preset")==10,"5B1 resolved variant uses actual2552x1526/240cell root10");
    f.command("tick","1000");state=f.pulseTick("1250");check(number(waveObject(state,24),"elapsedMs")==266&&boolean(state,"canSave"),"persistent10 uses same bounded logical clock");auto phase=waveObject(state,24);f.command("pause");state=f.command("tick","1000000");check(waveObject(state,24)==phase,"paused10 phase remains exact");f.command("resume");state=f.command("tick","1000000");check(number(waveObject(state,24),"elapsedMs")==282,"10 resume rebases wallclock");
    const auto visible=visibleLayers(state);f.command("save","2");const auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(good.hostExtra.begin(),good.hostExtra.end());check(currentHostSchema(good)&&extra.find("WAVES1 1\n24 10 0 0 0 1 282\n")!=std::string::npos,"10 reuses WAVES1 fields without schema extension");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==visible,"fresh Runtime restores10 resolved variant and exact phase");
    mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",host16Fixture(good));woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==visible,"WHS16 existing WAVES1 preset10 restores into current Runtime without footer changes");
    auto bad=editSavedLayer(good,24,[](auto& row){row[25]="25";});mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);auto rejected=std::string(woh_command(f.handle,"load","2"));check(rejected.find("Unsupported parented wave")!=std::string::npos&&visibleLayers(rejected)==visible,"saved10 cannot enter through child binding and failure preserves live state");mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);state=f.command("load","2");
    state=f.command("advance");auto hiddenPhase=number(waveObject(state,24),"elapsedMs");check(number(layerObject(state,24),"alpha")==0&&number(state,"nextTickMs")==0,"10 alpha0 suspends dynamic scheduling");state=f.command("tick","2000000");check(number(waveObject(state,24),"elapsedMs")==hiddenPhase,"alpha0 sampling rebases without advancing10");state=f.command("advance");check(hasWave(state,24)&&!hasWave(state,25)&&number(waveObject(state,24),"elapsedMs")==hiddenPhase,"242 clone copies variant geometry without copying wave10 job");
    state=f.command("advance");auto old=state.substr(state.find("\"old\":{\"scene\""));auto captured=waveObject(old,24);check(number(captured,"preset")==10,"old scene captures10 before replacement");state=acknowledge(f,state);f.command("tick","3000000");state=f.pulseTick("3000234");old=state.substr(state.find("\"old\":{\"scene\""));check(waveObject(old,24)==captured&&number(waveObject(state,24),"elapsedMs")==250,"captured10 phase stays frozen while new10 RT animates");state=f.pulseTick("3000484");check(waiting(state,"input"),"10 replacement scene reaches input after500ms");
    state=acknowledge(f,f.command("advance"));check(waiting(state,"animation")&&!boolean(state,"canSave"),"10 stop fade waits and prevents unsafe save");f.command("tick","4000000");state=f.pulseTick("4000234");check(number(waveObject(state,24),"strength")==.5,"10 stop strength halfway");state=f.pulseTick("4000484");check(waiting(state,"input")&&!hasWave(state,24)&&number(state,"nextTickMs")==0,"10 stop removes wave and polling at500ms");
    //Fresh restore must enforce intrinsic bitmap cells, not saved display scale.
    f.imageHeader("img0608_02_02",12800,1280);mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());auto tooLarge=std::string(woh_command(f.handle,"load","2"));check(tooLarge.find("Unsupported wave10 bitmap")!=std::string::npos&&tooLarge.find("\"notice\":\"Loaded\"")==std::string::npos,"fresh restore rejects a1000cell10 bitmap before installing saved scene");f.imageHeader("img0608_02_02",2552,1526);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==visible,"last good240cell10 checkpoint still restores after rejected dimensions");f.passed=true;
    for(auto dims:std::vector<std::array<int,2>>{{13824,1184},{12800,1280},{16385,32}}){Program grid;grid.image(1,"grid",24);grid.wave(24,10);grid.commit();grid.dialogue(0);Fixture g(std::move(grid));g.imageHeader("grid",dims[0],dims[1]);auto s=g.command("action","start");auto t=s.substr(s.find("\"transition\":"));s=woh_command(g.handle,"transitionReady",std::to_string(uint64_t(number(t,"id"))).c_str());if(dims[0]==13824)check(hasWave(s,24)&&s.find("\"error\":\"\"")!=std::string::npos,"999cell10 accepted independent of tiny display scale");else check(s.find("Unsupported wave10 bitmap")!=std::string::npos,"1000cell fallback or extent>16384 remains rejected");g.passed=true;}
}
static void nativeChild24PropertyGate(){
    for(int flag:{0,1}){
        Program p;p.image(1000,"fake",31,{{0,0,1920,1080,0,1}});p.image(1000,"fake",34,{},31);p.blend(34,0,flag);for(int n:{0,0,0,915,34})p.push(n);p.op(0x278);p.dialogue(0);p.op(0xf4);
        const std::string table="915 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n915 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n";
        const std::string windows="915 0 0 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n";
        Fixture f(std::move(p),true,table,"","",windows);woh_destroy(f.handle);auto path=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(path);{std::ofstream out(path);for(int i=0;i<2;++i){out<<"915 "<<i<<' '<<i<<" 0x2000000";for(int n=0;n<13;++n)out<<' '<<(n==2?24:-32767);out<<'\n';}}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));
        if(flag)check(state.find("Unsupported effect blend mode 24")!=std::string::npos,"effect properties cannot bypass the child24flag0 boundary");
        else{check(number(layerObject(state,34),"blend")==24,"effect blend property uses the same reviewed child24 route");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"fresh WH14 active-property restore retains child24flag0");}f.passed=true;
    }
}
static void nativeChild24FlagRestoreAndCapture(){
    for(int raw:{-1,0,2}){
        Program p;p.image(1000,"fake",30,{{0,0,1920,1080,0,1}});p.image(1000,"fake",31,{{0,0,1920,1080,0,1}});p.image(1000,"fake",34,{},31);p.image(1000,"fake",35);p.alpha(31,64);p.alpha(34,128);p.blend(34,24,raw);p.commit();p.dialogue(0);p.clone(36,34,"fake");p.dialogue(1);p.blend(34,0);for(int n:{0,256,0,0,300,0})p.push(n);p.op(0x200);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));if(waiting(state,"frame"))state=f.command("tick","1000");auto child=layerObject(state,34);
        check(number(child,"parent")==31&&number(child,"blend")==24&&number(child,"blendFlag")==0&&std::abs(number(child,"alpha")-128/255.)<1e-8,"actual child24 keeps its normalized flag0, fractional opacity and native parent graph");
        auto before=visibleLayers(state);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"WH14 fresh instance restores ordinary child24 without changing group geometry");
        std::vector<mahoyo::save::Document> invalid;
        invalid.push_back(editSavedLayer(good,34,[](auto& row){row[14]="1";}));
        invalid.push_back(editSavedLayer(good,34,[](auto& row){row[21]="1920";row[22]="1080";}));
        invalid.push_back(editSavedLayer(good,31,[](auto& row){row[21]="0";row[22]="0";}));
        invalid.push_back(editSavedLayer(good,31,[](auto& row){row[25]="30";}));
        invalid.push_back(editSavedLayer(good,35,[](auto& row){row[25]="34";}));
        for(const auto& bad:invalid){mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==before,"unsupported child24 flag/window/nested context is rejected before replacing the live scene");}
        mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);f.command("load","2");state=f.command("advance");check(number(layerObject(state,36),"blend")==24&&number(layerObject(state,36),"parent")==31,"clone retains an ordinary reviewed child24 source without widening child-wave support");
        state=f.command("advance");check(waiting(state,"transitionReady")&&state.find("\"old\":{\"scene\"")!=std::string::npos,"child24 capture fixture reaches its native transition barrier");auto old=state.substr(state.find("\"old\":{\"scene\""));check(number(layerObject(state,34),"blend")==0&&number(layerObject(old,34),"blend")==24,"transition capture retains the old child24 group while the new scene changes its blend");f.passed=true;
    }
    Program invalid;invalid.image(1000,"fake",31,{{0,0,1920,1080,0,1}});invalid.image(1000,"fake",34,{},31);invalid.blend(34,24,1);invalid.dialogue(0);Fixture bad(std::move(invalid),true);auto rejected=std::string(woh_command(bad.handle,"action","start"));check(rejected.find("Unsupported native window child blend mode/flag")!=std::string::npos,"host252 cannot pass unreviewed child24flag1 to the GPU");bad.passed=true;
    Program root;root.image(1000,"fake",34);root.blend(34,24,1);root.dialogue(0);root.op(0xf4);Fixture ordinary(std::move(root),true);auto state=ordinary.command("action","start");check(number(layerObject(state,34),"blendFlag")==1,"new child24 flag gate does not change existing root24flag1 behavior");ordinary.passed=true;
}
static void cloneEffectFullMapBoundary(){
    for(int clip:{915,916,3096,8372}){Program p;p.image();for(int n:{0,0,0,clip,0})p.push(n);p.op(0x278);p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);p.op(0xf4);
        const std::string table=std::to_string(clip)+" 0 -32767 -32767 -32767 1000 1000 -100001 0 0 0 1000\n"+std::to_string(clip)+" 1000 -32767 -32767 -32767 2000 500 -100001 0 0 0 1000\n";
        Fixture f(std::move(p),true,table,"fake 100 100\n");f.command("textSpeed","0");std::string state=woh_command(f.handle,"action","start");state=f.command("advance");check(state.find("\"error\":\"\"")!=std::string::npos,"full native map admits ordinary and special-time clone terminal samples");f.passed=true;
    }
}
static void specialTimeFreshRestoreCloneAndCompletion(){
    Program p;p.image();for(int n:{0,0,0,3120,0})p.push(n);p.op(0x278);p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);p.op(0xf4);
    const std::string table="3120 0 -32767 -32767 0 1000 1000 -100001 4 0 0 4000\n3120 4000 -32767 -32767 255 2000 1000 -100001 -32767 0 0 4000\n";
    Fixture f(std::move(p),true,table,"fake 100 100\n");f.command("textSpeed","0");f.command("action","start");f.command("tick","1000");auto state=f.command("tick","1984");
    check(number(layerObject(state,0),"alpha")==.125&&number(layerObject(state,0),"w")==112.5,"3120 raw1000 maps to500 once; alpha and scale share the same linear segment time");
    auto before=visibleLayers(state);f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");std::string extra(saved.hostExtra.begin(),saved.hostExtra.end());check(extra.find("\n0 3120 1000 ")!=std::string::npos,"WH13 stores raw phase1000 rather than mapped500");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh Runtime restores special-time state without a second remap");
    state=f.command("advance");check(number(layerObject(state,1),"w")==200&&number(layerObject(state,1),"alpha")==1&&number(layerObject(state,0),"w")==112.5,"clone-1 samples special terminal aggregate without altering its live owner");
    f.command("tick","3000");state=f.command("tick","5984");check(number(layerObject(state,0),"alpha")==1&&number(state,"nextTickMs")==16,"special job remains active at exactlyduration, as native done is strict greater-than");
    state=f.command("tick","5985");check(number(state,"nextTickMs")==0,"first tick afterduration retires a completed special job instead of resurrecting it");f.passed=true;
    Program loop;loop.image();for(int n:{0,0,0,6673,0})loop.push(n);loop.op(0x278);loop.dialogue(0);loop.clone(1,0,"fake");loop.dialogue(1);loop.op(0xf4);
    const std::string cyclic="6673 0 -32767 -32767 0 1000 1000 -100001 6 0 1 1200\n6673 1200 -32767 -32767 255 2000 1000 -100001 -32767 0 1 1200\n";
    Fixture l(std::move(loop),true,cyclic,"fake 100 100\n");l.command("textSpeed","0");l.command("action","start");l.command("tick","1000");state=l.command("tick","2185");check(number(layerObject(state,0),"alpha")==0,"6673 raw1201 wraps phase2 then maps0 without loop-minimum correction");l.command("save","2");saved=mahoyo::save::read(l.root/"savedata/slot2.whs");extra.assign(saved.hostExtra.begin(),saved.hostExtra.end());check(extra.find("\n0 6673 2 ")!=std::string::npos,"loop save stores pre-remap normalized phase2");
    before=visibleLayers(state);woh_destroy(l.handle);l.handle=woh_create(l.game.string().c_str());l.command("textSpeed","0");state=l.command("load","2");check(visibleLayers(state)==before,"fresh loop restore retains mapped0 even when its raw phase is above loopStart");state=l.command("advance");check(number(layerObject(state,1),"alpha")==0&&number(layerObject(state,1),"w")==100,"looping special clone-1 uses duration+1 then global warp, not the last key");l.passed=true;
    Program waited;waited.image();for(int n:{0,0,0,3120,0})waited.push(n);waited.op(0x278);for(int n:{1,32,0})waited.push(n);waited.op(0x27f);waited.dialogue(0);waited.op(0xf4);
    Fixture w(std::move(waited),true,table);w.command("textSpeed","0");state=acknowledge(w,w.command("action","start"));w.command("tick","1000");state=w.command("tick","4984");check(waiting(state,"animation")&&number(layerObject(state,0),"alpha")==1,"27f waits for an active special job at exactlyduration despite its endpoint pixels");state=w.command("tick","4985");check(waiting(state,"input")&&number(state,"nextTickMs")==0,"27f releases when the special nonloop job is done atduration+1");w.passed=true;
}
static std::string noiseObject(const std::string& state,int id){auto layer=layerObject(state,id);auto at=layer.find("\"noise\":{");check(at!=std::string::npos,"expected noise descriptor exists");return layer.substr(at+8);}
static std::string noiseToken(const std::string& state,int id){auto value=noiseObject(state,id);auto start=value.find("\"token\":\"")+9;auto end=value.find('"',start);return value.substr(start,end-start);}
static bool hasNoise(const std::string& state,int id){return layerObject(state,id).find("\"noise\":{")!=std::string::npos;}
static void prepareNoiseImages(Fixture& f){f.imageHeader("noise0",1920,1080);f.imageHeader("noise1",1920,1080);}
static void nativeNoiseController(){
    using namespace mahoyo::noise;Controller c;auto one=*decode(1);check(one==*decode(31)&&decode(11)->periodMs==150&&decode(21)->periodMs==100&&!decode(0),"native effective pair differs from raw decimal enum");
    check(c.reconcile({{0,one}}),"initial config creates a renderer token");auto token=c.at(0).token;
    check(!c.reconcile({{0,*decode(31)}})&&c.at(0).token==token,"same effective resource/period elides setter and retains token");
    c.reconcile({{0,*decode(11)},{1,*decode(22)}});check(c.minimumPeriod()==100&&c.at(0).token!=token,"period/resource changes create tokens and choose minimum wake");
    auto old=c.at(0).token;bool rejected=false;try{c.reconcile({{0,{1,50}},{64,{1,50}}});}catch(const std::exception&){rejected=true;}check(rejected&&c.at(0).token==old,"invalid candidate cannot partly replace live noise states");
    c.reset();c.reconcile({{0,one}});check(c.at(0).token!=token,"restore/new object cannot collide with an old token in this process");
    for(int raw:{-1,9,19}){rejected=false;try{decode(raw);}catch(const std::exception&){rejected=true;}check(rejected,"unknown noise selectors fail explicitly");}
}
static void noisePrepareIdempotencyWakeCloneAndClear(){
    Program p;p.image();p.noise(0,1);p.dialogue(0);p.noise(0,31);p.dialogue(1);p.noise(0,11);p.dialogue(0);p.noise(0,0);p.dialogue(1);p.noise(0,1);p.dialogue(0);p.clone(1,0,"fake");p.dialogue(1);p.push(0);p.op(0x241);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true,"","fake 100 100\n");prepareNoiseImages(f);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady")&&number(noiseObject(state,0),"clockMs")==0&&number(state,"nextTickMs")==0,"initial noise uses existing GPU-ready barrier without CPU phase simulation");
    auto token=noiseToken(state,0);f.command("tick","999999");state=acknowledge(f,state);check(number(state,"nextTickMs")==50,"noise-only idle wakes at native50ms rather than16ms");state=f.command("tick","1000");check(number(noiseObject(state,0),"clockMs")==50,"first noise-only wake consumes its requested logical50ms");state=f.command("tick","1050");check(number(noiseObject(state,0),"clockMs")==100&&noiseToken(state,0)==token,"later wake exports a draw clock without advancing GL phase in C++");
    auto repeated=std::string(woh_snapshot(f.handle));check(noiseObject(repeated,0)==noiseObject(state,0),"snapshot polling never changes the noise token or clock");state=f.command("advance");check(waiting(state,"input")&&noiseToken(state,0)==token&&number(layerObject(state,0),"decimalMode")==31,"same effective pair skips reset/extra barrier but keeps raw31");
    state=f.command("advance");check(waiting(state,"transitionReady")&&noiseToken(state,0)!=token&&number(noiseObject(state,0),"periodMs")==150,"new period creates a new token before preparation");auto second=noiseToken(state,0);state=acknowledge(f,state);check(number(state,"nextTickMs")==150,"noise-only150ms uses its actual period");state=f.command("tick","9000");check(number(noiseObject(state,0),"clockMs")==250,"prepare loading gap is excluded from the logical draw clock");
    state=acknowledge(f,f.command("advance"));check(!hasNoise(state,0)&&number(state,"nextTickMs")==0,"cleared noise returns to true idle0");state=acknowledge(f,f.command("advance"));check(noiseToken(state,0)!=second,"clear then reenable creates a fresh Native clock token");token=noiseToken(state,0);state=f.command("advance");check(noiseToken(state,0)==token&&!hasNoise(state,1)&&number(layerObject(state,1),"decimalMode")==0,"242 resets its target noise instead of copying the source cache/config");state=f.command("advance");check(number(state,"nextTickMs")==0&&!hasNoise(state,1),"deleting the last noise owner stops polling and keeps static clone");f.passed=true;
}
static void noiseFreshRestorePauseHistoryAndSameBatch(){
    Program p;p.image();p.noise(0,1);p.dialogue(0);p.image();p.position(0,70,50);p.noise(0,1);p.dialogue(1);p.image();p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),true);prepareNoiseImages(f);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));state=f.command("tick","1000");auto token=noiseToken(state,0);auto clock=number(noiseObject(state,0),"clockMs");
    f.command("pause");state=f.command("tick","999999");check(number(state,"nextTickMs")==0&&number(noiseObject(state,0),"clockMs")==clock,"pause freezes logical noise time and stops wakeups");state=f.command("resume");check(number(state,"nextTickMs")==50,"resume restores period wake");state=f.command("tick","999999");clock+=50;check(number(noiseObject(state,0),"clockMs")==clock,"resume excludes wall-time gap");f.command("history");state=f.command("tick","2000000");check(number(state,"nextTickMs")==0&&number(noiseObject(state,0),"clockMs")==clock,"history also freezes noise clock even if manually polled");f.command("history");state=f.command("tick","2000000");check(number(noiseObject(state,0),"clockMs")==clock+50,"history return consumes only one requested wake");
    f.command("save","2");auto doc=mahoyo::save::read(f.root/"savedata/slot2.whs");auto fields=savedLayerFields(doc,0);check(currentHostSchema(doc)&&fields[15]=="1"&&std::string(doc.hostExtra.begin(),doc.hostExtra.end()).find("noise-")==std::string::npos,"WH13 stores raw decimal only, not transient token/phase/cache");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(noiseToken(state,0)!=token&&number(noiseObject(state,0),"clockMs")==0&&number(state,"nextTickMs")==50,"fresh Runtime restores raw config with a new token/clock policy");token=noiseToken(state,0);state=f.command("advance");check(waiting(state,"input")&&noiseToken(state,0)==token&&number(layerObject(state,0),"decimalMode")==1,"same-batch240 reset plus257 samepair preserves an existing target cache");state=acknowledge(f,f.command("advance"));check(!hasNoise(state,0)&&number(state,"nextTickMs")==0,"240 alone resets pending noise to0");f.command("title");check(number(std::string(woh_snapshot(f.handle)),"nextTickMs")==0,"title has no retained noise timer");f.passed=true;
}
static void noiseAlphaDisabledFastWorkAndGuards(){
    Program p;p.image();p.alpha(0,0);p.push(0);p.push(0);p.op(0x25a);p.noise(0,21);p.dialogue(0);p.dialogue(1);p.push(2);p.op(0x13c);p.op(0xf4);
    Fixture f(std::move(p),true);prepareNoiseImages(f);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));check(number(layerObject(state,0),"alpha")==0&&hasNoise(state,0)&&number(state,"nextTickMs")==100,"alpha0/enabled0 retains native prepass and100ms wake");state=f.command("tick","1000");check(number(noiseObject(state,0),"clockMs")==100,"invisible registered source still receives logical time");f.command("textSpeed","40");state=f.command("advance");check(number(state,"nextTickMs")==16,"text reveal keeps existing16ms scheduling over slow noise wake");f.command("advance");state=f.command("advance");check(modalPhase(state,"opening"),"noise may remain behind TEATIME");f.command("tick","1000");state=f.command("tick","2000");check(modalPhase(state,"ready")&&number(state,"nextTickMs")==100,"steady intermission background retains only its native noise period wake");f.passed=true;
    for(int sub:{22,38,39}){Program entry;entry.string("c_9.chs");entry.op(0xf3);Fixture c(std::move(entry),true);prepareNoiseImages(c);Program module;module.push(sub);module.op(0x12a);module.image();module.noise(0,1);module.dialogue(0);module.op(0xf4);auto path=c.game/"scripts/c_9.chs";c.extraFiles.push_back(path);{auto data=module.build();std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));}c.command("textSpeed","0");std::string value=woh_command(c.handle,"action","start");if(sub==22)check(waiting(value,"transitionReady")&&value.find("\"error\":\"\"")!=std::string::npos,"actualsub22 is not incorrectly treated as chapter22");else check(value.find("Unreviewed C9 noise combination")!=std::string::npos,"exactchapter120 c_9/sub38 or39 fails rather than silently using default shader");c.passed=true;}
    for(int kind=0;kind<3;++kind){Program bad;bad.image(1000,"fake",0,kind==1?std::array<int32_t,6>{{0,0,1920,1080,0,0}}:std::array<int32_t,6>{});bad.noise(0,kind==0?9:1);if(kind==2)bad.wave(0,2);bad.commit();bad.dialogue(0);Fixture rejected(std::move(bad),true);prepareNoiseImages(rejected);auto value=std::string(woh_command(rejected.handle,"action","start"));check(value.find("Unsupported native noise")!=std::string::npos,"unreviewed selector/window/wave context remains an explicit stop");rejected.passed=true;}
}
static void noiseCallbackGridBoundariesAndPop(){
    struct Case{const char* module;int sub;const char* image;bool rejected;};
    const Case cases[]={{"9_3.chs",9,"img0408",false},{"9_3.chs",10,"img0408",true},{"9_3.chs",18,"img0408",true},{"9_3.chs",19,"img0408",false},{"9_3.chs",10,"img0408x",false},{"9_6.chs",0,"img0408",true},{"9_6.chs",1,"img0408",false},{"a_0.chs",33,"aok_n_06_02_07_40_00",true},{"a_0.chs",32,"aok_n_06_02_07_40_00",false}};
    for(const auto& item:cases){Program entry;entry.string(item.module);entry.op(0xf3);Fixture f(std::move(entry),true);prepareNoiseImages(f);f.imageHeader(item.image,100,100);Program body;body.push(item.sub);body.op(0x12a);body.image(1000,item.image);body.noise(0,1);body.dialogue(0);body.op(0xf4);auto path=f.game/"scripts"/item.module;f.extraFiles.push_back(path);{auto data=body.build();std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));}f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));if(item.rejected)check(state.find(std::string(item.module)=="9_3.chs"?"Unsupported noise78 Native context":std::string(item.module)=="9_6.chs"?"Unsupported noise82 Native context":"Unreviewed Native A0 final28 context")!=std::string::npos,"unreviewed callback material remains an explicit unsupported boundary");else check(waiting(state,"transitionReady")&&state.find("\"error\":\"\"")!=std::string::npos,"adjacent sub/name predicates retain default noise path");f.passed=true;}
    Program p;p.push(777);p.noise(-1,9);p.noise(64,1);p.push(0);p.op(0xe0);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");f.command("save","2");InspectHost host;mahoyo::vm::Machine vm(host);vm.loadState(mahoyo::save::read(f.root/"savedata/slot2.whs").vm);check(vm.variable(0)==777&&number(state,"nextTickMs")==0,"out-of-range257 consumes exactlytwo arguments without creating noise/layers");f.passed=true;
}
static constexpr const char* clockLayout="img1242 2187 1691\nimg1244 83 408\nimg1246 75 531\n";
static void prepareClockImages(Fixture& f,bool omitShadow=false){
    f.imageHeader("img1242",6632,5102);f.imageHeader("img1244",512,1272);f.imageHeader("img1246",512,1780);
    if(!omitShadow)f.imageHeader("img1244_02_02",512,1272);f.imageHeader("img1246_02_02",512,1780);
}
static void nativeClockCreationSaveAndDelete(){
    Program p;p.push(777);p.clock();p.push(0);p.op(0xe0);p.commit();p.dialogue(0);for(int id=2;id<=6;++id){p.push(id);p.op(0x241);}p.dialogue(1);p.op(0xf4);
    Fixture f(std::move(p),false,"",clockLayout);prepareClockImages(f);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"actual380 is staged into its following scene preparation");auto prepared=visibleLayers(state);state=f.command("tick","999999");check(visibleLayers(state)==prepared,"graphics loading cannot move the clock or execute its next page");
    state=acknowledge(f,state);state=f.command("tick","1000");check(waiting(state,"input")&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"static clock is idle and saveable after prepare");
    geometryNear(state,2,-1500,-1128,6632,5102,"clock face uses original PNG extent and native integer half logical origin");
    geometryNear(state,3,1653,237,512,1272,"hour hand uses explicit43/391 origin and native integer position");
    geometryNear(state,4,1668,261,512,1272,"hour shadow keeps scale-dependent offset and source storage dimensions");
    geometryNear(state,5,1671,-126,512,1780,"minute hand keeps independent38/514 pivot");
    geometryNear(state,6,1686,-102,512,1780,"minute shadow uses its own offset");
    const int priorities[]={100,103,101,104,102};for(int n=0;n<5;++n){auto l=layerObject(state,2+n);check(number(l,"z")==priorities[n]&&number(l,"parent")==-1&&number(l,"blend")==0,"clock priorities and ordinary root blending are native");check(number(l,"colorMode")==double(n==2||n==4?1:35),"only shadows force color1");}
    check(std::abs(number(layerObject(state,4),"alpha")-16448./65536)<1e-9&&layerObject(state,4).find("img1244_02_02.mzp")!=std::string::npos,"shadow variant2/2 preserves canonical fixed alpha rather than rounding64/255");
    check(number(layerObject(state,3),"rotation")==34.4970703&&number(layerObject(state,5),"rotation")==53.9978027,"native10:51 angle quantization is retained in output");
    auto original=visibleLayers(state);f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");InspectHost host;mahoyo::vm::Machine vm(host);vm.loadState(saved.vm);check(vm.variable(0)==777,"380 consumes seven words and pushes no value");
    check(currentHostSchema(saved),"settled clocks need no new save schema");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==original,"WH13 restores all five clock layers without replay");state=f.command("advance");check(visibleLayers(state)=="[]"&&number(state,"nextTickMs")==0,"actual241 sequence deletes all five clock parts without polling");f.passed=true;
}
static void nativeClockPendingRotationTiming(){
    Program p;p.clock();p.commit();p.dialogue(0);p.clockTime(2,10,52);for(int n:{1,4,-1})p.push(n);p.op(0x27f);p.dialogue(1);p.clockTime(2,10,51);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),false,"",clockLayout);prepareClockImages(f);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));state=f.command("tick","1000");auto before=visibleLayers(state);state=f.command("advance");check(waiting(state,"transitionReady")&&!boolean(state,"canSave"),"381 queues four rotations without starting before graphics ready");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"pending clock rotations cannot be saved without their prepare context");state=f.command("tick","999999");check(number(layerObject(state,3),"rotation")==number(layerObject(before,3),"rotation")&&number(layerObject(state,5),"rotation")==number(layerObject(before,5),"rotation"),"pending381 does not consume graphics-loading time");state=acknowledge(f,state);check(waiting(state,"animation"),"27fmask4 includes newly prepared clock rotations");
    f.command("tick","2000000");state=f.command("tick","2000084");check(number(layerObject(state,3),"rotation")==33.847229&&number(layerObject(state,5),"rotation")==46.199707,"100ms reaches native1.3 overshoot instead of cubic-in or shortest-angle");check(!boolean(state,"canSave"),"finite clock rotations keep save guard active");
    auto frozen=visibleLayers(state);f.command("pause");state=f.command("tick","9000000");check(visibleLayers(state)==frozen,"paused clock does not consume wall time");f.command("resume");f.command("tick","9000000");state=f.command("tick","9000183");check(waiting(state,"animation"),"clock wait remains at299ms after pause");state=f.command("tick","9000184");check(waiting(state,"input")&&number(layerObject(state,3),"rotation")==33.9971924&&boolean(state,"canSave"),"300ms settles exactly and releases27f once");
    f.command("save","2");auto settled=visibleLayers(state);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==settled,"WH13 retains settled updated hands and shadows");
    state=f.command("advance");check(waiting(state,"transitionReady"),"140 also prepares pending381 without a200");state=acknowledge(f,state);check(waiting(state,"input")&&!boolean(state,"canSave"),"clock update resumes input while its finite animation remains unsafe");f.passed=true;
}
static void nativeClockReplacementAndRemapSaveGuard(){
    Program p;p.clock();p.commit();p.dialogue(0);p.clockTime(2,10,52);p.dialogue(1);p.clockTime(2,10,51);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),false,"",clockLayout);prepareClockImages(f);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));f.command("tick","1000");state=acknowledge(f,f.command("advance"));f.command("tick","2000");state=f.command("tick","2084");check(number(layerObject(state,3),"rotation")==33.847229,"first update reaches the exact overshoot before replacement");
    state=f.command("advance");auto from=number(layerObject(state,3),"rotation");state=f.command("tick","900000");check(number(layerObject(state,3),"rotation")==from,"new NaN-from request cannot complete the old job during loading");state=acknowledge(f,state);check(number(layerObject(state,3),"rotation")==from,"new rotation captures current sampled value rather than old endpoint");f.command("tick","900000");state=f.command("tick","900284");check(number(layerObject(state,3),"rotation")==34.4970703&&boolean(state,"canSave"),"replacement owns one300ms timeline and reaches its new endpoint");f.passed=true;
    Program plain;plain.image();for(int n:{0,0,0,915,0})plain.push(n);plain.op(0x278);plain.dialogue(0);plain.op(0xf4);
    std::string tables;for(int clip:{915,3096})tables+=std::to_string(clip)+" 0 -32767 -32767 -32767 1000 1000 -100001 0 0 0 1000\n"+std::to_string(clip)+" 1000 -32767 -32767 -32767 2000 500 -100001 0 0 0 1000\n";
    Fixture saved(std::move(plain),true,tables);saved.command("textSpeed","0");state=saved.command("action","start");auto before=visibleLayers(state);saved.command("save","2");auto doc=mahoyo::save::read(saved.root/"savedata/slot2.whs");std::string extra(doc.hostExtra.begin(),doc.hostExtra.end());const std::string match="\n0 915 ";auto at=extra.find(match);check(at!=std::string::npos,"saved native effect row exists");extra.replace(at,match.size(),"\n0 3096 ");doc.hostExtra.assign(extra.begin(),extra.end());mahoyo::save::writeAtomic(saved.root/"savedata/slot2.whs",doc);state=saved.command("load","2");check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&visibleLayers(state)==before,"WH13 accepts derived special-time playback without changing canonical phase or layer fields");saved.passed=true;
    for(int mode:{2,3,5,9}){Program child;child.image(1000,"fake",18,{{0,0,1920,1080,0,1}});child.image(1000,"fake",19,{},18);child.blend(19,mode,0);child.dialogue(0);Fixture rejected(std::move(child),true);state=woh_command(rejected.handle,"action","start");check(state.find("Unsupported native window child blend")!=std::string::npos,"child5flag0 and unreviewed child2/3/9 remain rejected");rejected.passed=true;}
    Program replace;replace.clock();replace.commit();replace.dialogue(0);for(int n:{0,0,0,1000,3400,3400,3})replace.push(n);replace.op(0x247);replace.dialogue(1);replace.clockTime(2,10,51);for(int n:{1,4,-1})replace.push(n);replace.op(0x27f);replace.dialogue(0);replace.op(0xf4);
    Fixture duration(std::move(replace),false,"",clockLayout);prepareClockImages(duration);duration.command("textSpeed","0");state=acknowledge(duration,duration.command("action","start"));duration.command("tick","1000");duration.command("advance");state=acknowledge(duration,duration.command("advance"));duration.command("tick","2000");state=duration.command("tick","2284");check(waiting(state,"input")&&boolean(state,"canSave"),"prepared381 replaces an old1000ms rotation;27f waits only its new300ms job");duration.passed=true;
}
static void nativeClockBoundsAndErrors(){
    for(int first:{-1,59,64}){Program p;p.push(789);p.clock(first,10,51,0,0,0,6);p.clockTime(first,10,51);p.push(0);p.op(0xe0);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto state=f.command("action","start");check(visibleLayers(state)=="[]"&&boolean(state,"canSave"),"invalid unsigned base consumes arguments without requiring resources");f.command("save","2");InspectHost host;mahoyo::vm::Machine vm(host);vm.loadState(mahoyo::save::read(f.root/"savedata/slot2.whs").vm);check(vm.variable(0)==789,"380/381 bounds preserve the lower operand and no result");f.passed=true;}
    for(int kind=0;kind<3;++kind){Program p;p.image(1000,"fake",20);p.clock(2,10,51,-375,-283,1000,kind==2?83:35);p.commit();p.dialogue(0);Fixture f(std::move(p),true,"",kind==0?"":clockLayout);if(kind)prepareClockImages(f,kind==1);std::string state=woh_command(f.handle,"action","start");check(state.find("\"error\":\"\"")==std::string::npos&&state.find("\"image\":\"img1242.mzp\"")==std::string::npos,"missing clock metadata/resource or unsupported color cannot partially create clock parts");f.passed=true;}
    Program missing;missing.clockTime(2,10,51);for(int n:{1,4,-1})missing.push(n);missing.op(0x27f);missing.dialogue(0);Fixture absent(std::move(missing));auto state=absent.command("action","start");auto tail=state.substr(state.find("\"transition\":"));state=woh_command(absent.handle,"transitionReady",std::to_string(uint64_t(number(tail,"id"))).c_str());check(state.find("Missing clock rotation target")!=std::string::npos,"381 cannot silently invent missing graphics");absent.passed=true;
    Program clear;clear.clock();clear.commit();clear.dialogue(0);clear.clockTime(2,10,51);clear.push(-1);clear.op(0x241);clear.dialogue(1);clear.op(0xf4);Fixture clean(std::move(clear),false,"",clockLayout);prepareClockImages(clean);clean.command("textSpeed","0");state=acknowledge(clean,clean.command("action","start"));clean.command("tick","1000");state=clean.command("advance");check(waiting(state,"input")&&visibleLayers(state)=="[]"&&boolean(state,"canSave")&&number(state,"nextTickMs")==0,"clear cancels unprepared381 work as well as all current layers");clean.passed=true;
}
static void nativeChild5A8EffectPropertyGate(){
    for(int mode:{5,6,21,23})for(int flag:{0,1}){
        Program p;p.image(1000,"fake",31,{{0,0,1920,1080,0,1}});p.image(1000,"fake",34,{},31);p.blend(34,0,flag);for(int n:{0,0,0,915,34})p.push(n);p.op(0x278);p.dialogue(0);p.op(0xf4);
        const std::string table="915 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n915 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n";
        const std::string windows="915 0 0 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n";
        Fixture f(std::move(p),true,table,"","",windows);woh_destroy(f.handle);auto path=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(path);{std::ofstream out(path);for(int i=0;i<2;++i){out<<"915 "<<i<<' '<<i<<" 0x2000000";for(int n=0;n<13;++n)out<<' '<<(n==2?mode:-32767);out<<'\n';}}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));
        if(flag!=(mode==5?1:0))check(state.find("Unsupported effect blend mode "+std::to_string(mode))!=std::string::npos,"effect properties cannot bypass5A8 child flag restrictions");
        else{check(number(layerObject(state,34),"blend")==mode,"effect blend property takes the same reviewed5A8 route");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"fresh active-property restore retains reviewed5A8 child flag");}f.passed=true;
    }
}
static void nativeChild5A8BlendCaptureAndRestore(){
    for(int mode:{5,6,21,23}){
        const int flag=mode==5?1:0;
        Program p;p.image(1000,"fake",30,{{0,0,1920,1080,0,1}});p.image(1000,"fake",31,{{0,0,1920,1080,0,1}});p.image(1000,"fake",34,{},31);p.image(1000,"fake",35);p.alpha(31,64);p.alpha(34,128);p.blend(34,mode,flag);p.commit();p.dialogue(0);p.clone(36,34,"fake");p.dialogue(1);p.blend(34,0);for(int n:{0,256,0,0,300,0})p.push(n);p.op(0x200);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));if(waiting(state,"frame"))state=f.command("tick","1000");auto child=layerObject(state,34);
        check(number(child,"parent")==31&&number(child,"blend")==mode&&number(child,"blendFlag")==flag,"actual5A8 child blend and flag retain the source graph");auto before=visibleLayers(state);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh save restores reviewed5A8 child and opacity without replay");
        std::vector<mahoyo::save::Document> invalid;
        invalid.push_back(editSavedLayer(good,34,[&](auto& row){row[14]=std::to_string(1-flag);}));
        invalid.push_back(editSavedLayer(good,34,[](auto& row){row[21]="1920";row[22]="1080";}));
        invalid.push_back(editSavedLayer(good,31,[](auto& row){row[21]="0";row[22]="0";}));
        invalid.push_back(editSavedLayer(good,31,[](auto& row){row[25]="30";}));
        invalid.push_back(editSavedLayer(good,35,[](auto& row){row[25]="34";}));
        for(const auto& bad:invalid){mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==before,"unreviewed5A8 child flag/window/nesting fails before mutating the live Runtime");}
        mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);f.command("load","2");state=f.command("advance");check(number(layerObject(state,36),"blend")==mode&&number(layerObject(state,36),"blendFlag")==flag,"clone preserves each reviewed5A8 child blend flag");state=f.command("advance");check(waiting(state,"transitionReady"),"child5A8 transition capture is a graphics barrier");auto old=state.substr(state.find("\"old\":{\"scene\""));check(number(layerObject(state,34),"blend")==0&&number(layerObject(old,34),"blend")==mode,"old capture retains5A8 child composite before new blend");f.passed=true;
    }
    for(int mode:{5,6,21,23}){Program bad;bad.image(1000,"fake",31,{{0,0,1920,1080,0,1}});bad.image(1000,"fake",34,{},31);bad.blend(34,mode,mode==5?0:1);bad.dialogue(0);Fixture f(std::move(bad),true);auto state=std::string(woh_command(f.handle,"action","start"));check(state.find("Unsupported native window child blend mode/flag")!=std::string::npos,"252 rejects unsupported5A8 child flags at the host boundary");f.passed=true;}
    Program root;root.image();root.blend(0,6,0);root.dialogue(0);root.op(0xf4);Fixture f(std::move(root),true);f.command("textSpeed","0");auto state=f.command("action","start");check(number(layerObject(state,0),"blend")==6,"reviewed114 q0 root6 path is admitted independently");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"root6 roundtrip retains its descriptor");f.passed=true;
}
#include "archive_runtime_cases.h"
#include "extra_frontend_runtime_cases.h"
#include "screen_wave_runtime_cases.h"
#include "screen_wave1_runtime_cases.h"
#include "history_runtime_cases.h"
#include "effect0_runtime_cases.h"
#include "wait_clock_runtime_cases.h"
static void nativeRootEightTenCaptureCloneAndRestore(){
    for(int mode:{8,10})for(int raw:{-1,0,1,2}){
        Program p;p.image(1000,"fake",9,{{0,0,1920,1080,0,1}});p.image(1000,"fake",15);p.alpha(15,96);p.blend(15,mode,raw);p.commit();p.dialogue(0);p.clone(16,15,"fake");p.dialogue(1);p.blend(15,0);for(int n:{0,256,0,0,300,0})p.push(n);p.op(0x200);p.dialogue(0);p.op(0xf4);
        Fixture f(std::move(p),true,"","fake 100 100\n");f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));if(waiting(state,"frame"))state=f.command("tick","1000");const int flag=raw==1?1:0;auto layer=layerObject(state,15);check(number(layer,"blend")==mode&&number(layer,"blendFlag")==flag&&number(layer,"parent")==-1,"root8/10 preserve native normalized flag and root context");auto before=visibleLayers(state);f.command("save","2");auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");state=f.command("load","2");check(visibleLayers(state)==before,"fresh Runtime restores root8/10 fields without replay");
        auto bad=editSavedLayer(good,15,[](auto& row){row[25]="9";});mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",bad);state=f.command("load","2");if(mode==10&&flag==0)check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&number(layerObject(state,15),"parent")==9,"reviewed saved child10flag0 binds the existing window parent");else check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&visibleLayers(state)==before,"unreviewed child8/child10flag1 restore is atomic");mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",good);f.command("load","2");state=f.command("advance");check(number(layerObject(state,16),"blend")==mode&&number(layerObject(state,16),"blendFlag")==flag&&number(layerObject(state,16),"parent")==-1,"clone retains root8/10 and its flag");state=f.command("advance");check(waiting(state,"transitionReady"),"root8/10 capture waits for prepared graphics");auto old=state.substr(state.find("\"old\":{\"scene\""));check(number(layerObject(state,15),"blend")==0&&number(layerObject(old,15),"blend")==mode,"transition retains original root8/10 independently of the changed scene");f.passed=true;
    }
    for(int mode:{8,10})for(int flag:{0,1}){Program p;p.image(1000,"fake",9,{{0,0,1920,1080,0,1}});p.image(1000,"fake",15,{},9);p.blend(15,mode,flag);p.commit();p.dialogue(0);Fixture f(std::move(p),true);auto state=std::string(woh_command(f.handle,"action","start"));if(mode==10&&flag==0)check(waiting(state,"transitionReady")&&number(layerObject(state,15),"blend")==10&&number(layerObject(state,15),"parent")==9,"252 admits reviewed child10flag0");else check(state.find("Unsupported native window child blend mode/flag")!=std::string::npos,"252 still rejects child8 and child10flag1");f.passed=true;}
}
static void nativeRootEightTenEffectPropertyGate(){
    const std::string table="915 0 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n915 1000 -32767 -32767 -32767 -32767 -32767 -100001 0 0 0 1000\n";
    const std::string windows="915 0 0 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n915 1 1 0x2000000 -32767 -32767 -32767 -32767 -32767 -32767\n";
    for(int mode:{8,10})for(int flag:{0,1})for(bool child:{false,true}){
        Program p;if(child)p.image(1000,"fake",9,{{0,0,1920,1080,0,1}});p.image(1000,"fake",15,{},child?9:-1);p.blend(15,0,flag);for(int n:{0,0,0,915,15})p.push(n);p.op(0x278);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p),true,table,"","",windows);woh_destroy(f.handle);auto path=f.game/"meta/effects-properties.tsv";f.extraFiles.push_back(path);{std::ofstream out(path);for(int index=0;index<2;++index){out<<"915 "<<index<<' '<<index<<" 0x2000000";for(int field=0;field<13;++field)out<<' '<<(field==2?mode:-32767);out<<'\n';}}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto state=std::string(woh_command(f.handle,"action","start"));
        if(child&&!(mode==10&&flag==0))check(state.find("Unsupported effect blend mode")!=std::string::npos,"effect properties retain unreviewed child8/child10flag1 rejection");
        else{check(number(layerObject(state,15),"blend")==mode&&number(layerObject(state,15),"blendFlag")==flag,"effect property selects root8/10 without changing its flag");auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");check(visibleLayers(state)==before,"fresh Runtime restores root8/10 alongside active effect properties");}f.passed=true;
    }
}
#include "child12_runtime_cases.h"
#include "child17_flag1_runtime_cases.h"
#include "child20_runtime_cases.h"
#include "child14_runtime_cases.h"
#include "child25_runtime_cases.h"
#include "child18_runtime_cases.h"
#include "root18_runtime_cases.h"
#include "wave_bitmap_runtime_cases.h"
#include "choices_runtime_cases.inc"
#include "effect_capture_wait_runtime_cases.h"
#include "native_variant_runtime_cases.h"
#include "global_filter_runtime_cases.h"
#include "layer_mask_clear_runtime_cases.h"
#include "wave23_runtime_cases.h"
#include "forward_parent_runtime_cases.h"
#include "native223_runtime_cases.h"
#include "native130_runtime_cases.h"
#include "global270_mode2_runtime_cases.h"
#include "wave_next_runtime_cases.h"
#include "noise78_runtime_cases.h"
#include "child13_runtime_cases.h"
#include "bitmap_geometry_runtime_cases.h"
#include "child4_10_runtime_cases.h"
#include "input_region_runtime_cases.h"
#include "mask25b_runtime_cases.h"
#include "noise_extension_runtime_cases.h"
#include "transition_mode2_runtime_cases.h"
#include "wave_batch_runtime_cases.h"
#include "wave41_48_runtime_cases.h"
#include "wave_own_window_runtime_cases.h"
#include "child7_runtime_cases.h"
#include "mask25b_generic_runtime_cases.h"
#include "extra_shake270_runtime_cases.h"
#include "relative244_runtime_cases.h"
#include "extra_shake270_mode4_runtime_cases.h"
#include "extra_shake270_mode6_runtime_cases.h"
#include "extra_shake270_mode5_runtime_cases.h"
#include "color253_pending_runtime_cases.h"
#include "mask1_extra_runtime_cases.h"
#include "native118_12f_runtime_cases.h"
#include "layer274_runtime_cases.h"
#include "native27a_runtime_cases.h"
#include "particle_overlay_runtime_cases.h"
#include "particle_overlay_api_cases.h"
#include "host_state20_runtime_cases.h"
#include "audio_save_runtime_cases.h"
int main(){try{audio_save_cases::run();native_wave_own_window_cases::run();generic_mask_cases::run();nativeChild7WindowCloneRestore();nativeChild7EffectPropertyGate();native_wave41_48_cases::run();host_state20_cases::run();particle_overlay_api_cases::run();particle_overlay_cases::run();native27a_cases::run();native274PersistentMotionAndClear();native274ClockRngAndGuard();native274StackAndBounds();native118_12f_cases::run();nativeMask1ExtraAndNoisePrepare110();nativeExtra270ConstantMode5();nativeExtra270ReducedMode6();nativeExtra270SquareMode3();nativeScreenRasterOneSaveRestoreAndStop();nativeScreenRasterOneReplacementAndStrictRestore();relative244RuntimeCases();nativeVoiceLinkedShakeMode4();color253_pending_cases::run();nativeTransitionModeZeroTwoClock();nativeNoiseExtensionRuntimeCases();nativeMask7TransientRoutesAndRestore();bitmapGeometryLegacyAnd19Restores();bitmapGeometryOlderReaders();bitmapGeometryEffectBaselines();nativeChild4Or10RoutesAndRestore();input120_cases::run();nativeChild13RoutesAndRestore();nativeNoise78ExactProfileAndRestore();nativeWaveNextProfilesRestoreEnabled();global270Mode2AllProfiles();global270Mode2CacheCompleteCancel();global270Mode2DelayWaitPause();native130MainStackAndFreshRestore();native130ArchiveModeAndTitleReset();nativeWaveBatchProfilesRestoreAndClock();nativeWaveBatchMode0BoundsAndProfiles();native223EmptyAndRepeated();native223ActualStackAndSentinels();native223PreservesPendingLayersAudioAndRestore();nativeWave23Effect4254DynamicState();nativeWave23ProfileAndBitmapBounds();forwardParentBatchAndDeletedPreviousFrame();unresolvedForwardParentAndCycleRemainStrict();nativeChild18ActualCloneRestore();nativeChild18Effects4059And4061();nativeChild25ColourContrastCloneRestore();nativeChild25Effects3903And3905();nativeChild14ParentColorFlipAndRestore();nativeChild14EffectPropertyContexts();nativeRoot18CaptureCloneAndRestore();nativeRoot18EffectPropertyContexts();layerMaskClearStackAndRepeatedIds();layerMaskClearPreservesLiveAndEffectState();layerMaskUnreviewedProfilesAndUnderflowStayStrict();nativeChild20CaptureCloneAndRestore();nativeChild20EffectPropertyGate();neutralGlobalFilterOnePopNoPushAndImmediate();neutralGlobalFilterPreservesLayersAndPendingEffect();globalFilterNonNeutralAndEmptyRemainStrict();nativeMissingEffectVariantAndFreshBaselines();nativeValidVariantAndFiniteCompletion();nativeVariantStrictFallbackAndDirect256();nativeVariantInventoryMalformedCreationErrors();capturedEffectPreparesBeforeFirstWait();capturedEffectZeroAndShortWaitPrepare();capturedEffectUnknownWaitRemainsStrict();nativeWaveTwelveThirteenBitmapBounds();nativeChild17Flag1CaptureCloneAndRestore();nativeChild17Flag1EffectPropertyGate();choicesTextBranchAndGenerations();choicesWikHideDirectSlotAndLoadReset();choicesUnsupportedProfilesAndBadTargets();nativeChild12CaptureCloneAndRestore();nativeChild12EffectPropertyGate();waitClockMathAndNativeBoundaries();native112CutAndCompensated110();native110ThirdFlagsAndPreparePause();native112SaveRestoreAndMalformedAtomic();nativeEffectZeroCanonicalCloneAndNewPlayback();nativeEffectZeroCanonicalPendingAndCleanup();nativeEffectZeroFiniteFollowupMotion();nativeEffectZeroPrepareAndCurrentValue();nativeEffectZeroKeepsIndependentJobs();nativeEffectZeroBoundsAndLatestRequest();nativeEffectZeroResetAndCaptureIndependence();extraCatalogContracts();galleryRuntimeGatesAndReturn();musicRuntimeTimingsAndCompletion();archiveCheckpointReturnHasNoPicker();musicRuntimeErrorCanExit();historyMetadataNativeRowsAndReplay();historyNormalLifetimeClearTrimAndNull();historySavePendingLegacyAndAtomicReject();waveElevenNativeAndLazyClock();waveTenVariantCaptureCloneAndRestore();nativeRootEightTenCaptureCloneAndRestore();nativeRootEightTenEffectPropertyGate();screenWaveRuntimeSaveCloneAndClear();screenWaveFadeClockWaitAndCapture();screenWaveRestoreBoundsAndKnownScope();archiveParserAndGates();nativeArchiveLifecycle();nativeMusicUnlockAndTitleCue();titleCuePreservesLegacyCheckpoint();titleOnlyNeverMasksLegacyImport();nativeChild5A8EffectPropertyGate();nativeChild5A8BlendCaptureAndRestore();nativeFontReset14b();nativeFontCarryCompletionClearAndLegacy();nativeColor7CloneCaptureRestore();nativeChild24PropertyGate();nativeChild24FlagRestoreAndCapture();global270NativeGroupsAndScheduler();global270IdleCacheSaveAndLegacy();rootBlendTwoThreeEffectAndRestoreGates();noiseCallbackGridBoundariesAndPop();nativeNoiseController();noisePrepareIdempotencyWakeCloneAndClear();noiseFreshRestorePauseHistoryAndSameBatch();noiseAlphaDisabledFastWorkAndGuards();specialTimeFreshRestoreCloneAndCompletion();nativeClockCreationSaveAndDelete();nativeClockPendingRotationTiming();nativeClockReplacementAndRemapSaveGuard();nativeClockBoundsAndErrors();parentedWaveSavePhaseAndDetach();parentedWaveRestoreRejectsUnsupported();parentedWaveKnownBlendAndContextGates();cloneEffectFullMapBoundary();waveBatchReadyAndCleanup();waveWaitFadeAndPause();waveSaveCloneMalformedAndLegacy();waveCapturedOldScenePhase();wavePopBoundsAndImplicitTextBarrier();bgmLoopContract();teatimeOpeningIdleAndNext();teatimePauseSaveReturnAndRestore();teatimeNumericPopupState();sharedProgressSurvivesOlderSlotAndFreshProcess();legacySlotAdoptsSharedWithoutCommonFile();nativeWindowModeState();nativeTrophyGatesAndPersistence();parentWindowOffsetAndDetach();nativeEffectWaitMask();nativeEffectWaitSkipsLoops();nativeDialogueWindowLifecycle();effectCaptureWindowSave();solidColorScaleWithoutAsset();backendOneSound190();layerShakeReadyGeometryAndCompletion();layerShakeProjectedQuadInputs();layerShakeReplacementDelayAndCancel();layerShakePauseResetDeleteAndModes();layerRandomReadyHeldAndFinite();layerRandomDelayPauseAndIndefiniteCancel();layerRandomIndependentReplacementAndZero();backendOneSoundLoop();inactiveAndSupersededVolumeCleanup();globalShakeReadyAndGeometry();globalShakeClearDelayAndPause();globalShakeLoadAndTitleReset();delayedExistingSoundVolume();volumeDelayPauseAndMute();volumeReservationReplacementAndRestart();volumeFadeLoopAndOperandContract();nativeBlendFlagNormalization();imagePreloadPopAndVisibility();imagePreloadCommitSaveAndRelease();nativeImageUnlock();nativeZeroResultPreservesStack();targetPixelOffsetSaveRestore();nativePaletteSaveRestore();nativeOriginCoordinates();delayedSoundAndPan();capturedSceneTransition();immediateCommitWaitsForAssets();idleAppendClock();idleAutoClock();revealDoesNotReplaceFrameDeadline();disabledLayerSaveRestore();simultaneousMotionAndScale();effectAbsoluteScaleAndCompletion();cloneIntrinsicScaleAndFreshSave();cloneCustomOriginsAreAllOrNothing();cloneActiveLoopTerminalAndWh12Baseline();textControlSaveMigration();continuationSpacing();completedDialogueLineBreaks();exactVariantOriginAndLegacyRestore();nativeMetadataAnchor();std::cout<<"Runtime native result, clock, save, motion/scale, effects, text controls and native anchor regressions passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
