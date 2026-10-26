#include "runtime_api.h"
#include "language_assets.h"
#include "language_text.h"
#include "steam11_script_prefixes.h"
#include "steam11_korean_geometry.h"
#include "image_header.h"
#include "vm/machine.h"
#include "save_store.h"
#include "host_state20.h"
#include "bitmap_geometry.h"
#include "effects.h"
#include "global_shake.h"
#include "layer_shake.h"
#include "window_fade.h"
#include "shared_state.h"
#include "teatime.h"
#include "archive_catalog.h"
#include "extra_catalog.h"
#include "wave.h"
#include "screen_wave.h"
#include "noise.h"
#include "clock.h"
#include "history.h"
#include "wait_clock.h"
#include "choices.h"
#include "alpha_rule.h"
#include "input_region.h"
#include <limits>
#include "text.h"
#include "movie.h"
#include "particle_overlay.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace {
using mahoyo::vm::Flow;
using mahoyo::vm::Machine;
uint64_t historyToken(){static uint64_t token=0;if(token==UINT64_MAX)throw mahoyo::vm::Error("History identity exhausted");return ++token;}
std::string quote(const std::string& value) {
    std::string out="\"";
    const char* hex="0123456789abcdef";
    for(size_t i=0;i<value.size();++i) {
        unsigned char c=static_cast<unsigned char>(value[i]);
        if(c=='"'||c=='\\') {out+='\\';out+=char(c);}
        else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else if(c<128)out+=char(c);
        else {
            size_t n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
            bool valid=n&&i+n<=value.size();uint32_t point=n?c&((1u<<(7-n))-1u):0;
            for(size_t j=1;valid&&j<n;++j){auto b=static_cast<unsigned char>(value[i+j]);valid=(b&0xc0)==0x80;point=(point<<6)|(b&0x3f);}
            valid=valid&&point>=uint32_t(n==2?0x80:n==3?0x800:0x10000)&&point<=0x10ffff&&!(point>=0xd800&&point<=0xdfff);
            if(valid){out.append(value,i,n);i+=n-1;}else out+="\\ufffd";
        }
    }
    return out+'"';
}
std::string lower(std::string name) {
    for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    return name;
}
bool safeName(const std::string& name) {
    return !name.empty()&&name!="."&&name!=".."&&name.find('/')==std::string::npos&&name.find('\\')==std::string::npos;
}
std::set<std::string> readNativeImageResources(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path))return {};
    auto invalid=[](){throw mahoyo::vm::Error("Invalid native image resource inventory");};
    if(std::filesystem::file_size(path)>2*1024*1024)invalid();
    std::ifstream input(path,std::ios::binary);if(!input)invalid();
    std::string header;std::getline(input,header);std::istringstream fields(header);
    std::string signature,checksum,extra;size_t count=0;
    if(!(fields>>signature>>count>>checksum)||fields>>extra||signature!="WOH_NATIVE_IMAGES1"||!count||count>20000||checksum.size()!=16||
       !std::all_of(checksum.begin(),checksum.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))invalid();
    if(header!="WOH_NATIVE_IMAGES1 "+std::to_string(count)+" "+checksum)invalid();
    std::string body((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    uint64_t hash=14695981039346656037ull;for(unsigned char c:body){hash^=c;hash*=1099511628211ull;}
    if(body.empty()||body.back()!='\n'||hash!=std::stoull(checksum,nullptr,16))invalid();
    std::set<std::string> result;std::istringstream names(body);std::string name,previous;
    while(std::getline(names,name)){
        if(name.size()<=4||name.substr(name.size()-4)!=".mzp"||name<=previous||
           !std::all_of(name.begin(),name.end()-4,[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';}))invalid();
        previous=name;result.insert(std::move(name));
    }
    if(result.size()!=count)invalid();return result;
}
bool supportedColorMode(int mode) {
    return mode>=0&&mode<=82;
}
bool supportedRootBlendMode(int mode){
    switch(mode){case 0:case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:
        case 11:case 12:case 13:case 14:case 15:case 16:case 17:case 18:case 19:
        case 20:case 21:case 22:case 23:case 24:case 25:return true;
        default:return false;}
}
bool supportedWindowChildBlend(int mode,int flag){
    switch(mode){case 0:case 1:case 11:case 16:case 19:case 22:return true;
        case 15:return flag!=1;
        case 17:return flag==0||flag==1;
        case 4:case 10:case 6:case 7:case 12:case 13:case 14:case 20:case 21:case 23:case 24:case 25:case 18:return flag==0;
        case 5:return flag==1;
        default:return false;}
}
bool strictWindowChildContext(int mode){return mode==4||mode==10||mode==5||mode==6||mode==7||mode==12||mode==13||mode==14||mode==17||mode==20||mode==21||mode==23||mode==24||mode==25||mode==18;}
std::vector<uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw mahoyo::vm::Error("Cannot read "+path.filename().string());
    return {std::istreambuf_iterator<char>(file),{}};
}
struct Layer {
    int id=0,z=0;std::string image,kind="event",color;
    double x=0,y=0,w=1920,h=1080,alpha=1;
    std::array<int,4> crop{};bool hasCrop=false;
    double baseAlpha=1;
    bool enabled=true;
    double rotation=0,pivotX=.5,pivotY=.5;
    std::string baseImage;
    int variantX=0,variantY=0,colorMode=0,colorValue=0,rgb=0,brightness=0,blend=0,blendFlag=0,decimalMode=0;
    int targetOffsetX=0,targetOffsetY=0,targetOffsetDivisor=1;
    std::array<int32_t,6> window{}; //XYWH,color enum,soft width; canvas pixels.
    int parent=-1,matrixMode=0;
    double drawOffsetX=0,drawOffsetY=0,uniformScale=1,axisScaleX=1,axisScaleY=1;
};
struct EffectCapture {int clip;Layer layer;uint64_t queuedAt;};
struct Audio {uint64_t id;std::string name,channel;bool loop;double volume=1,pan=64;int oldFadeOutMs=0;};
struct AudioLoop {uint32_t sampleCount=0,sampleRate=0,channels=0,wrapperLoop=0,start=0,end=0;};
struct Animation {int id;double from,to;uint64_t begin,end;int curve=0;};
struct Effect {int id,clip;uint64_t begin;double x,y,w,h,alpha;double rotation=0,sourceW=0,sourceH=0;std::array<int32_t,6> window{};mahoyo::effects::Properties properties;double pivotX=.5,pivotY=.5;double drawOffsetX=0,drawOffsetY=0,uniformScale=1,axisScaleX=1,axisScaleY=1;bool paused=false;uint64_t pausedAt=0;};
static uint64_t effectRawElapsed(const Effect& e,uint64_t now){return (e.paused?e.pausedAt:now)-e.begin;}
static void setEffectPaused(Effect& e,bool paused,uint64_t now){if(e.paused==paused)return;if(paused)e.pausedAt=now;else{e.begin+=now-e.pausedAt;e.pausedAt=0;}e.paused=paused;}
void writeEffectPacket(std::ostream& out,const Effect& e,uint64_t elapsed){
    out<<e.id<<' '<<e.clip<<' '<<elapsed<<' '<<e.x<<' '<<e.y<<' '<<e.w<<' '<<e.h<<' '<<e.alpha<<' '<<e.rotation<<' '<<e.sourceW<<' '<<e.sourceH;
    for(auto value:e.window)out<<' '<<value;const auto& p=e.properties;
    out<<' '<<p.originX<<' '<<p.originY<<' '<<p.blendMode<<' '<<p.colorMode<<' '<<p.rgb<<' '<<p.variantX<<' '<<p.variantY<<' '<<p.brightness<<' '<<p.contrast<<' '<<p.priority<<' '<<e.pivotX<<' '<<e.pivotY<<' '<<e.drawOffsetX<<' '<<e.drawOffsetY<<' '<<e.uniformScale<<' '<<e.axisScaleX<<' '<<e.axisScaleY<<' '<<p.decimalMode<<'\n';
}
struct Motion {int id;double x0,y0,x1,y1;uint64_t begin,end;int curve=0;double pivotX=.5,pivotY=.5;};
struct PendingRelativeMotion {float x=0,y=0;int duration=0,curve=0;};
struct Transform {int id,mask;double fromX,fromY,toX,toY;uint64_t begin,end;int curve;int matrixMode=0;};
struct AudioFade {std::string channel;double from,to;uint64_t begin,end;bool stop;double fromPan=64,toPan=64;bool volumeActive=true,panActive=true,quantizeVolume=false;};
struct PendingSound {std::string name,channel;bool loop;double from,to,fromPan,toPan;uint64_t begin,duration;};
struct PendingVolumeFade {std::string channel;double to;uint64_t begin,duration;};
// Native FF5F0/15DBD0: observed mode2 tuple scope, same capture/fader as0.
bool reviewedMode2Transition(const std::array<int,6>& args){
    //Native FF5F0/15DBD0 preserve all fields;146570 tests only mode!=1.
    //Reuse mode0's renderer and resources inside this reviewed argument domain.
    return args[0]==2&&args[1]>=0&&args[1]<=86400000&&args[2]>=0&&args[2]<=14
        &&args[3]>=-1&&args[3]<=1&&args[4]>=0&&args[4]<=256&&(args[5]==0||args[5]==1);
}
struct Transition {uint64_t id=0,begin=0;int duration=0;bool active=false;std::string layers="[]",text="[]";int mode=0;int maskId=0,strength=256;std::string mask,movie="null",waitAfterReady,sourceScene="play";uint64_t deadlineAfterReady=0;bool waveWait=false;int waveWaitLayer=-1;bool clockWait=false;int clockWaitLayer=-1;uint64_t nonrotationDeadline=0;bool pulseWait=false;int pulseWaitLayer=-1;std::string screenWave="null";bool waitClockCaptured=false,wait110=false;uint64_t waitClockAge=0;int wait110Duration=0,wait110Mode=0;int nativeMode=-1,nativeStepMs=50;uint64_t nativePhaseMs=0;std::string particles="null",sourceModule;};
struct Shake {uint64_t mask,begin,phase=UINT64_MAX;int x,y,period,duration;double dx[2]{},dy[2]{};};
constexpr int LayerPulseMotion274=274;
struct LayerPulse {mahoyo::effects::LayerShakeMode2 parameters;uint64_t startsAt=0;int mode=2;mahoyo::effects::LayerShakeRandom random;mahoyo::effects::LayerShakeClock clock;uint64_t serial=0;mahoyo::effects::LayerMotion274 motion274;};
struct PendingLayerPulse {mahoyo::effects::LayerShakeMode2 parameters;int delayMs=0;bool clear=false;int mode=2;};
struct ImageHint {
    int layerId=-1,tag=0;std::string base;int variantX=0,variantY=0;
    std::array<int,4> range{};int brightness=0,contrast=0,colorMode=0,rgb=0;
};
struct PreloadState {
    std::map<int,ImageHint> ranges,pendingRanges;
    std::vector<ImageHint> retained,pending;
    bool releaseOld=false;
};
constexpr size_t MaxImageHints=4096;
void writeHint(std::ostream& out,const ImageHint& h) {
    out<<h.layerId<<' '<<h.tag<<' '<<std::quoted(h.base)<<' '<<h.variantX<<' '<<h.variantY;
    for(auto value:h.range)out<<' '<<value;
    out<<' '<<h.brightness<<' '<<h.contrast<<' '<<h.colorMode<<' '<<h.rgb<<'\n';
}
void writePreloads(std::ostream& out,const PreloadState& p) {
    //WOHHOST7 appends PRELOADS1/releaseOld then four counted groups: applied
    //ranges,pending ranges,retained222,pending222. Rows contain layerId,tag,
    //quoted base,variantX/Y,xMin/yMin/xMax/yMax,brightness,contrast,mode,rgb.
    //These are cache intentions only; no decoded/GPU handle is serialized.
    out<<"PRELOADS1 "<<p.releaseOld<<'\n'<<p.ranges.size()<<'\n';for(const auto& pair:p.ranges)writeHint(out,pair.second);
    out<<p.pendingRanges.size()<<'\n';for(const auto& pair:p.pendingRanges)writeHint(out,pair.second);
    out<<p.retained.size()<<'\n';for(const auto& h:p.retained)writeHint(out,h);
    out<<p.pending.size()<<'\n';for(const auto& h:p.pending)writeHint(out,h);
}
PreloadState readPreloads(std::istream& input) {
    PreloadState p;std::string signature;
    if(!(input>>signature>>p.releaseOld)||signature!="PRELOADS1")throw mahoyo::save::Error("Invalid saved preload extension");
    auto readHint=[&](bool range){ImageHint h;
        if(!(input>>h.layerId>>h.tag>>std::quoted(h.base)>>h.variantX>>h.variantY>>h.range[0]>>h.range[1]>>h.range[2]>>h.range[3]>>h.brightness>>h.contrast>>h.colorMode>>h.rgb)||h.base.size()>4096||!safeName(h.base)||(range?(h.layerId<0||h.layerId>=64):h.layerId!=-1))throw mahoyo::save::Error("Invalid saved image hint");
        return h;};
    auto readRanges=[&](std::map<int,ImageHint>& values){size_t count;if(!(input>>count)||count>64)throw mahoyo::save::Error("Invalid saved preload range count");for(size_t i=0;i<count;++i){auto h=readHint(true);if(!values.emplace(h.layerId,std::move(h)).second)throw mahoyo::save::Error("Duplicate saved preload layer");}};
    auto readImages=[&](std::vector<ImageHint>& values){size_t count;if(!(input>>count)||count>MaxImageHints)throw mahoyo::save::Error("Invalid saved preload image count");for(size_t i=0;i<count;++i)values.push_back(readHint(false));};
    readRanges(p.ranges);readRanges(p.pendingRanges);readImages(p.retained);readImages(p.pending);
    if(!p.releaseOld&&p.retained.size()+p.pending.size()>MaxImageHints)throw mahoyo::save::Error("Saved preload queue exceeds limit");
    return p;
}
double ease(double p,int curve) { return mahoyo::effects::ease(p,curve); }
double nativeAngle(double hundredths) {
    auto value=uint16_t(int64_t(hundredths*32768/18000));
    int signedValue=value<32768?int(value):int(value)-65536;
    return signedValue*180./32768;
}
struct Runtime final:mahoyo::vm::Host {
    std::filesystem::path root;
    mahoyo::language::Assets languageAssets;
    std::unique_ptr<Machine> machine;
    mahoyo::shared::State sharedState;bool sharedLoaded=false,sharedWritePending=false;
    std::set<size_t> pendingFrontendFlags;
    mahoyo::teatime::Controller intermission;uint64_t intermissionGeneration=0;
    mahoyo::choices::Controller choices;std::optional<std::vector<mahoyo::choices::Layout>> wikChoiceLayout;
    std::optional<mahoyo::archive::Catalog> archiveCatalog;
    uint64_t archiveGeneration=0;int archiveSelected=0,archiveCheckpoint=0,archiveReplay=0;
    std::optional<mahoyo::extra::Gallery> galleryCatalog;std::optional<mahoyo::extra::Music> musicCatalog;
    std::map<int,std::string> checkpointTargets;uint64_t frontendGeneration=0;
    int galleryGroup=-1,galleryVariant=-1,galleryFocus=-1,musicRow=-1,musicLast=-1,musicPending=-1;
    std::string musicPhase="idle";bool musicRepeat=false;uint64_t musicDue=0;
    std::vector<std::string> table,history;
    std::vector<mahoyo::language::TextSource> paragraphSources,historySources;
    std::string pendingLanguage;
    std::map<std::string,std::map<std::string,std::string>> menuMessages;
    mahoyo::history::State historyMeta;
    std::optional<mahoyo::history::Voice> deferredVoice;
    uint64_t historyRevision=historyToken(),historyGeneration=historyToken();
    struct HistoryClip {size_t row,index;std::string path;int actor,volume,pan,variant,gain;};
    struct HistoryReplay {uint64_t id,generation;std::vector<HistoryClip> clips;};
    std::optional<HistoryReplay> historyReplay;
    uint64_t cachedHistoryRevision=0;std::string cachedHistory="[]";
    std::array<int,15> historyActorVolumes=[]{std::array<int,15> v;v.fill(128);return v;}();
    std::map<int,std::string> music;
    std::map<int,std::string> masks;
    std::map<std::string,AudioLoop> audioLoops;
    std::set<std::string> seenImages;
    std::set<int> trophies;bool trophiesDirty=false;
    std::map<std::string,std::array<int,2>> imageLayouts,legacyImageLayouts;
    std::set<std::string> exactLayoutNames;
    std::optional<mahoyo::bitmap_geometry::Recipes> compositeGeometry;
    std::map<std::string,std::array<int,2>> imageSizes,imageOrigins;
    std::set<std::string> nativeImageResources;
    std::map<int,Layer> layers;
    struct PendingColor {int mode=0,rgb=0;};
    std::map<int,PendingColor> pendingColors;
    mahoyo::alpha_rule::Controller alphaRules;
    mahoyo::alpha_rule::Context alphaContext(const Layer& l){auto z=imageSize(l.image);bool child=false;for(const auto& p:layers)child|=p.second.parent==l.id;return {l.image,z[0],z[1],l.parent,l.blend,l.blendFlag,l.colorMode,l.colorValue,l.brightness,l.rgb,l.hasCrop,l.window[2]!=0||l.window[3]!=0,waves.contains(l.id),noise.contains(l.id),child};}
    void prepareAlphaRules(){alphaRules.prepare();for(const auto& p:layers)if(alphaRules.contains(p.first))mahoyo::alpha_rule::validate(alphaContext(p.second));}
    std::vector<Audio> audio;
    std::vector<Animation> fades;
    std::map<int,mahoyo::effects::Clip> effectClips;
    std::vector<Effect> effects;
    std::map<int,Effect> stoppedEffectBases;
    //Native event registry outlives completed jobs;27A snapshots this set.
    std::set<int> registeredEffectIds,pendingEffectRegistrations,pendingEffectRegistryDeletes;
    std::map<int,bool> pendingEffectPause;
    std::optional<int> effectWaitLayer;
    std::set<int> pendingEffectBaseEdits;
    std::map<int,Effect> effectBaseBeforePendingEdits;
    std::map<int,std::vector<std::function<void(Layer&)>>> effectPendingBaseEdits;
    mahoyo::effects::FrameOverrides frameOverrides;
    std::map<int,EffectCapture> pendingEffectCaptures;
    std::set<int> pendingEffectClears;
    std::vector<Motion> motions;
    std::map<int,PendingRelativeMotion> pendingRelativeMotions;
    std::set<int> unpreparedAbsoluteMotions;
    std::vector<Transform> transforms;
    std::map<int,float> pendingClockRotations;
    mahoyo::noise::Controller noise;
    std::set<int> pendingNoise,pendingEffectNoise;
    uint64_t noiseClockMs=0;
    int tickBootstrapMs=16;
    std::vector<AudioFade> audioFades;
    std::vector<PendingSound> pendingSounds;
    std::vector<PendingVolumeFade> pendingVolumeFades;
    std::vector<Shake> shakes;
    std::map<int,LayerPulse> layerPulses;
    std::map<int,PendingLayerPulse> pendingLayerPulses;
    std::map<int,mahoyo::effects::LayerShakeOffset> layerPulseOffsets;
    mahoyo::effects::LayerShakeCache layerPulseCache;
    uint64_t layerPulseSerial=0;
    std::optional<int> pulseWaitLayer;
    mahoyo::effects::GlobalShake globalShake;
    mahoyo::effects::GlobalShakeOffset globalShakeOffset;
    bool globalShakeClear=false;
    std::optional<mahoyo::effects::GlobalShakeRequest> globalShakeAfterClear;
    mahoyo::wave::Controller waves;
    mahoyo::screen_wave::Controller screenWave;
    bool waitingScreenWave=false;
    PreloadState preloads;
    bool preloadsDirty=false;
    std::string cachedPreloads="[]";
    uint32_t randomState=1;
    mahoyo::particles::Overlays particleOverlays;
    std::string mode="title",paragraph,error,notice,result,wait;
    bool fontCarryItalic=false,paragraphItalic=false;
    std::string presentedLayers="[]",presentedText="[]",presentedMovie="null",presentedScene="play",presentedModule,presentedScreenWave="null",presentedParticles="null";
    Transition transition;
    uint64_t transitionId=0;
    mahoyo::movie::Opening opening;
    uint64_t movieGeneration=0;
    mahoyo::text::Reveal reveal;
    int autoDelay=2400;
    mahoyo::effects::WindowFade windowFade;
    int windowMode=0;std::optional<int> pendingWindowMode;
    void commitWindow(){if(pendingWindowMode){windowMode=*pendingWindowMode;pendingWindowMode.reset();}windowFade.commit(tick,true);inputRegion.commit();}
    bool windowFadeBusy() const {return windowFade.active()||windowFade.pending().has_value()||pendingWindowMode.has_value();}
    mahoyo::input_region::Controller inputRegion;
    uint64_t inputJumpRelease=0,inputJumpPreviousPc=0;
    bool hidden=false,automatic=false,skipping=false,paused=false;
    int textLine=0,activeSlot=1;uint64_t version=1,audioId=0,tick=0,deadline=0,platformLast=0;bool havePlatformTime=false;
    uint64_t wavePlatformLast=0;bool wavePlatformEnabled=false,wavePlatformSeen=false;
    mahoyo::wait_clock::Clock waitClock;
    void captureWaitClock(){if(transition.active&&wait=="transitionReady"&&!transition.waitClockCaptured){transition.waitClockAge=waitClock.age(tick);transition.waitClockCaptured=true;}}
    uint64_t prepareWaitClock(){const auto captured=waitClock.age(tick);waitClock.prepared(captured,tick);return waitClock.age(tick);}
    explicit Runtime(const char* path,const char* edition=nullptr,const char* language=nullptr):root(path?path:"") {
        try {
            languageAssets.open(root,edition?edition:"",language?language:"");
            const auto messagesPath=root/"meta/language-messages.tsv";
            if(std::filesystem::exists(messagesPath)){
                if(std::filesystem::file_size(messagesPath)>4*1024*1024)throw mahoyo::vm::Error("Menu translation budget");
                std::ifstream file(messagesPath);std::string tag,key,locale,text;if(!std::getline(file,tag)||tag!="WOHMESSAGES1")throw mahoyo::vm::Error("Invalid menu translation header");size_t rows=0;
                while(file>>key){if(!(file>>locale>>std::quoted(text))||key.size()>128||key.rfind("SYSMES_",0)!=0||key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos||text.size()>4096||++rows>4096||(!mahoyo::language::supported("vanilla",locale)&&locale!="ko")||!menuMessages[key].emplace(locale,text).second)throw mahoyo::vm::Error("Invalid menu translation row");}
            }
            const auto dimensionsPath=root/"meta/image-dimensions.tsv";
            if(std::filesystem::exists(dimensionsPath)){if(std::filesystem::file_size(dimensionsPath)>4*1024*1024)throw mahoyo::vm::Error("Prepared image dimensions budget");std::ifstream file(dimensionsPath);imageSizes=mahoyo::image_header::dimensions(file);}
            auto raw=readBytes(root/std::filesystem::u8path(languageAssets.tablePath()));
            std::string text(raw.begin(),raw.end());std::istringstream stream(text);std::string line;
            while(std::getline(stream,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();table.push_back(mahoyo::text::normalizeCtdLine(line));}
            std::ifstream musicFile(root/"meta"/"music-map.tsv");
            while(std::getline(musicFile,line)){auto sep=line.find('\t');if(sep!=std::string::npos){auto name=line.substr(sep+1);if(!name.empty()&&name.back()=='\r')name.pop_back();music[std::stoi(line.substr(0,sep))]=lower(name);}}
            std::ifstream maskFile(root/"meta"/"mask-map.tsv");
            while(std::getline(maskFile,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);int id;std::string name,extra;if(!(row>>id>>name)||row>>extra||id<1||id>14||!safeName(name)||!masks.emplace(id,lower(name)).second)throw mahoyo::vm::Error("Invalid native mask map");}
            std::ifstream loopFile(root/"meta/audio-loop-map.tsv");
            while(std::getline(loopFile,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string name,extra;AudioLoop loop;
                if(!(row>>name>>loop.sampleCount>>loop.sampleRate>>loop.channels>>loop.wrapperLoop>>loop.start>>loop.end)||row>>extra||!safeName(name)||!loop.sampleCount||!loop.sampleRate||loop.sampleRate>768000||!loop.channels||loop.channels>8||!audioLoops.emplace(lower(name),loop).second)throw mahoyo::vm::Error("Invalid native audio loop map");}
            std::ifstream layoutFile(root/"meta"/"image-layout.tsv");
            while(std::getline(layoutFile,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string name,extra;int w,h;if(!(row>>name>>w>>h)||row>>extra||!safeName(name)||w<=0||h<=0||w>65535||h>65535||!imageLayouts.emplace(lower(name),std::array<int,2>{w,h}).second)throw mahoyo::vm::Error("Invalid image layout metadata");}
            std::ifstream originFile(root/"meta"/"image-origins.tsv");
            while(std::getline(originFile,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string name,extra;int x,y;if(!(row>>name>>x>>y)||row>>extra||!safeName(name)||!imageOrigins.emplace(lower(name),std::array<int,2>{x,y}).second)throw mahoyo::vm::Error("Invalid image origin metadata");}
            legacyImageLayouts=imageLayouts;
            const auto exactPath=root/"meta"/"native-exact-layouts.tsv";
            if(std::filesystem::exists(exactPath)){
                if(std::filesystem::file_size(exactPath)>2*1024*1024)throw mahoyo::vm::Error("Native layout metadata exceeds budget");
                std::ifstream file(exactPath);std::string tag,sha;size_t count;
                if(!(file>>tag>>sha>>count)||tag!="WOHEXACTLAYOUTS1"||sha!="1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"||count>20000)throw mahoyo::vm::Error("Invalid exact native layout header");
                for(size_t i=0;i<count;++i){std::string name;int w,h,x,y;if(!(file>>name>>w>>h>>x>>y)||!safeName(name)||w<=0||h<=0||w>65535||h>65535||!exactLayoutNames.insert(name).second)throw mahoyo::vm::Error("Invalid exact native image layout");imageLayouts[name]={w,h};imageOrigins[name]={x,y};}
                std::string trailing;if(file>>trailing)throw mahoyo::vm::Error("Trailing exact layout metadata");
            }
            const auto geometryPath=root/"meta"/"native-composite-geometry.tsv";
            if(std::filesystem::exists(geometryPath)){std::ifstream file(geometryPath);if(!file)throw mahoyo::vm::Error("Cannot read native bitmap geometry metadata");compositeGeometry=mahoyo::bitmap_geometry::readRecipes(file,mahoyo::bitmap_geometry::Steam11PE,mahoyo::bitmap_geometry::Steam11Authority);}
            nativeImageResources=readNativeImageResources(root/"meta"/"native-image-resources.tsv");
            effectClips=mahoyo::effects::load(root/"meta"/"effects.tsv");mahoyo::effects::loadWindows(effectClips,root/"meta"/"effects-window.tsv");mahoyo::effects::loadProperties(effectClips,root/"meta"/"effects-properties.tsv");
            std::filesystem::file_time_type latest=std::filesystem::file_time_type::min();
            for(int slot=1;slot<=12;++slot){std::error_code ec;auto stamp=std::filesystem::last_write_time(savePath(slot),ec);if(!ec&&stamp>latest){latest=stamp;activeSlot=slot;}}
            //The title has no native game VM. An empty temporary machine here
            //would overwrite existing common progress (or mask legacy globals
            //on the first slot import) when title() flushes the old session.
            loadSeenImages();loadTrophies();loadShared();
            title();
        }catch(const std::exception& e){error=e.what();}
    }
    std::vector<uint8_t> loadModule(const std::string& name) override {
        if(!safeName(name))throw mahoyo::vm::Error("Invalid module name");
        return readBytes(root/std::filesystem::u8path(languageAssets.path(lower(name))));
    }
    std::string resourceName(std::string name)const {
        name=lower(name);return languageAssets.localize(name,[&](const std::string& candidate){auto relative=mahoyo::language::defaultPath(candidate);return !relative.empty()&&std::filesystem::is_regular_file(root/std::filesystem::u8path(relative));});
    }
    std::string menuMessage(const std::string& key,const std::string& fallback)const{
        auto row=menuMessages.find(key);if(row==menuMessages.end())return fallback;auto value=row->second.find(languageAssets.current());return value==row->second.end()?fallback:value->second;
    }
    std::array<int,2> imageSize(std::string name) {
        if(name.find('.')==std::string::npos)name+=".mzp";
        if(!safeName(name))throw mahoyo::vm::Error("Invalid image name");
        name=resourceName(name);if(auto overlay=languageAssets.overlay(name);overlay&&overlay->size[0]>0)return overlay->size;auto cached=imageSizes.find(name);if(cached!=imageSizes.end())return cached->second;
        unsigned char header[32]{};size_t bytes=0;
        auto relative=languageAssets.path(name);
        {std::ifstream file(root/std::filesystem::u8path(relative),std::ios::binary);file.read(reinterpret_cast<char*>(header),sizeof(header));bytes=static_cast<size_t>(file.gcount());}
        auto size=mahoyo::image_header::size(header,bytes);
        if(size[0]>0) {
            imageSizes.emplace(name,size);return size;
        }
        if(lower(name)=="black.mzp"||lower(name)=="white.mzp")return {1920,1080};
        throw mahoyo::vm::Error("Missing or invalid prepared image: "+name);
    }
    std::array<int,2> intrinsicSize(const Layer& l){return l.image.empty()?std::array<int,2>{1920,1080}:imageSize(l.image);}
    std::string bitmapKey(const Layer& l)const{if(l.image.empty())return {};auto key=resourceName(l.image);if(key.find('.')==std::string::npos)key+=".mzp";return key;}
    bool bitmapLayoutPresent(const std::string& key)const{auto name=key;auto dot=name.rfind('.');if(dot!=std::string::npos)name.resize(dot);return imageLayouts.count(name)!=0;}
    std::optional<mahoyo::bitmap_geometry::Resize> bitmapResize(const Layer& l,std::optional<mahoyo::bitmap_geometry::Size> saved){
        const auto size=intrinsicSize(l);const auto key=bitmapKey(l);
        if(saved)if(auto resize=mahoyo::language::koreanCanvasResize(key,*saved,{size[0],size[1]}))return resize;
        // Korean single images can differ from the original canvas. Accept only
        // the two declared sizes, in either direction; arbitrary save geometry
        // must still follow the strict Native pair migration contract below.
        if(saved&&languageAssets.edition()=="korean"){
            const auto ko=languageAssets.koreanRecord(key);const auto original=imageSizes.find(key);
            if(ko&&ko->size[0]>0&&original!=imageSizes.end()){
                const mahoyo::bitmap_geometry::Size a{original->second[0],original->second[1]},b{ko->size[0],ko->size[1]},now{size[0],size[1]};
                if(!(a==b)&&(*saved==a||*saved==b)&&(now==a||now==b)&&!(*saved==now))return mahoyo::bitmap_geometry::Resize{*saved,now};
            }
        }
        return mahoyo::bitmap_geometry::plan(compositeGeometry?&*compositeGeometry:nullptr,key,saved,{size[0],size[1]},bitmapLayoutPresent(key));
    }
    std::array<int,2> legacyIntrinsicSize(const Layer& l){if(auto resize=bitmapResize(l,std::nullopt))return {resize->before.width,resize->before.height};return intrinsicSize(l);}
    void migrateBitmapLayer(Layer& l,std::optional<mahoyo::bitmap_geometry::Size> saved){if(auto resize=bitmapResize(l,saved))mahoyo::bitmap_geometry::apply(*resize,l.w,l.h,l.pivotX,l.pivotY);}
    void migrateBitmapEffect(Effect& e,const std::map<int,Layer>& scene){auto base=effectBase(scene.at(e.id),e);const auto old=mahoyo::bitmap_geometry::checkedSize(e.sourceW,e.sourceH);if(auto resize=bitmapResize(base,old)){mahoyo::bitmap_geometry::apply(*resize,e.w,e.h,e.pivotX,e.pivotY);e.sourceW=resize->after.width;e.sourceH=resize->after.height;}}
    bool nativeFullBitmapGeometryReviewed(const Layer& l){
        const auto key=bitmapKey(l);
        //Native table row2879 is a full SINGLE variant, independent of its paired base.
        if(key=="aok_n_06_02_07_40_00.mzp"){const auto size=intrinsicSize(l);return size[0]==1532&&size[1]==1526;}
        if(!compositeGeometry)return false;auto at=compositeGeometry->find(key);if(at==compositeGeometry->end())return false;
        const auto size=intrinsicSize(l);return at->second.primary==mahoyo::bitmap_geometry::Size{size[0],size[1]};
    }
    std::array<double,2> worldAnchor(const Layer& l){auto size=intrinsicSize(l);return {l.x+l.w*l.pivotX-l.drawOffsetX-size[0]*l.pivotX,l.y+l.h*l.pivotY-l.drawOffsetY-size[1]*l.pivotY};}
    void placeAt(Layer& l,double x,double y){auto size=intrinsicSize(l);l.x=x+l.drawOffsetX+size[0]*l.pivotX-l.w*l.pivotX;l.y=y+l.drawOffsetY+size[1]*l.pivotY-l.h*l.pivotY;}
    void setResourceOrigin(Layer& l,double nativeX,double nativeY){auto anchor=worldAnchor(l);auto size=intrinsicSize(l);l.pivotX=nativeX*3./size[0];l.pivotY=nativeY*3./size[1];placeAt(l,anchor[0],anchor[1]);}
    void setAxisScale(Layer& l,double x,double y){auto anchor=worldAnchor(l);auto size=intrinsicSize(l);l.axisScaleX=x;l.axisScaleY=y;l.w=size[0]*l.uniformScale*x;l.h=size[1]*l.uniformScale*y;placeAt(l,anchor[0],anchor[1]);}
    mahoyo::effects::Properties effectProperties(const Layer& l,double sourceW,double sourceH) const {
        mahoyo::effects::Properties p;
        p.originX=int32_t(std::lround(l.pivotX*sourceW/3.));p.originY=int32_t(std::lround(l.pivotY*sourceH/3.));
        p.blendMode=l.blend;p.colorMode=l.colorMode;p.rgb=uint32_t(l.rgb)&0xffffff;
        p.variantX=l.variantX;p.variantY=l.variantY;p.brightness=l.brightness;p.contrast=l.colorValue;p.priority=l.z;p.decimalMode=l.decimalMode;
        return p;
    }
    Layer effectBase(const Layer& live,const Effect& e){
        auto base=live;const auto& p=e.properties;base.variantX=p.variantX;base.variantY=p.variantY;
        if(!live.image.empty())base.image=variantName(base);
        base.x=e.x;base.y=e.y;base.w=e.w;base.h=e.h;base.alpha=e.alpha;base.rotation=e.rotation;
        base.pivotX=e.pivotX;base.pivotY=e.pivotY;base.drawOffsetX=e.drawOffsetX;base.drawOffsetY=e.drawOffsetY;
        base.uniformScale=e.uniformScale;base.axisScaleX=e.axisScaleX;base.axisScaleY=e.axisScaleY;base.window=e.window;
        base.z=p.priority;base.blend=p.blendMode;base.colorMode=p.colorMode;base.rgb=int(p.rgb);base.brightness=p.brightness;base.colorValue=p.contrast;base.decimalMode=p.decimalMode;
        return base;
    }
    void captureEffectBase(Effect& e,const Layer& base){
        auto size=intrinsicSize(base);e.x=base.x;e.y=base.y;e.w=base.w;e.h=base.h;e.alpha=base.alpha;e.rotation=base.rotation;
        e.sourceW=size[0];e.sourceH=size[1];e.pivotX=base.pivotX;e.pivotY=base.pivotY;e.drawOffsetX=base.drawOffsetX;e.drawOffsetY=base.drawOffsetY;
        e.uniformScale=base.uniformScale;e.axisScaleX=base.axisScaleX;e.axisScaleY=base.axisScaleY;e.window=base.window;e.properties=effectProperties(base,size[0],size[1]);
    }
    void editEffectBase(int id,const std::function<void(Layer&)>& edit){
        for(auto& e:effects)if(e.id==id){auto base=effectBase(layers.at(id),e);edit(base);captureEffectBase(e,base);}
        auto stopped=stoppedEffectBases.find(id);if(stopped!=stoppedEffectBases.end()){auto base=effectBase(layers.at(id),stopped->second);edit(base);captureEffectBase(stopped->second,base);}
    }
    void editPendingEffectBase(int id,const std::function<void(Layer&)>& edit){
        auto active=std::find_if(effects.begin(),effects.end(),[&](const Effect& e){return e.id==id;});
        if(active!=effects.end()){
            pendingEffectBaseEdits.insert(id);effectBaseBeforePendingEdits.emplace(id,*active);
            effectPendingBaseEdits[id].push_back(edit);
        }
        editEffectBase(id,edit);
    }
    void applyEffectSample(Layer& l,const Effect& e,uint64_t t,bool terminal=false){
        using namespace mahoyo::effects;const auto& clip=effectClips.at(e.clip);auto anchor=worldAnchor(l);const auto time=prepareTime(clip,t);
        auto currentSize=intrinsicSize(l);auto initial=effectProperties(l,currentSize[0],currentSize[1]);
        if(clip.propertiesMetadata){
            auto p=samplePropertiesPrepared(clip,time,initial);
            if(isSelected(clip,VariantX)||isSelected(clip,VariantY))setEffectVariant(l,isSelected(clip,VariantX)?p.variantX:l.variantX,isSelected(clip,VariantY)?p.variantY:l.variantY);
            if(isSelected(clip,ColorMode)){if(!supportedColorMode(p.colorMode))throw mahoyo::vm::Error("Unsupported effect color mode "+std::to_string(p.colorMode));l.colorMode=p.colorMode;l.rgb=int(p.rgb);}
            if(isSelected(clip,BlendMode)){if(!supportedRootBlendMode(p.blendMode)||(l.parent>=0&&(!supportedWindowChildBlend(p.blendMode,l.blendFlag))))throw mahoyo::vm::Error("Unsupported effect blend mode "+std::to_string(p.blendMode));l.blend=p.blendMode;}
            if(isSelected(clip,Brightness))l.brightness=p.brightness;if(isSelected(clip,Contrast))l.colorValue=p.contrast;
            if(isSelected(clip,AuxResourceAndScale)){mahoyo::noise::decode(p.decimalMode);if(l.decimalMode!=p.decimalMode){l.decimalMode=p.decimalMode;if(!terminal)pendingEffectNoise.insert(l.id);}}
            if(isSelected(clip,Priority)&&(terminal||(l.z&&p.priority)))l.z=p.priority;
            auto size=intrinsicSize(l);if(isSelected(clip,OriginX))l.pivotX=p.originX*3./size[0];if(isSelected(clip,OriginY))l.pivotY=p.originY*3./size[1];
        }
        double x=isSelected(clip,X)?samplePrepared(clip,time,X,anchor[0]/4.,&frameOverrides)*4.:anchor[0];
        double y=isSelected(clip,Y)?samplePrepared(clip,time,Y,anchor[1]/4.,&frameOverrides)*4.:anchor[1];
        if(isSelected(clip,ScaleX))l.axisScaleX=samplePrepared(clip,time,ScaleX,l.axisScaleX*1000,&frameOverrides)/1000.;
        if(isSelected(clip,ScaleY))l.axisScaleY=samplePrepared(clip,time,ScaleY,l.axisScaleY*1000,&frameOverrides)/1000.;
        auto size=intrinsicSize(l);l.w=size[0]*l.uniformScale*l.axisScaleX;l.h=size[1]*l.uniformScale*l.axisScaleY;placeAt(l,x,y);
        if(isSelected(clip,Alpha))l.alpha=std::clamp(samplePrepared(clip,time,Alpha,l.alpha*255,&frameOverrides)/255.,0.,1.);
        if(isSelected(clip,Rotation))l.rotation=nativeAngle(samplePrepared(clip,time,Rotation,l.rotation*100,&frameOverrides));
        if(clip.windowMetadata){auto update=[&](Field field,int index){if(isSelected(clip,field))l.window[size_t(index)]=int32_t(samplePrepared(clip,time,field,l.window[size_t(index)],&frameOverrides));};
            update(WindowX,0);update(WindowY,1);update(WindowWidth,2);update(WindowHeight,3);update(WindowColor,4);update(WindowSoft,5);validateWindow(l.window);}
        validateRoot18Context(l);if(l.blend==18&&waves.contains(l.id))throw mahoyo::vm::Error("Unsupported local wave/root blend18 combination");
    }
    void applyEffect(const Effect& e){if(!e.paused)applyEffectSample(layers.at(e.id),e,effectRawElapsed(e,tick));}
    bool effectFinished(const Effect& e) const {const auto& clip=effectClips.at(e.clip);return !e.paused&&!clip.loopStart&&effectRawElapsed(e,tick)>=uint64_t(clip.duration)+uint64_t(clip.timeRemap);}
    bool finiteEffectWaitBusy(int id) const {for(const auto& e:effects)if((id<0||id==e.id)&&!effectClips.at(e.clip).loopStart&&!effectFinished(e))return true;return false;}
    void queryEffectRegistry(int id){
        for(auto it=registeredEffectIds.begin();it!=registeredEffectIds.end();)if((id<0||id==*it)&&std::none_of(effects.begin(),effects.end(),[&](const Effect& e){return e.id==*it&&!effectFinished(e);}))it=registeredEffectIds.erase(it);else ++it;
        effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect& e){return (id<0||id==e.id)&&effectFinished(e);}),effects.end());
    }
    void resetEffectRegistry(){registeredEffectIds.clear();pendingEffectRegistrations.clear();pendingEffectRegistryDeletes.clear();pendingEffectPause.clear();effectWaitLayer.reset();}
    void applyEffects(){for(const auto& effect:effects)applyEffect(effect);refreshEffectNoise();}
    std::filesystem::path sharedPath() const {return root.parent_path()/"savedata/shared.whp";}
    void loadShared(){auto value=mahoyo::shared::read(sharedPath());if(value){sharedState=std::move(*value);sharedLoaded=true;
        if(onlyInitialTitleCommon()){pendingFrontendFlags.insert(321);sharedState.generation=0;sharedLoaded=false;}}}
    bool flushShared(){
        try{
            if(machine){for(auto flag:pendingFrontendFlags)if(flag<machine->sharedFlags().size())machine->setFlag(flag,true,true);pendingFrontendFlags.clear();
                const auto& vars=machine->sharedVariables();const auto& flags=machine->sharedFlags();
                if(!sharedLoaded||sharedState.variables!=vars||sharedState.flags!=flags){
                    if(sharedState.generation==UINT64_MAX)throw mahoyo::save::Error("Shared generation exhausted");
                    ++sharedState.generation;sharedState.variables=vars;sharedState.flags=flags;sharedLoaded=true;sharedWritePending=true;}}
            if(sharedLoaded&&sharedWritePending){mahoyo::shared::write(sharedPath(),sharedState);sharedWritePending=false;}return true;
        }catch(const std::exception& e){notice=std::string("Shared progress persistence: ")+e.what();return false;}
    }
    ~Runtime(){try{flushShared();}catch(...){}}
    const std::vector<uint8_t>& commonFlags() const {return machine?machine->sharedFlags():sharedState.flags;}
    bool onlyInitialTitleCommon() const {
        // Code24 could persist this exact empty-base record before a legacy
        // import. It contains no game progress to prefer over that import.
        if(sharedState.generation!=1||sharedState.flags.size()<=321||!sharedState.flags[321])return false;
        if(std::any_of(sharedState.variables.begin(),sharedState.variables.end(),[](int32_t value){return value!=0;}))return false;
        for(size_t i=0;i<sharedState.flags.size();++i)if(i!=321&&sharedState.flags[i])return false;
        return true;
    }
    void setCommonFlag(size_t flag) {
        if(machine){if(flag<machine->sharedFlags().size())machine->setFlag(flag,true,true);return;}
        if(!sharedLoaded){
            // A title cue is not a base for migrating legacy VM globals. Keep
            // its exact flags until a real VM/import provides that base.
            sharedState.variables.resize(4096);sharedState.flags.resize(16384);
            if(flag<sharedState.flags.size()){sharedState.flags[flag]=1;pendingFrontendFlags.insert(flag);}return;
        }
        if(flag>=sharedState.flags.size()||sharedState.flags[flag])return;
        if(sharedState.generation==UINT64_MAX)throw mahoyo::save::Error("Shared generation exhausted");
        sharedState.flags[flag]=1;++sharedState.generation;sharedWritePending=true;
    }
    bool completedAll() const {
        for(size_t flag:{128,65,73,72,206,129,130,131,132,133,134})if(flag>=commonFlags().size()||!commonFlags()[flag])return false;
        return true;
    }
    void titleMusic(const std::optional<Audio>& previous) {
        const bool complete=completedAll();const std::string name=complete?"m42.hw":"x_m64.hw";
        // Synthetic/minimal packages can omit frontend media; shipping catalog packages require its metadata.
        if(!audioLoops.count(name)){if(std::filesystem::exists(root/"meta/archive-catalog.tsv"))notice="Title music metadata is missing: "+name;return;}
        if(previous&&previous->name==name&&previous->loop)audio.push_back(*previous);
        else{sound(name,"bgm",true);audio.back().volume=1;audio.back().pan=64;audio.back().oldFadeOutMs=50;}
        setCommonFlag(complete?298:321);flushShared();
    }
    const mahoyo::archive::Catalog& catalog() {
        if(!archiveCatalog)archiveCatalog=mahoyo::archive::Catalog::load(root/"meta/archive-catalog.tsv");return *archiveCatalog;
    }
    std::string archiveJson() const {
        if(mode!="archive"||!archiveCatalog)return "null";
        std::ostringstream out;out<<"{\"id\":"<<archiveGeneration<<",\"selected\":"<<archiveSelected<<",\"checkpoint\":"<<archiveCheckpoint<<",\"entries\":[";bool first=true;
        for(const auto& e:archiveCatalog->entries)if(mahoyo::archive::Catalog::eligible(e,commonFlags())){
            if(!first)out<<',';first=false;out<<"{\"id\":"<<e.story<<",\"group\":"<<e.group<<",\"label\":"<<quote(menuMessage("SYSMES_ARCHIVE_"+std::string(e.story<=10?"00":"0")+std::to_string(e.story-1),e.label))<<",\"preview\":"<<quote(resourceName(e.preview))<<",\"header\":"<<quote(resourceName(archiveCatalog->headers.at(e.group)))<<",\"read\":"<<(mahoyo::archive::Catalog::read(e,commonFlags())?"true":"false")<<'}';}
        return out.str()+"]}";
    }
    void openArchive(int selected,int checkpoint) {
        catalog();if(checkpoint<0||checkpoint>13)throw mahoyo::vm::Error("Invalid Archive checkpoint");
        title();layers.clear();clearHistory();automatic=false;skipping=false;mode="archive";wait="archive";archiveSelected=selected;archiveCheckpoint=checkpoint;archiveReplay=0;++archiveGeneration;
        if(!sharedLoaded){sharedState.variables.resize(4096);sharedState.flags.resize(16384);sharedState.variables[9]=checkpoint;}
        else{if(sharedState.generation==UINT64_MAX)throw mahoyo::save::Error("Shared generation exhausted");sharedState.variables[9]=checkpoint;++sharedState.generation;sharedWritePending=true;flushShared();}++version;
    }
    void startArchive(int story) {
        const auto* e=catalog().find(story);if(!e||!mahoyo::archive::Catalog::eligible(*e,commonFlags()))throw mahoyo::vm::Error("Archive entry is locked");
        const int chapter=e->chapter,checkpoint=archiveCheckpoint;
        // The native bootstrap owns local500/459/24..26 and its original F3 dispatch.
        play("archive.chs",false);machine->setVariable(8,chapter,true);machine->setVariable(9,checkpoint,true);archiveReplay=story;archiveSelected=story;archiveCheckpoint=checkpoint;pump();
    }
    bool safeSceneSave() const {return particleOverlays.safe()&&pendingColors.empty()&&inputRegion.safe()&&alphaRules.empty()&&choices.safe()&&pendingNoise.empty()&&pendingEffectNoise.empty()&&pendingClockRotations.empty()&&pendingRelativeMotions.empty()&&waves.safe()&&screenWave.safe()&&!opening.active()&&!globalShakeBusy()&&!layerShakeBusy()&&!windowFadeBusy()&&pendingEffectCaptures.empty()&&pendingEffectClears.empty()&&pendingEffectBaseEdits.empty()&&pendingEffectRegistrations.empty()&&pendingEffectRegistryDeletes.empty()&&pendingEffectPause.empty()&&fades.empty()&&motions.empty()&&transforms.empty()&&audioFades.empty()&&pendingSounds.empty()&&pendingVolumeFades.empty()&&shakes.empty();}
    bool canSaveNow() const {
        if(mode=="play"&&wait=="input")return safeSceneSave();
        if(mode=="teatime"&&wait=="teatime"&&intermission.active()){auto phase=intermission.state()->phase;return (phase==mahoyo::teatime::Phase::Ready||phase==mahoyo::teatime::Phase::Saving)&&safeSceneSave();}
        return false;
    }
#include "extra_frontends.inc"
#include "choice_runtime.inc"
    std::string intermissionJson() const {
        if(!intermission.active())return "null";const auto& state=*intermission.state();using mahoyo::teatime::Phase;
        const char* phase=state.phase==Phase::Opening?"opening":state.phase==Phase::Ready?"ready":state.phase==Phase::Saving?"saving":"closing";
        std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"kind\":\"teatime\",\"id\":"<<state.id<<",\"revision\":"<<state.revision<<",\"selector\":"<<state.selector<<",\"theme\":"<<quote(intermission.isBlack(state.selector)?"black":"white")<<",\"phase\":"<<quote(phase)<<",\"alpha\":"<<intermission.alphaFixed16(tick)/65536.<<",\"initialFocusAction\":\"save\",\"returnAction\":\"save-return\",\"actions\":[";
        const char* ids[]={"archive","save","title","next"};const char* labels[]={"書庫","セーブ","タイトル","次へ"};
        for(int i=0;i<4;++i){if(i)out<<',';bool enabled=state.phase==Phase::Ready&&(i!=1||safeSceneSave());out<<"{\"id\":"<<quote(ids[i])<<",\"label\":"<<quote(labels[i])<<",\"enabled\":"<<(enabled?"true":"false")<<",\"alpha\":"<<intermission.buttonAlphaFixed16(i,tick)/65536.<<'}';}
        out<<"]}";return out.str();
    }
    void validateWindow(std::array<int32_t,6>& values) const {
        if(!values[2]||!values[3]){values={};return;}
        if(uint32_t(values[2])>=4096||uint32_t(values[3])>=2048||uint32_t(values[5])>=4096)throw mahoyo::vm::Error("Unsupported native window dimensions");
    }
    void validateRoot18Context(const Layer& layer) const {if(layer.blend==18&&((layer.parent>=0&&layer.blendFlag!=0)||layer.window[2]||layer.window[3]||layer.decimalMode%10!=0))throw mahoyo::vm::Error("Unsupported root blend18 window/parent/noise context");}
    void validateParents(const std::map<int,Layer>& values) const {
        for(const auto& pair:values){validateRoot18Context(pair.second);std::set<int> seen;int parent=pair.second.parent;
            if(parent>=0){const auto& child=pair.second;if(!supportedWindowChildBlend(child.blend,child.blendFlag))throw mahoyo::vm::Error("Unsupported native window child blend mode");if(child.blend==15&&child.blendFlag==1)throw mahoyo::vm::Error("Unsupported native window child special blend flag1");
                if(strictWindowChildContext(child.blend)){auto owner=values.find(parent);if(owner==values.end())throw mahoyo::vm::Error("Invalid native parent layer graph");if(child.window[2]||child.window[3]||!owner->second.window[2]||!owner->second.window[3]||owner->second.parent>=0)throw mahoyo::vm::Error("Unsupported reviewed child window group context");if(child.blend==7&&(child.hasCrop||owner->second.hasCrop||child.decimalMode%10!=0||owner->second.decimalMode%10!=0))throw mahoyo::vm::Error("Unsupported child7 cropped/noise context");for(const auto& descendant:values)if(descendant.second.parent==child.id)throw mahoyo::vm::Error("Unsupported nested reviewed child window group");}}
            while(parent>=0){if(!values.count(parent)||parent==pair.first||!seen.insert(parent).second)throw mahoyo::vm::Error("Invalid native parent layer graph");parent=values.at(parent).parent;}}
    }
    void commitRelativeMotions(){
        //102350 stores f32 relative deltas;146570 resolves the current point
        //only at prepare. The GL acknowledgement freezes this logical tick.
        for(const auto& pair:pendingRelativeMotions){
            auto found=layers.find(pair.first);if(found==layers.end())continue;
            auto& l=found->second;const auto& p=pair.second;auto anchor=worldAnchor(l);
            float x=float(anchor[0]/3.),y=float(anchor[1]/3.);
            double endX=double(float(x+p.x))*3.,endY=double(float(y+p.y))*3.;
            motions.erase(std::remove_if(motions.begin(),motions.end(),[&](const Motion& m){return m.id==pair.first;}),motions.end());
            if(p.duration>0){placeAt(l,double(x)*3.,double(y)*3.);motions.push_back(Motion{pair.first,double(x)*3.,double(y)*3.,endX,endY,tick,tick+uint64_t(p.duration),p.curve,l.pivotX,l.pivotY});}
            else{placeAt(l,endX,endY);editPendingEffectBase(pair.first,[=](Layer& base){placeAt(base,endX,endY);});}
        }
        pendingRelativeMotions.clear();unpreparedAbsoluteMotions.clear();
    }
    void commitParticles(){
        auto candidate=particleOverlays;candidate.commit();const auto frame=candidate.frame();
        const std::array<std::array<int,2>,3> fallSizes{{{{13512,187}},{{4500,112}},{{3600,150}}}};
        for(size_t i=0;i<frame.fallNote.resources.size();++i)if(imageSize(frame.fallNote.resources[i])!=fallSizes[i])throw mahoyo::vm::Error("Unsupported FallNote bitmap geometry");
        if(!frame.focusLine.resources.empty()&&imageSize(frame.focusLine.resources.front())!=std::array<int,2>{{960,2}})throw mahoyo::vm::Error("Unsupported FocusLine bitmap geometry");
        particleOverlays=std::move(candidate);
    }
    void commitEffectCaptures(){
        if(!pendingEffectCaptures.empty())alphaRules.rejectCapture();
        validateParents(layers);
        commitRelativeMotions();
        //169D50 commits finite effects to D0, while cancelled142460 leaves
        //live graphics alone. Loop cancellation retains the preceding D0 base.
        for(const auto& e:effects)if(pendingEffectClears.count(e.id)){
            auto before=effectBaseBeforePendingEdits.find(e.id);
            auto base=effectBase(layers.at(e.id),before==effectBaseBeforePendingEdits.end()?e:before->second);
            const auto& clip=effectClips.at(e.clip);
            if(!clip.loopStart)applyEffectSample(base,e,uint64_t(clip.duration)+1,true);
            auto edits=effectPendingBaseEdits.find(e.id);if(edits!=effectPendingBaseEdits.end())for(const auto& edit:edits->second)edit(base);
            auto retained=e;retained.begin=0;retained.paused=false;retained.pausedAt=0;captureEffectBase(retained,base);stoppedEffectBases.insert_or_assign(e.id,std::move(retained));
        }
        effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect& e){return pendingEffectClears.count(e.id)!=0;}),effects.end());
        pendingEffectClears.clear();
        for(const auto& pair:pendingEffectCaptures){const auto& request=pair.second;
            //Image requests currently replace CPU graphics immediately. Reject
            //an unknown old-object timeline instead of guessing across a wait.
            if(request.queuedAt!=tick)throw mahoyo::vm::Error("Unsupported effect capture across an asynchronous wait");
            const auto& clip=effectClips.at(request.clip);const auto& l=request.layer;
            if(!clip.windowMetadata||clip.frames.empty()||clip.frames.front().globalIndex<0)throw mahoyo::vm::Error("Missing prepared native effect window metadata");
            if(l.parent>=0)throw mahoyo::vm::Error("Unsupported effect capture from a parented image");
            if(l.targetOffsetX||l.targetOffsetY)throw mahoyo::vm::Error("Unsupported effect capture with target-pixel offsets");
            auto output=l;applyOutputShakes(output);
            auto narrow=[](float value){auto n=int64_t(std::round(value));uint16_t bits=uint16_t(n);return bits<32768?int32_t(bits):int32_t(bits)-65536;};
            auto anchor=worldAnchor(output);float x=float(anchor[0]/3.),y=float(anchor[1]/3.);
            auto fixed=int32_t(std::clamp(output.alpha,0.,1.)*65536.);
            mahoyo::effects::FrameOverride captured;captured.x=narrow(float(float(x*480.f)/640.f));captured.y=narrow(float(float(y*360.f)/480.f));
            captured.alpha=int32_t(float(float(fixed)*255.f)*(1.f/65536.f));
            for(int i=0;i<4;++i){uint16_t bits=uint16_t(l.window[size_t(i)]);captured.window[size_t(i)]=bits<32768?int32_t(bits):int32_t(bits)-65536;}
            frameOverrides.insert_or_assign(clip.frames.front().globalIndex,captured);
        }
        pendingEffectCaptures.clear();pendingEffectBaseEdits.clear();effectBaseBeforePendingEdits.clear();effectPendingBaseEdits.clear();
        for(int id:pendingEffectRegistryDeletes)registeredEffectIds.erase(id);
        for(int id:pendingEffectRegistrations)if(layers.count(id)&&std::any_of(effects.begin(),effects.end(),[&](const Effect& e){return e.id==id;}))registeredEffectIds.insert(id);
        for(const auto& request:pendingEffectPause)for(auto& e:effects)if(e.id==request.first)setEffectPaused(e,request.second,tick);
        pendingEffectRegistrations.clear();pendingEffectRegistryDeletes.clear();pendingEffectPause.clear();applyEffects();commitParticles();
    }
    void cancelLayer(int id,bool clearQueuedShake=true) {
        //CPU replacement is early; native registration is removed at prepare.
        if(id<0){pendingEffectRegistryDeletes.insert(registeredEffectIds.begin(),registeredEffectIds.end());pendingEffectRegistrations.clear();pendingEffectPause.clear();}
        else{if(registeredEffectIds.count(id))pendingEffectRegistryDeletes.insert(id);pendingEffectRegistrations.erase(id);pendingEffectPause.erase(id);}
        if(id<0)pendingColors.clear();else pendingColors.erase(id);
        alphaRules.remove(id);
        if(id<0){pendingRelativeMotions.clear();unpreparedAbsoluteMotions.clear();}else{pendingRelativeMotions.erase(id);unpreparedAbsoluteMotions.erase(id);}
        if(id<0)pendingEffectClears.clear();else pendingEffectClears.erase(id);
        if(id<0){stoppedEffectBases.clear();effectBaseBeforePendingEdits.clear();effectPendingBaseEdits.clear();}
        else{stoppedEffectBases.erase(id);effectBaseBeforePendingEdits.erase(id);effectPendingBaseEdits.erase(id);}
        if(id<0){noise.reset();pendingNoise.clear();pendingEffectNoise.clear();noiseClockMs=0;}else{pendingEffectNoise.erase(id);if(clearQueuedShake){noise.remove(id);pendingNoise.erase(id);}}
        if(id<0)waves.reset();else waves.remove(id,clearQueuedShake);
        if(id<0)pendingClockRotations.clear();else if(clearQueuedShake)pendingClockRotations.erase(id);
        if(id<0)pendingEffectBaseEdits.clear();else pendingEffectBaseEdits.erase(id);
        fades.erase(std::remove_if(fades.begin(),fades.end(),[&](const Animation&a){return id<0||a.id==id;}),fades.end());
        motions.erase(std::remove_if(motions.begin(),motions.end(),[&](const Motion&a){return id<0||a.id==id;}),motions.end());
        effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect&a){return id<0||a.id==id;}),effects.end());
        transforms.erase(std::remove_if(transforms.begin(),transforms.end(),[&](const Transform&a){return id<0||a.id==id;}),transforms.end());
        if(id<0)shakes.clear();else if(id<64){for(auto& s:shakes)s.mask&=~(uint64_t(1)<<id);shakes.erase(std::remove_if(shakes.begin(),shakes.end(),[](const Shake&s){return !s.mask;}),shakes.end());}
        if(id<0){layerPulses.clear();layerPulseOffsets.clear();layerPulseCache={};layerPulseSerial=0;pulseWaitLayer.reset();if(clearQueuedShake)pendingLayerPulses.clear();}
        else{layerPulses.erase(id);layerPulseOffsets.erase(id);if(clearQueuedShake)pendingLayerPulses.erase(id);}
    }
    std::string imageLayoutKey(const Layer& l) const {
        auto key=bitmapKey(l);if(key.size()>=4)key.resize(key.size()-4);if(imageLayouts.count(key))return key;
        key=lower(l.baseImage.empty()?l.image:l.baseImage);auto dot=key.rfind('.');if(dot!=std::string::npos)key.resize(dot);return key;
    }
    void migrateExactLayout(Layer& l){
        auto resolved=bitmapKey(l);if(resolved.size()<4)return;resolved.resize(resolved.size()-4);
        if(!exactLayoutNames.count(resolved)||!(l.variantX||l.variantY))return;
        auto base=lower(l.baseImage);auto dot=base.rfind('.');if(dot!=std::string::npos)base.resize(dot);
        auto old=legacyImageLayouts.find(base),next=imageLayouts.find(resolved);if(old==legacyImageLayouts.end()||next==imageLayouts.end()||resolved==base)return;
        auto anchor=worldAnchor(l);l.pivotX*=double(next->second[0])/old->second[0];l.pivotY*=double(next->second[1])/old->second[1];placeAt(l,anchor[0],anchor[1]);
    }
    std::array<double,2> anchorExtent(const Layer& l,const std::array<int,2>* savedSize=nullptr) {
        if(l.image.empty())return {1,1};
        auto name=imageLayoutKey(l);
        auto found=imageLayouts.find(name);
        if(found==imageLayouts.end()&&name.size()>6&&name[name.size()-6]=='_'&&name[name.size()-3]=='_')found=imageLayouts.find(name.substr(0,name.size()-6));
        if(found==imageLayouts.end())return {1,1};
        auto size=savedSize?*savedSize:imageSize(l.image);return {found->second[0]*3./size[0],found->second[1]*3./size[1]};
    }
    std::array<double,2> imagePivot(const Layer& l,const std::string& anchor) {
        auto extent=anchorExtent(l);
        auto size=l.image.empty()?std::array<int,2>{1920,1080}:imageSize(l.image);
        //The native CC path halves integer layout dimensions before turning
        //them into a float origin. Retain the old generic fallback for images
        //that have no native resource record (for example synthetic fixtures).
        double halfX=extent[0]*.5,halfY=extent[1]*.5;
        auto name=imageLayoutKey(l);
        auto found=imageLayouts.find(name);if(found==imageLayouts.end()&&name.size()>6&&name[name.size()-6]=='_'&&name[name.size()-3]=='_')found=imageLayouts.find(name.substr(0,name.size()-6));
        if(found!=imageLayouts.end()){halfX=(found->second[0]/2)*3./size[0];halfY=(found->second[1]/2)*3./size[1];}
        return {anchor.size()==2&&anchor[0]=='L'?0:anchor.size()==2&&anchor[0]=='R'?extent[0]:halfX,anchor.size()==2&&anchor[1]=='T'?0:anchor.size()==2&&anchor[1]=='B'?extent[1]:halfY};
    }
    std::string variantName(const Layer& l) {
        auto name=l.baseImage.empty()?l.image:l.baseImage;
        if(name.find('.')==std::string::npos)name+=".mzp";
        if(l.variantX||l.variantY){auto at=name.rfind('.');std::ostringstream suffix;suffix<<'_'<<std::setfill('0')<<std::setw(2)<<l.variantX<<'_'<<std::setw(2)<<l.variantY;name.insert(at,suffix.str());}
        return lower(name);
    }
    void setVariant(Layer& l,int x,int y) {
        if(l.variantX==x&&l.variantY==y)return;
        if(l.baseImage.empty())l.baseImage=l.image;
        auto before=imageSize(l.image);auto oldExtent=anchorExtent(l);
        auto anchor=worldAnchor(l);double ax=anchor[0],ay=anchor[1];
        Layer candidate=l;candidate.variantX=x;candidate.variantY=y;
        auto name=variantName(candidate);auto after=imageSize(name);
        candidate.image=std::move(name);auto extent=anchorExtent(candidate);
        candidate.w*=double(after[0])/before[0];candidate.h*=double(after[1])/before[1];
        candidate.pivotX*=extent[0]/oldExtent[0];candidate.pivotY*=extent[1]/oldExtent[1];
        placeAt(candidate,ax,ay);
        l=std::move(candidate);
    }
    void setEffectVariant(Layer& l,int x,int y) {
        if(l.variantX==x&&l.variantY==y)return;
        auto base=l.baseImage.empty()?l.image:l.baseImage;if(base.find('.')==std::string::npos)base+=".mzp";
        Layer candidate=l;candidate.variantX=x;candidate.variantY=y;
        // Native143BD0/142460 retain the old resource when its lookup fails; other effect fields still advance.
        if(x>=0&&y>=0&&x<=32767&&y<=32767&&nativeImageResources.count(lower(base))&&!nativeImageResources.count(variantName(candidate)))return;
        setVariant(l,x,y);
    }
    std::array<int,2> defaultOrigin(const Layer& l){
        auto name=imageLayoutKey(l);
        auto layout=imageLayouts.find(name);
        if(layout==imageLayouts.end()&&name.size()>6&&name[name.size()-6]=='_'&&name[name.size()-3]=='_'){auto base=name.substr(0,name.size()-6);if(imageLayouts.count(base)){name=base;layout=imageLayouts.find(base);}}
        auto origin=imageOrigins.find(name);if(origin!=imageOrigins.end())return origin->second;
        //The fingerprint-pinned native table has origin W/2,H/2 in every
        //named row. This fallback keeps earlier prepared packages readable.
        if(layout!=imageLayouts.end())return {layout->second[0]/2,layout->second[1]/2};
        throw mahoyo::vm::Error("Missing prepared native clone image origin: "+name);
    }
    Layer cloneSource(int id){
        alphaRules.rejectCapture();
        if(pendingClockRotations.count(id))throw mahoyo::vm::Error("Unsupported clone before pending clock rotation prepare");
        if(pendingEffectBaseEdits.count(id))throw mahoyo::vm::Error("Unsupported clone of pending fields over an active native effect");
        auto result=layers.at(id);auto stopped=stoppedEffectBases.find(id);if(stopped!=stoppedEffectBases.end())result=effectBase(result,stopped->second);
        for(const auto& e:effects)if(e.id==id)result=effectBase(result,e);
        for(const auto& a:fades)if(a.id==id)result.alpha=a.to;
        for(const auto& m:motions)if(m.id==id)placeAt(result,m.x1,m.y1);
        for(const auto& a:transforms)if(a.id==id){if(a.mask==2){setAxisScale(result,a.toX,a.toY);result.matrixMode=a.matrixMode;}else result.rotation=a.toX;}
        auto relative=pendingRelativeMotions.find(id);if(relative!=pendingRelativeMotions.end()){auto anchor=worldAnchor(result);placeAt(result,double(float(float(anchor[0]/3.)+relative->second.x))*3.,double(float(float(anchor[1]/3.)+relative->second.y))*3.);}
        for(const auto& e:effects)if(e.id==id){
            applyEffectSample(result,e,uint64_t(effectClips.at(e.clip).duration)+1,true);
        }
        if(pendingEffectClears.count(id)){auto edits=effectPendingBaseEdits.find(id);if(edits!=effectPendingBaseEdits.end())for(const auto& edit:edits->second)edit(result);}
        return result;
    }
    bool reviewedC9Clip6666() const {
        auto it=effectClips.find(6666);if(it==effectClips.end())return false;const auto& c=it->second;
        if(c.nativeId!=6666||!c.windowMetadata||!c.propertiesMetadata||c.descriptor!=0x1e944431||c.duration!=4000||c.loopStart||c.timeRemap||c.frames.size()!=2)return false;
        const auto& a=c.frames[0];const auto& b=c.frames[1];
        const std::array<int32_t,13> p0{-32767,-32767,17,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,9530};
        std::array<int32_t,13> p1;p1.fill(-32767);
        return a.time==0&&b.time==4000&&a.globalIndex==22566&&b.globalIndex==22567&&a.curve==2&&a.mode==0&&b.curve==0&&b.mode==-32767
            &&a.values==std::array<int32_t,6>{242,137,0,-32767,-32767,-100001}&&b.values==std::array<int32_t,6>{241,-32767,160,-32767,-32767,-100001}
            &&a.window==std::array<int32_t,6>{864,0,191,1080,0,94}&&b.window==std::array<int32_t,6>{806,0,308,1080,-32767,-32767}
            &&a.properties==p0&&b.properties==p1;
    }
    std::array<int32_t,6> c9Window6666(uint64_t elapsed) const {
        const auto& c=effectClips.at(6666);std::array<int32_t,6> result{864,0,191,1080,0,94};
        for(size_t n=0;n<result.size();++n)result[n]=int32_t(mahoyo::effects::sample(c,elapsed,mahoyo::effects::Field(mahoyo::effects::WindowX+n),result[n]));
        return result;
    }
    bool reviewedC9Rect(const Layer& l) const {
        if(l.blend==22&&l.window==std::array<int32_t,6>{864,0,191,1080,0,94})return true;
        if(l.id!=7||l.blend!=17||!reviewedC9Clip6666())return false;
        const auto active=std::find_if(effects.begin(),effects.end(),[&](const Effect& e){return e.id==l.id;});
        if(active!=effects.end())return active->clip==6666&&c9Window6666(tick-active->begin)==l.window;
        const auto stopped=stoppedEffectBases.find(l.id);
        if(stopped!=stoppedEffectBases.end()){
            if(stopped->second.clip!=6666)return false;
            //A stopped live quad may freeze between keys while its D0 stores the endpoint.
            auto key=[](const std::array<int32_t,6>& w){return (int64_t(864)-w[0])*512+w[2]-191;};
            uint64_t lo=0,hi=4000;const auto target=key(l.window);
            while(lo<hi){auto mid=(lo+hi)/2;if(key(c9Window6666(mid))<target)lo=mid+1;else hi=mid;}
            return c9Window6666(lo)==l.window;
        }
        //Natural completion removes the job. Exact endpoint inputs are Native-equivalent.
        return l.window==std::array<int32_t,6>{806,0,308,1080,0,94};
    }

    int finalCallback28(const Machine* vm,const Layer& l) const {
        if(!vm)return 0;auto module=lower(vm->moduleName());auto sub=vm->variable(0x462);
        const bool a0=module=="a_0.chs"&&sub==33&&l.image.find("aok_n_06_02_07_40_00.")!=std::string::npos;
        const bool c9=module=="c_9.chs"&&(sub==38||sub==39)&&l.image.find("img2376.")!=std::string::npos;
        if(!a0&&!c9)return 0;
        if(alphaRules.contains(l.id))throw mahoyo::vm::Error("Unreviewed alpha-rule/final28 combination");
        if(a0){
            if(l.image!="aok_n_06_02_07_40_00.mzp"||l.baseImage!="aok_n_06_02_07.mzp"||l.parent>=0||l.kind=="ui"||l.hasCrop||l.window[2]||l.window[3]||l.blend!=17||l.blendFlag||(l.colorMode!=0&&l.colorMode!=70)||l.colorValue||l.brightness||l.rgb||l.decimalMode%10==0)throw mahoyo::vm::Error("Unreviewed Native A0 final28 context");
            return 85;
        }
        if(!l.window[2]&&!l.window[3])return 0; //Native no-rect callback4 keeps requested blend.
        if(l.image!="img2376.mzp"||l.baseImage!="img2376.mzp"||l.parent>=0||l.kind=="ui"||l.hasCrop||!reviewedC9Rect(l)||l.blendFlag||l.colorMode||l.colorValue||l.brightness||l.rgb||l.decimalMode%10!=0||waves.contains(l.id))throw mahoyo::vm::Error("Unreviewed Native C9 rect final28 context");
        for(const auto& child:layers)if(child.second.parent==l.id)throw mahoyo::vm::Error("Unreviewed C9 final28 children");
        return 120;
    }
    int noiseProfile(const Machine* vm,const Layer& l) const {
        if(!vm)return 0;auto module=lower(vm->moduleName());auto sub=vm->variable(0x462);
        auto contains=[&](const char* key){return l.image.find(key)!=std::string::npos||l.baseImage.find(key)!=std::string::npos;};
        if(module=="9_3.chs"&&sub>=10&&sub<=18&&contains("img0408.")){
            if(l.image!="img0408.mzp"||l.baseImage!="img0408.mzp"||l.parent>=0||l.blend!=6||l.blendFlag||l.colorMode!=17||l.colorValue||l.brightness||l.rgb||l.kind=="ui"||l.hasCrop||(l.window[2]&&l.window[3]))throw mahoyo::vm::Error("Unsupported noise78 Native context");
            return 78;
        }
        if(module=="9_6.chs"&&sub==0&&contains("img0408.")){
            if(l.image!="img0408.mzp"||l.baseImage!="img0408.mzp"||l.parent>=0||l.blend!=6||l.blendFlag||l.colorMode||l.colorValue||l.brightness||l.rgb||l.kind=="ui"||l.hasCrop||(l.window[2]&&l.window[3]))throw mahoyo::vm::Error("Unsupported noise82 Native context");
            return 82;
        }
        if(module=="a_0.chs"&&sub==33&&contains("aok_n_06_02_07_40_00.")){if(finalCallback28(vm,l)!=85)throw mahoyo::vm::Error("Missing Native A0 final28");return 85;}
        if(module=="c_9.chs"&&(sub==38||sub==39))throw mahoyo::vm::Error("Unreviewed C9 noise combination");
        return 0;
    }
    void validateNoiseCallback(const Machine* vm,const Layer& l)const{(void)noiseProfile(vm,l);}
    void validateNoiseLayer(const Layer& l,const mahoyo::wave::Controller& waveState) const {
        auto pending=waveState.pending().find(l.id);
        if(alphaRules.contains(l.id)||l.kind=="ui"||l.image.empty()||l.parent>=0||l.hasCrop||(l.window[2]&&l.window[3])||waveState.contains(l.id)||
           (pending!=waveState.pending().end()&&pending->second.preset>0))throw mahoyo::vm::Error("Unsupported native noise layer context");
    }
    std::map<int,mahoyo::noise::Config> noiseConfiguration(const std::map<int,Layer>& values,const mahoyo::wave::Controller& waveState,const Machine* vm){
        std::map<int,mahoyo::noise::Config> desired;
        for(const auto& pair:values)if(auto cfg=mahoyo::noise::decode(pair.second.decimalMode)){
            validateNoiseLayer(pair.second,waveState);validateNoiseCallback(vm,pair.second);imageSize(cfg->selector==1?"noise0.mzp":"noise1.mzp");desired.emplace(pair.first,*cfg);
        }
        return desired;
    }
    bool prepareNoise(){
        if(pendingNoise.empty()&&pendingEffectNoise.empty())return false;
        auto desired=noiseConfiguration(layers,waves,machine.get());auto changed=noise.reconcile(desired);pendingNoise.clear();pendingEffectNoise.clear();return changed;
    }
    void refreshEffectNoise(){
        if(pendingEffectNoise.empty())return;
        std::map<int,mahoyo::noise::Config> desired;for(const auto& pair:noise.states())desired.emplace(pair.first,pair.second.config);
        for(int id:pendingEffectNoise){auto found=layers.find(id);if(found==layers.end()){desired.erase(id);continue;}if(auto cfg=mahoyo::noise::decode(found->second.decimalMode)){validateNoiseLayer(found->second,waves);validateNoiseCallback(machine.get(),found->second);imageSize(cfg->selector==1?"noise0.mzp":"noise1.mzp");desired.insert_or_assign(id,*cfg);}else desired.erase(id);}
        noise.reconcile(desired);pendingEffectNoise.clear();
    }
    void clearPreloads(){preloads=PreloadState{};preloadsDirty=true;}
    void createClock(int first,int hour,int minute,int x,int y,int scale,int color){
        if(uint32_t(first)>58)return;
        if(!supportedColorMode(color))throw mahoyo::vm::Error("Unsupported clock color mode "+std::to_string(color));
        auto face=imageLayouts.find("img1242");if(face==imageLayouts.end())throw mahoyo::vm::Error("Missing prepared native clock image layout: img1242");
        for(const auto& pair:layers)if(pair.second.parent>=first&&pair.second.parent<first+5&&(pair.first<first||pair.first>=first+5))throw mahoyo::vm::Error("Unsupported clock replacement of an image parent group");
        auto parts=mahoyo::clock::parts(hour,minute,x,y,scale,color,face->second[0],face->second[1]);std::array<Layer,5> candidate;
        for(size_t i=0;i<parts.size();++i){const auto& p=parts[i];auto& l=candidate[i];l.id=first+int(i);l.baseImage=p.image;l.variantX=l.variantY=p.variant;l.image=variantName(l);auto size=imageSize(l.image);
            l.z=p.priority;l.alpha=l.baseAlpha=p.alphaFixed16/65536.;l.uniformScale=p.uniform;l.w=size[0]*double(p.uniform);l.h=size[1]*double(p.uniform);
            l.pivotX=p.px*3./size[0];l.pivotY=p.py*3./size[1];l.drawOffsetX=-p.px*3.;l.drawOffsetY=-p.py*3.;l.rotation=p.rotation;l.colorMode=p.colorMode;
            if(layers.count(l.id))l.matrixMode=layers.at(l.id).matrixMode; //1012B0 does not overwrite pending+90.
            placeAt(l,p.x*3.,p.y*3.);
        }
        for(auto& l:candidate){if(noise.contains(l.id))pendingNoise.insert(l.id);cancelLayer(l.id,false);layers.insert_or_assign(l.id,std::move(l));}
        for(const auto& p:parts){auto name=std::string(p.image);name.resize(name.size()-4);recordSeenImage(name);}
    }
    void commitClockRotations(){
        for(const auto& pair:pendingClockRotations)if(!layers.count(pair.first))throw mahoyo::vm::Error("Missing clock rotation target layer "+std::to_string(pair.first));
        for(const auto& pair:pendingClockRotations){auto& l=layers.at(pair.first);transforms.erase(std::remove_if(transforms.begin(),transforms.end(),[&](const Transform& a){return a.id==pair.first&&a.mask==4;}),transforms.end());
            transforms.push_back(Transform{pair.first,4,l.rotation,0,pair.second,0,tick,tick+300,70});}
        pendingClockRotations.clear();
    }
    bool globalShakeBusy() const {return globalShake.active()||globalShake.pending()||globalShakeClear;}
    void resetGlobalShake(){globalShake.reset();globalShakeOffset={};globalShakeClear=false;globalShakeAfterClear.reset();}
    void validateWaveLayer(const std::map<int,Layer>& scene,int id,int preset) {
        auto found=scene.find(id);
        if(found==scene.end()||found->second.image.empty()||found->second.kind=="ui"||found->second.hasCrop||
           (found->second.window[2]&&found->second.window[3]&&!((preset==43||preset==44)&&found->second.parent<0&&found->second.blend==0&&found->second.blendFlag==0)))throw mahoyo::vm::Error("Unsupported wave owner: requires uncropped image without own window");
        const auto& owner=found->second;
        if(preset>=24&&preset<=36){
            const int blends[]={0,0,0,0,0,0,0,11,0,19,0,0,0},colours[]={39,39,39,39,0,39,39,70,52,0,27,0,0};
            if((preset==35?owner.parent<0:owner.parent>=0)||owner.blend!=blends[preset-24]||owner.blendFlag||owner.colorMode!=colours[preset-24]||owner.colorValue!=(preset==33?89:0)||owner.brightness||owner.rgb)throw mahoyo::vm::Error("Unsupported wave24..36 Native context");
        }
        if(preset>=37&&preset<=40){
            const int blends[]={0,20,11,17},flags[]={0,0,0,1},colours[]={0,39,37,0};
            if(owner.parent>=0||owner.blend!=blends[preset-37]||owner.blendFlag!=flags[preset-37]||owner.colorMode!=colours[preset-37]||owner.colorValue||owner.brightness||owner.rgb)throw mahoyo::vm::Error("Unsupported wave37..40 Native context");
        }
        if(preset>=41&&preset<=48&&(owner.parent>=0||owner.blend!=0||owner.blendFlag!=0))throw mahoyo::vm::Error("Unsupported wave41..48 root material");
        if(preset==23&&(owner.parent>=0||owner.blend!=22||owner.blendFlag!=0||owner.colorMode!=1||owner.rgb!=0x4ca5ff))throw mahoyo::vm::Error("Unsupported wave23 root22/flag0/color1/4CA5FF context");
        if(preset==22&&(owner.parent>=0||owner.blend!=17||owner.blendFlag!=0||owner.colorMode!=1||owner.rgb!=0x8e0707))throw mahoyo::vm::Error("Unsupported wave22 root17/flag0/color1/8E0707 context");
        if(preset==21&&(owner.parent>=0||owner.blend!=0||owner.blendFlag!=0||owner.colorMode!=1||owner.rgb!=0x8e0707))throw mahoyo::vm::Error("Unsupported wave21 root0/flag0/color1/8E0707 context");
        if(preset==19&&(owner.parent>=0||owner.blend!=16||owner.blendFlag!=0||owner.colorMode!=0||owner.rgb!=0))throw mahoyo::vm::Error("Unsupported wave19 root16/flag0/color0 context");
        if(owner.blend==18)throw mahoyo::vm::Error("Unsupported local wave/root blend18 combination");
        if(preset==10||preset==11||preset==12||preset==13||preset==14||preset==15||preset==16||preset==17||preset==18||preset==19||preset==20||preset==21||preset==22||preset==23||(preset>=24&&preset<=48)){
            //31/36 also review native mode0 up to1590/5084 cells. Validate
            //the resolved variant bitmap, not its logical/scaled layer size.
            const auto size=imageSize(owner.image);
            const int height=preset>=24&&preset<=48?std::array<int,25>{32,32,16,64,256,256,256,4,64,64,256,16,4,64,64,64,128,64,64,8,8,64,64,64,64}[preset-24]:preset==10?32:(preset==13||preset==14||preset==18||preset==20)?64:256;
            if(size[0]>16384||size[1]>16384||((size[0]+511)/512)*((size[1]+height-1)/height)>=(preset==31?1591:preset==36?5085:(preset==43||preset==44)?3052:1000))
                throw mahoyo::vm::Error("Unsupported wave"+std::to_string(preset)+" bitmap: "+((preset==31||preset==36||preset==43||preset==44)?"outside reviewed mode0 cell bound":"requires fewer than1000 native cells"));
        }
        if(preset==20&&(owner.parent<0||owner.blend!=11||owner.blendFlag!=0||owner.colorMode!=1||owner.rgb!=0xff1313))throw mahoyo::vm::Error("Unsupported wave20 child11/flag0/color1/FF1313 context");
        if(owner.parent>=0){auto parent=scene.find(owner.parent);
            //Reviewed child-wave paths are6/19,20/11 and35/0 with
            //their source/parent profiles. Ordinary children are broader.
            if(!((preset==6&&owner.blend==19&&owner.blendFlag==0)||(preset==20&&owner.blend==11&&owner.blendFlag==0&&owner.colorMode==1&&owner.rgb==0xff1313)||(preset==35&&owner.blend==0&&owner.blendFlag==0&&owner.colorMode==0&&owner.rgb==0)))throw mahoyo::vm::Error("Unsupported parented wave: requires reviewed6/19 or20/11 context");
            if(parent==scene.end()||parent->second.parent>=0||parent->second.image.empty()||parent->second.kind=="ui"||parent->second.hasCrop||
               !parent->second.window[2]||!parent->second.window[3])throw mahoyo::vm::Error("Unsupported wave parent: requires single-level native window image");
            if((preset==20||preset==35)&&(parent->second.blend!=0||parent->second.blendFlag!=0||parent->second.colorMode!=0||parent->second.rgb!=0||parent->second.window!=std::array<int32_t,6>{0,0,1920,1080,0,0}))throw mahoyo::vm::Error("Unsupported wave"+std::to_string(preset)+" full-window root0 parent context");
        }
        for(const auto& pair:scene)if(pair.second.parent==id)throw mahoyo::vm::Error("Unsupported nested wave image group");
    }
    void commitWaves(){
        //Validate the effective batch: new positives remove OLD wave jobs,
        //while a surviving active child must still have a supported parent.
        auto candidate=waves;candidate.commit(tick);
        for(const auto& record:candidate.records())validateWaveLayer(layers,record.layer,record.preset);
        auto globalCandidate=screenWave;globalCandidate.commit(tick);
        if(globalCandidate.active())validateScreenWave(layers);
        waves=std::move(candidate);screenWave=std::move(globalCandidate);
    }
    bool localWaveClockRunning()const{
        return error.empty()&&!paused&&(mode=="play"||mode=="teatime")&&!opening.active()&&wait!="transitionReady"&&wait!="movieReady";
    }
    void sampleWaveClock(uint64_t now){
        wavePlatformEnabled=true;
        if(wavePlatformSeen&&now<wavePlatformLast)return;
        const uint64_t elapsed=wavePlatformSeen?now-wavePlatformLast:0;
        wavePlatformLast=now;wavePlatformSeen=true;
        if(!elapsed||!localWaveClockRunning())return;
        //Native780E0's retained sourceRT clock runs atalpha0. Account for
        //time only when another real command arrives; do not wake or render.
        for(const auto& pair:layers)if(pair.second.enabled&&waves.contains(pair.first))waves.advancePhase(pair.first,elapsed);
    }
    void validateScreenWave(const std::map<int,Layer>& scene)const{
        //Native equal-priority ordering for a second22560 root is unreviewed.
        for(const auto& pair:scene)if(pair.second.parent<0&&pair.second.z==22560)throw mahoyo::vm::Error("Unsupported Screen Raster priority tie");
    }
    std::string screenWaveJson(){
        auto sample=screenWave.draw(tick,error.empty()&&!paused&&mode!="history"&&mode!="archive"&&!opening.active());
        if(!sample||mode=="archive"||opening.active())return "null";
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"{\"id\":"<<quote(std::to_string(sample->id))<<",\"preset\":"<<sample->preset<<",\"strength\":"<<sample->strength<<",\"elapsedMs\":"<<sample->phaseMs<<",\"captureMaxPriority\":22559,\"priority\":22560}";return out.str();
    }
    bool waveScheduled() const {
        if(waves.finite()||(screenWave.active()&&(mode=="play"||mode=="teatime")))return true;
        //The native prepass belongs to the child itself. A parent's global
        //alpha0 must not freeze its child's wave phase or dynamic scheduling.
        for(const auto& pair:layers)if(waves.contains(pair.first)&&pair.second.enabled&&pair.second.alpha>.0001)return true;
        return false;
    }
    void commitGlobalShake(){
        //341 cancellation is applied at the scene-ready barrier. A340 issued
        //after that clear request remains the replacement for this commit.
        if(globalShakeClear){globalShake.reset();if(globalShakeAfterClear)globalShake.queue(*globalShakeAfterClear);}
        globalShakeClear=false;globalShakeAfterClear.reset();globalShake.commit(tick);globalShakeOffset=globalShake.sample(tick);
    }
    // Native111CD0: ordinary voice slot0 only, including its queued play.
    // This current request record is cleared by actual matching audioEnded,
    // stop/replacement/reset; it is not a duration estimate or decoder query.
    bool normalVoicePendingOrPlaying() const {
        auto voice=std::find_if(audio.rbegin(),audio.rend(),[](const Audio& a){return a.channel=="voice";});
        return voice!=audio.rend()&&!voice->name.empty();
    }
    bool layerShakeBusy() const {return !layerPulses.empty()||!pendingLayerPulses.empty();}
    bool finiteLayerShakeBusy(int id) const {
        for(const auto& pair:layerPulses)if((id<0||pair.first==id)&&pair.second.parameters.durationMs>=0)return true;
        return false;
    }
    std::array<float,2> globalLayerOffset(const Layer& layer) const {
        if(layer.kind=="ui"||mahoyo::effects::isGlobalShakeExcluded(layer.baseImage.empty()?layer.image:layer.baseImage))return {0,0};
        return {globalShakeOffset.x*3.f,globalShakeOffset.y*3.f};
    }
    mahoyo::effects::LayerShakeGeometry layerShakeGeometry(const Layer& sourceLayer) {
        auto layer=sourceLayer;applyOutputParents(layer);
        //Current renderer supports a Z0 quad. Match its pivot/clockwise float
        //geometry, with global and target-pixel offsets but no local shake.
        auto global=globalLayerOffset(layer);const double divisor=layer.targetOffsetDivisor?layer.targetOffsetDivisor:1;
        const float left=float(layer.x+global[0]+layer.targetOffsetX/divisor),top=float(layer.y+global[1]+layer.targetOffsetY/divisor);
        const float width=float(layer.w),height=float(layer.h),cx=left+width*float(layer.pivotX),cy=top+height*float(layer.pivotY);
        const double radians=std::fmod(float(layer.rotation),360.f)*3.14159265358979323846/180.;
        const float cosine=float(std::cos(radians)),sine=float(std::sin(radians));
        mahoyo::effects::LayerShakeGeometry result;result.minProjectedX=INFINITY;result.minProjectedY=INFINITY;
        if(layer.matrixMode==1){auto size=intrinsicSize(layer);for(float u:{0.f,1.f})for(float v:{0.f,1.f}){float dx=size[0]*(u-float(layer.pivotX)),dy=size[1]*(v-float(layer.pivotY));result.minProjectedX=std::min(result.minProjectedX,cx+(dx*cosine-dy*sine)*width/size[0]);result.minProjectedY=std::min(result.minProjectedY,cy+(dx*sine+dy*cosine)*height/size[1]);}}
        else for(float x:{left,left+width})for(float y:{top,top+height}){float dx=x-cx,dy=y-cy;result.minProjectedX=std::min(result.minProjectedX,cx+dx*cosine-dy*sine);result.minProjectedY=std::min(result.minProjectedY,cy+dx*sine+dy*cosine);}
        return result;
    }
    void sampleLayerPulses(){
        if(paused||mode=="history"||wait=="transitionReady"||wait=="movieReady"){
            for(auto& pair:layerPulses)pair.second.clock.rebase(tick);return;
        }
        std::vector<std::pair<int,LayerPulse*>> ordered;for(auto& pair:layerPulses)ordered.emplace_back(pair.first,&pair.second);
        //Native+220 primaries precede+230 followers. Tail registration order
        //is preserved inside each group, including replacement jobs.
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){if((a.first<2)!=(b.first<2))return a.first<2;return a.second->serial<b.second->serial;});
        std::vector<int> finished;
        for(const auto& pair:ordered){int id=pair.first;auto& pulse=*pair.second;
            auto layer=layers.find(id);if(layer==layers.end()){layerPulseOffsets.erase(id);finished.push_back(id);continue;}
            auto elapsed=pulse.clock.sample(tick);if(!elapsed){layerPulseOffsets.erase(id);continue;}
            auto rand15=[&](){randomState=randomState*214013u+2531011u;return (randomState>>16)&32767u;};
            //Native5 period0 bypasses073B50/quad amplification entirely.
            if(pulse.mode==LayerPulseMotion274){layerPulseOffsets.insert_or_assign(id,pulse.motion274.sample(tick,rand15));continue;}
            auto geometry=layerShakeGeometry(layer->second);
            auto offset=(pulse.mode==0||pulse.mode==4||pulse.mode==5||pulse.mode==6)?pulse.random.sampleNative(pulse.parameters,*elapsed,geometry,rand15,id,layerPulseCache,pulse.mode==4&&!normalVoicePendingOrPlaying(),pulse.mode==6,pulse.mode==5):pulse.mode==3?mahoyo::effects::sampleLayerShakeMode3(pulse.parameters,*elapsed,geometry):mahoyo::effects::sampleLayerShakeMode2(pulse.parameters,*elapsed,geometry);
            if(!offset.active){if((pulse.mode==2||pulse.mode==3)&&id<2)layerPulseCache={};layerPulseOffsets.erase(id);finished.push_back(id);}else layerPulseOffsets.insert_or_assign(id,offset);
        }
        for(int id:finished)layerPulses.erase(id);
    }
    void commitLayerPulses(){
        for(const auto& pair:pendingLayerPulses){const auto& request=pair.second;
            //Native zero-amplitude non-clear requests do not replace old jobs.
            if(!request.clear&&!request.parameters.amplitudeX&&!request.parameters.amplitudeY)continue;
            auto old=layerPulses.find(pair.first);
            //Only a real primary callback clears the shared pre-geometry
            //cache; a missing graphics target leaves that context state alone.
            if(old!=layerPulses.end()&&(old->second.mode==0||old->second.mode==2||old->second.mode==3||old->second.mode==4||old->second.mode==5||old->second.mode==6||old->second.mode==LayerPulseMotion274)&&pair.first<2&&layers.count(pair.first))layerPulseCache={};
            layerPulses.erase(pair.first);layerPulseOffsets.erase(pair.first);
            for(auto& random:shakes)random.mask&=~(uint64_t(1)<<pair.first);
            if(!request.clear&&layers.count(pair.first)){LayerPulse pulse{request.parameters,tick,request.mode,{}};pulse.clock.reset(tick,request.delayMs);pulse.serial=++layerPulseSerial;layerPulses.emplace(pair.first,std::move(pulse));}
        }
        pendingLayerPulses.clear();shakes.erase(std::remove_if(shakes.begin(),shakes.end(),[](const Shake& shake){return !shake.mask;}),shakes.end());
    }
    void applyOutputParents(Layer& layer) const {
        int parent=layer.parent;std::set<int> visited;
        while(parent>=0){auto found=layers.find(parent);if(found==layers.end()||!visited.insert(parent).second)break;
            const auto& window=found->second.window;
            layer.x+=float(float(window[0])*640.f/1920.f)*3.f;
            layer.y+=float(float(window[1])*360.f/1080.f)*2.9779410362243652f;
            parent=found->second.parent;}
    }
    void applyOutputShakes(Layer& layer) const {
        auto global=globalLayerOffset(layer);layer.x+=global[0];layer.y+=global[1];
        if(layer.id>=0&&layer.id<64)for(const auto& shake:shakes)if(shake.mask&(uint64_t(1)<<layer.id)){int slot=layer.id==0?0:1;layer.x+=shake.dx[slot];layer.y+=shake.dy[slot];}
        auto pulse=layerPulseOffsets.find(layer.id);if(layer.kind!="ui"&&pulse!=layerPulseOffsets.end()){layer.x+=pulse->second.x*3.f;layer.y+=pulse->second.y*3.f;}
    }
    void commitPreloads(){
        if(preloads.releaseOld){preloads.ranges.clear();preloads.retained.clear();}
        for(const auto& pair:preloads.pendingRanges)preloads.ranges.insert_or_assign(pair.first,pair.second);
        preloads.retained.insert(preloads.retained.end(),preloads.pending.begin(),preloads.pending.end());
        preloads.pendingRanges.clear();preloads.pending.clear();preloads.releaseOld=false;preloadsDirty=true;
    }
    std::string preloadJson(){
        if(!preloadsDirty)return cachedPreloads;
        std::ostringstream out;out<<'[';bool first=true;
        auto emit=[&](const ImageHint& h){if(!first)out<<',';first=false;
            out<<"{\"kind\":"<<quote(h.layerId>=0?"range":"image")<<",\"base\":"<<quote(h.base)<<",\"name\":"<<quote(h.base)<<",\"layerId\":"<<h.layerId<<",\"tag\":"<<h.tag<<",\"variantX\":"<<h.variantX<<",\"variantY\":"<<h.variantY<<",\"xMin\":"<<h.range[0]<<",\"yMin\":"<<h.range[1]<<",\"xMax\":"<<h.range[2]<<",\"yMax\":"<<h.range[3]<<",\"brightness\":"<<h.brightness<<",\"contrast\":"<<h.contrast<<",\"colorMode\":"<<h.colorMode<<",\"rgb\":"<<h.rgb<<'}';};
        auto ranges=preloads.releaseOld?std::map<int,ImageHint>{}:preloads.ranges;
        for(const auto& pair:preloads.pendingRanges)ranges.insert_or_assign(pair.first,pair.second);
        for(const auto& pair:ranges)emit(pair.second);
        if(!preloads.releaseOld)for(const auto& h:preloads.retained)emit(h);
        for(const auto& h:preloads.pending)emit(h);
        out<<']';preloadsDirty=false;return cachedPreloads=out.str();
    }
    void title() {
        inputRegion.reset();inputJumpRelease=0;
        choices.reset();
        waitClock.clearRequest();
        wavePlatformSeen=false;
        resetExtraFrontends();
        std::optional<Audio> previous;for(auto i=audio.rbegin();i!=audio.rend();++i)if(i->channel=="bgm"){previous=*i;break;}
        flushShared();intermission.reset();machine.reset();
        archiveReplay=0;
        opening.stop();
        pendingEffectCaptures.clear();
        resetGlobalShake();
        screenWave.reset();waitingScreenWave=false;
        particleOverlays.clear();
        clearPreloads();
        transition.active=false;
        windowFade.reset();windowMode=0;pendingWindowMode.reset();
        resetHistorySession();mode="title";wait.clear();error.clear();layers.clear();cancelLayer(-1);resetEffectRegistry();paragraph.clear();paragraphSources.clear();fontCarryItalic=false;paragraphItalic=false;reveal.reset(paragraph,tick);hidden=false;resetAudio();
        layers[0]=Layer{0,0,"title_bg1.cbg","background","",0,0,1920,1080,1};
        layers[1]=Layer{1,1,"title_titlelogo_ja.cbg","ui","",100,110,952,303,1};
        for(int i=0;i<6;++i){Layer l{10+i,10+i,"title_menu_ja.cbg","ui","",1280.,455.+i*89.,495,89,1};l.crop={0,i*89,495,89};l.hasCrop=true;layers[l.id]=l;}
        titleMusic(previous);
        ++version;
    }
    void play(const std::string& module="1_0.chs",bool run=true) {
        inputRegion.reset();inputJumpRelease=0;
        waitClock.clearRequest();
        resetExtraFrontends();
        flushShared();intermission.reset();
        archiveReplay=0;
        opening.stop();
        pendingEffectCaptures.clear();
        resetGlobalShake();
        screenWave.reset();waitingScreenWave=false;
        particleOverlays.clear();
        clearPreloads();
        transition.active=false;presentedModule.clear();presentedLayers="[]";presentedText="[]";presentedMovie="null";presentedScreenWave="null";presentedParticles="null";
        windowFade.reset();windowMode=0;pendingWindowMode.reset();
        mode="play";layers.clear();cancelLayer(-1);resetEffectRegistry();paragraph.clear();paragraphSources.clear();fontCarryItalic=false;paragraphItalic=false;reveal.reset(paragraph,tick);clearHistory();error.clear();notice.clear();wait.clear();textLine=0;resetAudio();
        choices.reset();machine=std::make_unique<Machine>(*this);if(sharedLoaded)machine->restoreShared(sharedState.variables,sharedState.flags);flushShared();machine->start(module);if(run)pump();
    }
    void pump() {
        try {auto flow=machine->run(20000);if(flow==Flow::Halt){if(archiveReplay)openArchive(archiveReplay,archiveCheckpoint);else wait="end";}else if(flow==Flow::Continue){wait="budget";}captureWaitClock();++version;}
        catch(const std::exception& e){error=e.what();wait="error";++version;}
    }
    void resume(bool interruptVoice=false) {if(inputRegion.jumpQueued()){effectWaitLayer.reset();finishInputJump();return;}if(!machine||wait=="error"||wait=="end")return;if(deferredVoice){if(paused||mode!="play"||(!interruptVoice&&!skipping&&voicePlaying()))return;auto next=std::move(*deferredVoice);deferredVoice.reset();startNormalVoice(next);}if(transition.mode==1)transition.active=false;wait.clear();waitingScreenWave=false;if(effectWaitLayer)queryEffectRegistry(*effectWaitLayer);effectWaitLayer.reset();machine->resume();pump();}
    #include "input_runtime.inc"
    void historyChanged(){historyRevision=historyToken();cachedHistoryRevision=0;}
    void resetHistorySession(){historyReplay.reset();historyGeneration=historyToken();}
    void clearHistory(){history.clear();historySources.clear();historyMeta.clear();historyChanged();resetHistorySession();}
    bool effectNoiseKeys(const mahoyo::effects::Clip& clip)const{return clip.propertiesMetadata&&mahoyo::effects::isSelected(clip,mahoyo::effects::AuxResourceAndScale)&&std::any_of(clip.frames.begin(),clip.frames.end(),[](const auto& f){return f.properties[11]!=-32766&&f.properties[11]!=-32767;});}
    void savedEffectNoise(Effect& e,int schema,const Layer& saved){if(schema<19){if(effectNoiseKeys(effectClips.at(e.clip)))throw mahoyo::save::Error("Legacy active/stopped effect lacks initial noise state");e.properties.decimalMode=saved.decimalMode;}try{mahoyo::noise::decode(e.properties.decimalMode);}catch(const std::exception&){throw mahoyo::save::Error("Invalid saved effect noise baseline");}}
    std::map<int,Effect> readStoppedEffectBases(std::istream& input,const std::map<int,Layer>& scene,const std::vector<Effect>& running,int schema){
        std::string tag,countToken;if(!(input>>tag>>countToken)||tag!="EFFECTBASES1"||countToken.empty()||countToken.size()>2||countToken.find_first_not_of("0123456789")!=std::string::npos)throw mahoyo::save::Error("Invalid saved stopped effect count");
        const auto count=std::stoul(countToken);if(count>64)throw mahoyo::save::Error("Invalid saved stopped effect count");
        std::map<int,Effect> values;auto canonicalScene=scene;
        for(size_t index=0;index<count;++index){
            Effect e{};std::string elapsed;auto& p=e.properties;
            if(!(input>>e.id>>e.clip>>elapsed>>e.x>>e.y>>e.w>>e.h>>e.alpha>>e.rotation>>e.sourceW>>e.sourceH))throw mahoyo::save::Error("Invalid saved stopped effect geometry");
            for(auto& value:e.window)if(!(input>>value))throw mahoyo::save::Error("Invalid saved stopped effect window");
            if(!(input>>p.originX>>p.originY>>p.blendMode>>p.colorMode>>p.rgb>>p.variantX>>p.variantY>>p.brightness>>p.contrast>>p.priority>>e.pivotX>>e.pivotY>>e.drawOffsetX>>e.drawOffsetY>>e.uniformScale>>e.axisScaleX>>e.axisScaleY))throw mahoyo::save::Error("Invalid saved stopped effect properties");
            if(schema>=19&&!(input>>p.decimalMode))throw mahoyo::save::Error("Invalid stopped effect noise baseline");
            if(e.id<0||e.id>=64||!scene.count(e.id)||e.clip<=0||!effectClips.count(e.clip)||elapsed!="0"||values.count(e.id)||std::any_of(running.begin(),running.end(),[&](const Effect& active){return active.id==e.id;}))throw mahoyo::save::Error("Invalid saved stopped effect identity");
            for(double value:{e.x,e.y,e.w,e.h,e.alpha,e.rotation,e.sourceW,e.sourceH,e.pivotX,e.pivotY,e.drawOffsetX,e.drawOffsetY,e.uniformScale,e.axisScaleX,e.axisScaleY})if(!std::isfinite(value))throw mahoyo::save::Error("Nonfinite saved stopped effect geometry");
            if(e.alpha<0||e.alpha>1||e.sourceW<=0||e.sourceH<=0||!supportedRootBlendMode(p.blendMode)||!supportedColorMode(p.colorMode)||p.rgb>0xffffff||p.variantX<0||p.variantY<0||p.variantX>32767||p.variantY>32767||p.priority<0||p.priority>=0x1000000)throw mahoyo::save::Error("Invalid saved stopped effect range");
            auto checkedWindow=e.window;validateWindow(checkedWindow);if(checkedWindow!=e.window)throw mahoyo::save::Error("Invalid saved stopped effect window");
            savedEffectNoise(e,schema,scene.at(e.id));
            //Source-size checks happen after all19 tags and per-baseline migration.
            auto base=effectBase(scene.at(e.id),e);
            canonicalScene.at(e.id)=std::move(base);values.emplace(e.id,e); //begin stays0: never a playback clock.
        }
        validateParents(canonicalScene);return values;
    }
    void appendHistory(const std::string& text,mahoyo::history::Row row,const std::string& raw){
        historySources.push_back({"",raw});if(historySources.size()>mahoyo::history::MaxRows)historySources.erase(historySources.begin());
        historyMeta.append(std::move(row));history.push_back(text);if(history.size()>mahoyo::history::MaxRows)history.erase(history.begin());historyChanged();
    }
    std::string historyVoicePath(const mahoyo::history::Voice& voice)const{return "audio/"+lower(voice.name)+".ogg";}
    bool historyVoiceAvailable(const mahoyo::history::Voice& voice)const{
        if(!voice.active||voice.name.empty()||voice.backend!=0)return false;
        auto name=lower(voice.name);if(name.size()<=3||name.compare(name.size()-3,3,".hw")!=0)return false;
        for(size_t i=0;i+3<name.size();++i)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='0'&&name[i]<='9')||name[i]=='_'||name[i]=='-'))return false;
        std::error_code error;
        if(std::filesystem::is_regular_file(root/".steam-ready",error)&&!error)return audioLoops.count(name)!=0;
        error.clear();return std::filesystem::is_regular_file(root/historyVoicePath(voice),error)&&!error;
    }
    bool historyGroupStart(size_t index)const{return index==0||!historyMeta.rows[index].known||!historyMeta.rows[index-1].known||historyMeta.rows[index].startsGroup||historyMeta.rows[index-1].endsGroup;}
    std::string historyJson(bool preview=false){
        if(mode!="history"&&!preview)return "[]";
        if(cachedHistoryRevision==historyRevision)return cachedHistory;
        historyMeta.validate(history.size());std::ostringstream out;out<<'[';
        for(size_t i=0;i<history.size();++i){if(i)out<<',';const auto& r=historyMeta.rows[i];
            out<<"{\"index\":"<<i<<",\"text\":"<<quote(history[i])<<",\"metadataKnown\":"<<(r.known?"true":"false")<<",\"leadingBreaks\":"<<r.leadingBreaks<<",\"startsGroup\":"<<(r.startsGroup?"true":"false")<<",\"endsGroup\":"<<(r.endsGroup?"true":"false")<<",\"initialItalic\":"<<(r.initialItalic?"true":"false")<<",\"fontSize\":48,\"fontColor\":\"#ffffff\",\"wait\":"<<(r.wait?"true":"false")<<",\"page\":"<<(r.page?"true":"false")<<",\"style\":"<<r.style<<",\"voices\":[";
            for(size_t j=0;j<r.voices.size();++j){if(j)out<<',';const auto& v=r.voices[j];out<<"{\"active\":"<<(v.active?"true":"false")<<",\"backend\":"<<v.backend<<",\"name\":"<<quote(v.name)<<",\"volume\":"<<v.volume<<",\"pan\":"<<v.pan<<",\"variant\":"<<v.variant<<",\"actorId\":"<<mahoyo::history::actorId(v.name)<<",\"available\":"<<(historyVoiceAvailable(v)?"true":"false")<<'}';}
            out<<"]}";
        }
        out<<']';cachedHistory=out.str();cachedHistoryRevision=historyRevision;return cachedHistory;
    }
    std::string historyReplayJson()const{
        if(mode!="history"||!historyReplay)return "null";const auto& replay=*historyReplay;std::ostringstream out;
        out<<"{\"id\":"<<quote(std::to_string(replay.id))<<",\"generation\":"<<quote(std::to_string(replay.generation))<<",\"status\":\"playing\",\"clips\":[";
        for(size_t i=0;i<replay.clips.size();++i){if(i)out<<',';const auto& c=replay.clips[i];out<<"{\"row\":"<<c.row<<",\"index\":"<<c.index<<",\"path\":"<<quote(c.path)<<",\"actorId\":"<<c.actor<<",\"rowVolumePercent\":"<<c.volume<<",\"actorGainPercent\":"<<c.gain<<",\"pan\":"<<c.pan<<",\"variant\":"<<c.variant<<'}';}
        out<<"]}";return out.str();
    }
    void requestHistoryReplay(const std::string& payload){
        std::istringstream in(payload);std::string revision,extra;uint64_t start,count;
        if(mode!="history")return;
        if(!(in>>revision>>start>>count)||in>>extra||revision!=std::to_string(historyRevision)||!count||start>=history.size()||count>history.size()-start){notice="Stale or invalid history selection";return;}
        const size_t end=size_t(start+count);if(!historyGroupStart(size_t(start))||(end<history.size()&&!historyGroupStart(end))){notice="History selection is not a complete group";return;}
        std::vector<HistoryClip> clips;
        for(size_t i=size_t(start);i<end;++i){const auto& row=historyMeta.rows[i];
            if(!row.known||(i>start&&historyGroupStart(i))){notice="History selection has no supported voice metadata";return;}
            size_t eligible=0;
            for(size_t j=0;j<row.voices.size();++j){const auto& voice=row.voices[j];if(!voice.active||voice.name.empty())continue;
                if(voice.backend!=0||++eligible>1){notice="Multiple voices in one history row are not supported";return;}
                if(!historyVoiceAvailable(voice)){notice="Missing prepared history voice: "+voice.name;return;}
                int actor=mahoyo::history::actorId(voice.name),setting=actor==0?14:actor-1;
                clips.push_back(HistoryClip{i,j,historyVoicePath(voice),actor,voice.volume,voice.pan,voice.variant,mahoyo::history::actorGain(historyActorVolumes[size_t(setting)])});
            }
        }
        if(clips.empty()){notice="This history group has no replayable voice";return;}
        //Validation finishes before either normal or replay voice is stopped.
        auto onlyVoice=[](const auto& item){return item.channel=="voice";};
        pendingSounds.erase(std::remove_if(pendingSounds.begin(),pendingSounds.end(),onlyVoice),pendingSounds.end());
        pendingVolumeFades.erase(std::remove_if(pendingVolumeFades.begin(),pendingVolumeFades.end(),onlyVoice),pendingVolumeFades.end());
        audioFades.erase(std::remove_if(audioFades.begin(),audioFades.end(),onlyVoice),audioFades.end());
        auto normal=std::find_if(audio.rbegin(),audio.rend(),[](const Audio& a){return a.channel=="voice";});
        if(normal!=audio.rend()&&!normal->name.empty()){audio.push_back(Audio{++audioId,"","voice",false,0});if(audio.size()>64)audio.erase(audio.begin());}
        historyReplay=HistoryReplay{historyToken(),historyGeneration,std::move(clips)};notice.clear();
    }
    void finishHistoryReplay(const std::string& payload,bool failed){
        std::istringstream in(payload);std::string generation,id,extra;
        if(!(in>>generation>>id)||!historyReplay||generation!=std::to_string(historyReplay->generation)||id!=std::to_string(historyReplay->id))return;
        if(!failed&&in>>extra)return;
        historyReplay.reset();if(failed)notice="History voice playback failed";
    }
    void sound(std::string name,std::string channel,bool loop) {
        if(name.find('.')==std::string::npos)name+=".hw";
        if(!safeName(name))throw mahoyo::vm::Error("Invalid audio asset name");
        pendingSounds.erase(std::remove_if(pendingSounds.begin(),pendingSounds.end(),[&](const PendingSound& p){return p.channel==channel;}),pendingSounds.end());
        pendingVolumeFades.erase(std::remove_if(pendingVolumeFades.begin(),pendingVolumeFades.end(),[&](const PendingVolumeFade& p){return p.channel==channel;}),pendingVolumeFades.end());
        audioFades.erase(std::remove_if(audioFades.begin(),audioFades.end(),[&](const AudioFade& a){return a.channel==channel;}),audioFades.end());
        audio.push_back(Audio{++audioId,lower(name),std::move(channel),loop});
        if(audio.size()>64)audio.erase(audio.begin());
    }
    bool voicePlaying() const {auto voice=std::find_if(audio.rbegin(),audio.rend(),[](const Audio& item){return item.channel=="voice";});return voice!=audio.rend()&&!voice->name.empty();}
    bool continuedVoicePlaying() const {
        if(!historyMeta.pendingRows)return false;
        auto current=std::find_if(audio.rbegin(),audio.rend(),[](const Audio& a){return a.channel=="voice";});
        if(current==audio.rend()||current->name.empty())return false;
        bool continuation=false,currentInGroup=false;
        for(size_t i=historyMeta.rows.size()-historyMeta.pendingRows;i<historyMeta.rows.size();++i){
            const auto& source=historySources[i].raw;continuation|=source.size()>=4&&source.compare(source.size()-4,4,"@k@e")==0;
            for(const auto& voice:historyMeta.rows[i].voices)currentInGroup|=voice.active&&lower(voice.name)==current->name;
        }
        return continuation&&currentInGroup;
    }
    void startNormalVoice(const mahoyo::history::Voice& voice){historyMeta.pendingVoices.clear();historyMeta.pendingVoices.push_back(voice);sound(voice.name,"voice",false);}
    void resetAudio() {
        deferredVoice.reset();
        std::map<std::string,bool> channels;for(const auto& a:audio)channels[a.channel]=true;
        pendingSounds.clear();pendingVolumeFades.clear();audio.clear();audioFades.clear();for(const auto& c:channels)audio.push_back(Audio{++audioId,"",c.first,false,0});
    }
    void fadeSound(const std::string& channel,int duration,double to,bool stop) {
        pendingSounds.erase(std::remove_if(pendingSounds.begin(),pendingSounds.end(),[&](const PendingSound& p){return p.channel==channel;}),pendingSounds.end());
        pendingVolumeFades.erase(std::remove_if(pendingVolumeFades.begin(),pendingVolumeFades.end(),[&](const PendingVolumeFade& p){return p.channel==channel;}),pendingVolumeFades.end());
        auto found=std::find_if(audio.rbegin(),audio.rend(),[&](const Audio& a){return a.channel==channel;});
        if(found==audio.rend())return;
        audioFades.erase(std::remove_if(audioFades.begin(),audioFades.end(),[&](const AudioFade& a){return a.channel==channel;}),audioFades.end());
        if(duration>0)audioFades.push_back(AudioFade{channel,found->volume,to,tick,tick+uint64_t(duration),stop,found->pan,found->pan});
        else if(stop)audio.push_back(Audio{++audioId,"",channel,false,0});else found->volume=to;
    }
    void startDueSounds() {
        for(;;){
            auto found=std::find_if(pendingSounds.begin(),pendingSounds.end(),[&](const PendingSound& p){return tick>=p.begin;});if(found==pendingSounds.end())break;
            auto p=*found;pendingSounds.erase(found);sound(p.name,p.channel,p.loop);auto& a=audio.back();a.volume=p.duration?p.from:p.to;a.pan=p.duration?p.fromPan:p.toPan;
            if(p.duration)audioFades.push_back(AudioFade{p.channel,p.from,p.to,p.begin,p.begin+p.duration,false,p.fromPan,p.toPan});
        }
    }
    void requireSoundLoopMetadata(const std::string& name) const {
        auto key=lower(name);if(key.find('.')==std::string::npos)key+=".hw";auto loop=audioLoops.find(key);
        if(loop==audioLoops.end())throw mahoyo::vm::Error("Missing prepared native audio loop metadata: "+key);
        auto end=loop->second.end&&loop->second.end<loop->second.sampleCount?loop->second.end:loop->second.sampleCount;
        if(loop->second.start>=end)throw mahoyo::vm::Error("Invalid sound loop range: "+key);
    }
    void startDueVolumeFades() {
        for(;;){
            auto next=std::find_if(pendingVolumeFades.begin(),pendingVolumeFades.end(),[&](const PendingVolumeFade& f){return tick>=f.begin;});
            if(next==pendingVolumeFades.end())break;
            auto request=*next;pendingVolumeFades.erase(next);
            auto current=std::find_if(audio.rbegin(),audio.rend(),[&](const Audio& a){return a.channel==request.channel;});
            if(current==audio.rend()||current->name.empty())continue;
            //196 starts from the actual due tick's current volume. Its volume
            //axis replaces the old envelope, while a concurrent pan/stop lives on.
            for(auto& fade:audioFades)if(fade.channel==request.channel)fade.volumeActive=false;
            audioFades.erase(std::remove_if(audioFades.begin(),audioFades.end(),[](const AudioFade& fade){return !fade.volumeActive&&!fade.panActive&&!fade.stop;}),audioFades.end());
            double from=std::trunc(std::clamp(current->volume,0.,1.)*100.+1e-9)/100.;
            current->volume=from;
            audioFades.push_back(AudioFade{request.channel,from,request.to,tick,tick+request.duration,false,current->pan,current->pan,true,false,true});
        }
    }
    bool movieActions(mahoyo::movie::Actions actions) {
        bool completed=false;
        for(int n=0;actions.accepted&&n<4;++n){
            completed=completed||actions.completed;
            if(actions.playSE){const auto& c=actions.cue;auto channel="se"+std::to_string(c.slot);pendingSounds.push_back(PendingSound{c.path,channel,c.loop,c.volumeStart/100.,c.volumeEnd/100.,double(c.pan),double(c.pan),tick,c.fadeMs});startDueSounds();}
            if(actions.fadeBgm)fadeSound("bgm",int(actions.bgmFadeMs),0,true);
            if(!actions.playSE&&!actions.fadeBgm)return completed;
            actions=opening.committed(actions.id);
        }
        return completed;
    }
    std::string movieJson() const {
        if(!opening.active())return "null";const auto& m=opening.state();std::ostringstream s;s.imbue(std::locale::classic());
        s<<"{\"id\":"<<m.id<<",\"path\":"<<quote(resourceName("opening_ja.mp4"))<<",\"positionMs\":"<<m.positionMs<<",\"durationMs\":"<<m.durationMs<<",\"seekSerial\":"<<m.seekSerial<<",\"volume\":0.4,\"paused\":"<<((paused||mode=="history")?"true":"false");
        if(m.seekSerial)s<<",\"seekToMs\":"<<m.seekToMs;s<<'}';return s.str();
    }
    std::filesystem::path savePath(int slot) const {return root.parent_path()/"savedata"/("slot"+std::to_string(slot)+".whs");}
    std::filesystem::path seenPath() const {return root.parent_path()/"savedata/seen-images.whp";}
    void loadSeenImages(){
        if(!std::filesystem::exists(seenPath()))return;
        const auto document=mahoyo::save::read(seenPath());std::string bytes(document.hostExtra.begin(),document.hostExtra.end());std::istringstream rows(bytes);std::string signature,name;std::getline(rows,signature);
        if(signature!="WOHSEEN1")throw mahoyo::vm::Error("Invalid image-unlock database");
        while(rows>>std::quoted(name)){if(!safeName(name)||name.find_first_of("\r\n")!=std::string::npos||name.size()>4096)throw mahoyo::vm::Error("Invalid image-unlock name");seenImages.insert(name);}
        if(!rows.eof())throw mahoyo::vm::Error("Invalid image-unlock database entry");
    }
    std::filesystem::path trophyPath() const {return root.parent_path()/"savedata/trophies.whp";}
    void loadTrophies(){
        if(!std::filesystem::exists(trophyPath()))return;
        auto document=mahoyo::save::read(trophyPath());std::istringstream input(std::string(document.hostExtra.begin(),document.hostExtra.end()));std::string tag;size_t count;
        if(!(input>>tag>>count)||tag!="WOHTROPHY1"||count>26)throw mahoyo::vm::Error("Invalid local trophy registry");
        for(size_t i=0;i<count;++i){int id;if(!(input>>id)||id<1||id>26||!trophies.insert(id).second)throw mahoyo::vm::Error("Invalid local trophy ID");}
        if(input>>tag)throw mahoyo::vm::Error("Unexpected local trophy data");
    }
    void unlockTrophy(int selector,Machine& vm){
        auto gate=[&](std::initializer_list<int> flags){return std::all_of(flags.begin(),flags.end(),[&](int index){return vm.flag(size_t(index),true);});};
        auto unlock=[&](int id){trophiesDirty=trophies.insert(id).second||trophiesDirty;};
        if((selector>=1&&selector<=15)||selector==17||selector==26)unlock(selector);
        if(selector==16&&gate({153,178,181,187,196,197,200,205}))unlock(16);
        if(selector>=18&&selector<=23)unlock(selector);
        if(selector>=18&&selector<=24&&gate({129,130,131,132,133,134}))unlock(24);
        if(selector==25&&gate({6,7,8,9}))unlock(25);
        if(!trophiesDirty)return;
        //Android has no SteamUserStats adapter. This independent atomic local
        //registry is a port policy; optional achievement I/O never halts CHS.
        try{std::ostringstream out;out<<"WOHTROPHY1 "<<trophies.size()<<'\n';for(int id:trophies)out<<id<<'\n';auto text=out.str();mahoyo::save::Document doc;doc.hostExtra.assign(text.begin(),text.end());mahoyo::save::writeAtomic(trophyPath(),doc);trophiesDirty=false;}
        catch(const std::exception& e){notice=std::string("Local trophy persistence: ")+e.what();}
    }
    void recordSeenImage(std::string name){
        name=lower(name);if(name.rfind("img",0)!=0)return;name+=".mzp";
        if(!safeName(name)||name.find_first_of("\r\n")!=std::string::npos||name.size()>4096)throw mahoyo::vm::Error("Invalid image-unlock name");
        if(seenImages.count(name))return;
        auto candidate=seenImages;candidate.insert(name);std::ostringstream rows;rows<<"WOHSEEN1\n";for(const auto& item:candidate)rows<<std::quoted(item)<<'\n';
        auto bytes=rows.str();mahoyo::save::Document document;document.hostExtra.assign(bytes.begin(),bytes.end());mahoyo::save::writeAtomic(seenPath(),document);seenImages.swap(candidate);
    }
    void saveGame(int slot) {
        if(slot<1||slot>12)throw mahoyo::save::Error("Invalid save slot");
        if(!canSaveNow())throw mahoyo::save::Error("Save is available after the current transition");
        if(!flushShared())throw mahoyo::save::Error(notice);
        mahoyo::save::Document document;document.vm=machine->saveState();document.history=history;document.text=paragraph;document.textLine=uint32_t(textLine);document.hidden=hidden;
        for(const auto& pair:layers){const auto& l=pair.second;document.layers.push_back(mahoyo::save::Layer{l.id,l.z,l.image,l.kind,l.color,l.x,l.y,l.w,l.h,l.alpha,l.crop,l.hasCrop});}
        std::map<std::string,Audio> channels;for(const auto& a:audio)channels.insert_or_assign(a.channel,a);
        for(const auto& pair:channels){const auto& a=pair.second;if(!a.name.empty())document.audio.push_back(mahoyo::save::Audio{a.name,a.channel,a.loop,a.volume});}
        //WOHHOST7 retains both applied and uncommitted cache intentions. They
        //never become visible layers, and old3..6 saves begin with empty hints.
        std::ostringstream effectData;effectData.imbue(std::locale::classic());effectData<<std::setprecision(17)<<"WOHHOST23\n"<<layers.size()<<'\n';
        for(const auto& pair:layers){const auto& l=pair.second;effectData<<l.id<<' '<<l.rotation<<' '<<l.pivotX<<' '<<l.pivotY<<' '<<l.baseAlpha<<' '<<l.enabled<<' '<<std::quoted(l.baseImage)<<' '<<l.variantX<<' '<<l.variantY<<' '<<l.colorMode<<' '<<l.colorValue<<' '<<l.rgb<<' '<<l.brightness<<' '<<l.blend<<' '<<l.blendFlag<<' '<<l.decimalMode<<' '<<l.targetOffsetX<<' '<<l.targetOffsetY<<' '<<l.targetOffsetDivisor;for(auto value:l.window)effectData<<' '<<value;effectData<<' '<<l.parent<<' '<<l.drawOffsetX<<' '<<l.drawOffsetY<<' '<<l.uniformScale<<' '<<l.axisScaleX<<' '<<l.axisScaleY<<'\n';}
        effectData<<effects.size()<<'\n';
        for(const auto& e:effects){auto elapsed=effectRawElapsed(e,tick);const auto& clip=effectClips.at(e.clip);if(elapsed>clip.duration)elapsed=clip.loopStart?clip.loopStart+(elapsed-clip.duration)%(clip.duration-clip.loopStart):clip.duration;writeEffectPacket(effectData,e,elapsed);}
        writePreloads(effectData,preloads);
        effectData<<"WINDOWFADE2 "<<windowFade.visible()<<' '<<windowFade.currentFixed16()<<' '<<windowMode<<'\n';
        effectData<<"EFFECTFRAMES1 "<<frameOverrides.size()<<'\n';
        for(const auto& pair:frameOverrides){const auto& f=pair.second;effectData<<pair.first<<' '<<f.x<<' '<<f.y<<' '<<f.alpha;for(auto value:f.window)effectData<<' '<<value;effectData<<'\n';}
        effectData<<"TEATIME1 "<<(intermission.active()?intermission.state()->selector:0)<<'\n'<<"COMMON1 "<<sharedState.generation<<'\n';
        effectData<<"GEOMETRYMODES1 "<<layers.size()<<'\n';for(const auto& pair:layers)effectData<<pair.first<<' '<<pair.second.matrixMode<<'\n';
        const auto savedWaves=waves.records();effectData<<"WAVES1 "<<savedWaves.size()<<'\n';
        for(const auto& w:savedWaves)effectData<<w.layer<<' '<<w.preset<<' '<<w.fadeMs<<' '<<w.strengthElapsed<<' '<<w.stopping<<' '<<w.drawStarted<<' '<<w.phaseMs<<'\n';
        //Port future-state persistence: native full save/RNG serialization was
        //not proved. Two idle cache floats can affect a later follower-only job.
        effectData<<"LOCAL270CACHE1 "<<layerPulseCache.x<<' '<<layerPulseCache.y<<'\n';
        effectData<<"FONTCARRY1 "<<fontCarryItalic<<'\n';
        effectData<<"EXACTLAYOUT1 "<<!exactLayoutNames.empty()<<'\n';
        const auto savedScreen=screenWave.record();effectData<<"SCREENWAVE1 "<<screenWave.generation()<<' '<<bool(savedScreen)<<'\n';
        if(savedScreen){const auto& r=*savedScreen;effectData<<r.preset<<' '<<r.fadeMs<<' '<<r.strengthElapsed<<' '<<r.stopping<<' '<<r.drawStarted<<' '<<r.phaseMs<<'\n';}
        //Only metadata is duplicated here; Document.history remains the sole
        //string owner. Native replay decoder/position is intentionally volatile.
        mahoyo::history::write(effectData,historyMeta,document.history.size());
        //Port future-state policy: native snapshots prove the owner's reset
        //pair, but not serialization of the separate manager clock itself.
        waitClock.write(effectData,tick);
        //Inactive native D0 can differ from frozen live graphics. It is not
        //an animation job and has no elapsed time or scheduling side effects.
        effectData<<"EFFECTBASES1 "<<stoppedEffectBases.size()<<'\n';
        for(const auto& pair:stoppedEffectBases)writeEffectPacket(effectData,pair.second,0);
        inputRegion.write(effectData);
        mahoyo::bitmap_geometry::SavedSizes bitmapSizes;for(const auto& pair:layers){auto size=intrinsicSize(pair.second);bitmapSizes.emplace(pair.first,mahoyo::bitmap_geometry::Size{size[0],size[1]});}mahoyo::bitmap_geometry::writeSavedSizes(effectData,bitmapSizes);
        //Current request descriptors restart on load, never platform playheads.
        effectData<<"AUDIOPAN1 "<<document.audio.size()<<'\n';
        for(const auto& a:document.audio)effectData<<std::quoted(a.channel)<<' '<<channels.at(a.channel).pan<<'\n';
        mahoyo::host_state20::State hostState;hostState.registeredEffects=registeredEffectIds;hostState.randomState=randomState;
        hostState.fallNote=particleOverlays.fallNoteState();hostState.focusLine=particleOverlays.focusLineState();
        std::vector<int> activeEffectIds;for(const auto& e:effects){activeEffectIds.push_back(e.id);if(e.paused)hostState.pausedEffects.insert(e.id);}
        std::set<int> layerIds;for(const auto& pair:layers)layerIds.insert(pair.first);
        mahoyo::host_state20::write(effectData,hostState,activeEffectIds,layerIds);
        mahoyo::language::writeTextState(effectData,{languageAssets.edition(),languageAssets.current(),paragraphSources,historySources});
        auto extra=effectData.str();document.hostExtra.assign(extra.begin(),extra.end());
        std::filesystem::create_directories(savePath(slot).parent_path());mahoyo::save::writeAtomic(savePath(slot),document);activeSlot=slot;notice="Saved";
    }
    void loadGame(int slot) {
        if(slot<1||slot>12)throw mahoyo::save::Error("Invalid save slot");
        std::error_code missing;
        if(!std::filesystem::exists(savePath(slot),missing)&&!missing){notice="No saved game was found.";return;}
        const auto document=mahoyo::save::read(savePath(slot));auto candidate=std::make_unique<Machine>(*this);candidate->loadState(document.vm);
        std::map<int,Layer> restored;
        for(const auto& s:document.layers){if(!s.image.empty()&&!safeName(s.image))throw mahoyo::save::Error("Invalid saved image path");Layer l{s.id,s.z,s.image,s.kind,s.color,s.x,s.y,s.w,s.h,s.alpha};l.baseAlpha=s.alpha;l.crop=s.crop;l.hasCrop=s.hasCrop;restored.emplace(l.id,l);}
        for(const auto& a:document.audio)if(!safeName(a.name))throw mahoyo::save::Error("Invalid saved audio path");
        mahoyo::language::TextState restoredLanguage;
        mahoyo::history::State restoredHistoryMeta;restoredHistoryMeta.legacy(document.history.size());
        mahoyo::wait_clock::Clock restoredWaitClock;restoredWaitClock.restore(0,false,tick);
        std::map<int,Effect> restoredEffectBases;mahoyo::input_region::Stable restoredInput;mahoyo::host_state20::State restoredHost20;std::map<std::string,double> restoredAudioPans;
        mahoyo::wave::Controller restoredWaves;mahoyo::screen_wave::Controller restoredScreen;mahoyo::noise::Controller restoredNoise;mahoyo::effects::LayerShakeCache restoredPulseCache;std::vector<Effect> restoredEffects;mahoyo::effects::FrameOverrides restoredFrames;PreloadState restoredPreloads;bool restoredWindowVisible=!document.text.empty();int restoredWindowAlpha=restoredWindowVisible?65536:0,restoredWindowMode=0;mahoyo::bitmap_geometry::SavedSizes restoredBitmapSizes;int restoredIntermission=0,restoredFontCarry=0;uint64_t restoredCommonGeneration=0;bool legacy=true,restoredExactLayout=false;int hostSchema=0;
        if(!document.hostExtra.empty()){
            std::istringstream data(std::string(document.hostExtra.begin(),document.hostExtra.end()));data.imbue(std::locale::classic());
            if(data.peek()=='W'){
                std::string signature;size_t count;if(!(data>>signature>>count)||(signature!="WOHHOST3"&&signature!="WOHHOST4"&&signature!="WOHHOST5"&&signature!="WOHHOST6"&&signature!="WOHHOST7"&&signature!="WOHHOST8"&&signature!="WOHHOST9"&&signature!="WOHHOST10"&&signature!="WOHHOST11"&&signature!="WOHHOST12"&&signature!="WOHHOST13"&&signature!="WOHHOST14"&&signature!="WOHHOST15"&&signature!="WOHHOST16"&&signature!="WOHHOST17"&&signature!="WOHHOST18"&&signature!="WOHHOST19"&&signature!="WOHHOST20"&&signature!="WOHHOST21"&&signature!="WOHHOST22"&&signature!="WOHHOST23")||count!=restored.size())throw mahoyo::save::Error("Invalid saved layer extension");legacy=false;hostSchema=signature=="WOHHOST23"?23:signature=="WOHHOST22"?22:signature=="WOHHOST21"?21:signature=="WOHHOST20"?20:signature=="WOHHOST19"?19:signature=="WOHHOST18"?18:signature=="WOHHOST17"?17:signature=="WOHHOST16"?16:signature=="WOHHOST15"?15:signature=="WOHHOST14"?14:signature=="WOHHOST13"?13:signature=="WOHHOST12"?12:signature=="WOHHOST11"?11:signature=="WOHHOST10"?10:signature=="WOHHOST9"?9:signature=="WOHHOST8"?8:signature=="WOHHOST7"?7:signature=="WOHHOST6"?6:signature=="WOHHOST5"?5:signature=="WOHHOST4"?4:3;
                std::vector<int> seen;
                for(size_t i=0;i<count;++i){int id;if(!(data>>id)||!restored.count(id)||std::find(seen.begin(),seen.end(),id)!=seen.end())throw mahoyo::save::Error("Invalid saved layer ID");seen.push_back(id);auto& l=restored.at(id);
                    if(!(data>>l.rotation>>l.pivotX>>l.pivotY>>l.baseAlpha>>l.enabled>>std::quoted(l.baseImage)>>l.variantX>>l.variantY>>l.colorMode>>l.colorValue>>l.rgb)||(hostSchema>=4&&!(data>>l.brightness))||!(data>>l.blend>>l.blendFlag>>l.decimalMode)||(hostSchema>=6&&!(data>>l.targetOffsetX>>l.targetOffsetY>>l.targetOffsetDivisor))||!std::isfinite(l.rotation)||!std::isfinite(l.pivotX)||!std::isfinite(l.pivotY)||!std::isfinite(l.baseAlpha)||(!l.baseImage.empty()&&!safeName(l.baseImage))||!supportedColorMode(l.colorMode)||!supportedRootBlendMode(l.blend))throw mahoyo::save::Error("Invalid saved layer properties");
                    if(hostSchema>=9){for(auto& value:l.window)if(!(data>>value))throw mahoyo::save::Error("Invalid saved window fields");validateWindow(l.window);if(!(data>>l.parent)||l.parent< -1||l.parent>=64||l.parent==l.id)throw mahoyo::save::Error("Invalid saved parent layer");}
                    if(hostSchema>=12){if(!(data>>l.drawOffsetX>>l.drawOffsetY>>l.uniformScale>>l.axisScaleX>>l.axisScaleY)||!std::isfinite(l.drawOffsetX)||!std::isfinite(l.drawOffsetY)||!std::isfinite(l.uniformScale)||!std::isfinite(l.axisScaleX)||!std::isfinite(l.axisScaleY))throw mahoyo::save::Error("Invalid saved separated image geometry");}
                }
            }
            if(hostSchema<12)for(auto& pair:restored){auto& l=pair.second;auto size=legacyIntrinsicSize(l);l.drawOffsetX=-size[0]*l.pivotX;l.drawOffsetY=-size[1]*l.pivotY;l.uniformScale=1;l.axisScaleX=l.w/size[0];l.axisScaleY=l.h/size[1];}
            size_t count=0;if(!(data>>count)||count>64)throw mahoyo::save::Error("Invalid saved effects");
            for(size_t i=0;i<count;++i){Effect e{};uint64_t elapsed;if(!(data>>e.id>>e.clip>>elapsed>>e.x>>e.y>>e.w>>e.h>>e.alpha)||(!legacy&&!(data>>e.rotation))||(hostSchema>=4&&!(data>>e.sourceW>>e.sourceH))||e.id<0||e.id>63||!restored.count(e.id)||!effectClips.count(e.clip)||!std::isfinite(e.x)||!std::isfinite(e.y)||!std::isfinite(e.w)||!std::isfinite(e.h)||!std::isfinite(e.alpha)||!std::isfinite(e.rotation)||((e.w==0||e.h==0)&&hostSchema<12)||elapsed>86400000)throw mahoyo::save::Error("Invalid saved effect playback");if(hostSchema<4){auto& layer=restored.at(e.id);auto size=legacyIntrinsicSize(layer);e.sourceW=size[0];e.sourceH=size[1];}if(!std::isfinite(e.sourceW)||!std::isfinite(e.sourceH)||e.sourceW<=0||e.sourceH<=0)throw mahoyo::save::Error("Invalid saved effect dimensions");if(hostSchema>=9){for(auto& value:e.window)if(!(data>>value))throw mahoyo::save::Error("Invalid saved effect window");validateWindow(e.window);}else e.window=restored.at(e.id).window;
                auto& p=e.properties;const auto& savedLayer=restored.at(e.id);e.pivotX=savedLayer.pivotX;e.pivotY=savedLayer.pivotY;
                if(hostSchema>=11){
                    if(!(data>>p.originX>>p.originY>>p.blendMode>>p.colorMode>>p.rgb>>p.variantX>>p.variantY>>p.brightness>>p.contrast>>p.priority>>e.pivotX>>e.pivotY)||!std::isfinite(e.pivotX)||!std::isfinite(e.pivotY)||!supportedColorMode(p.colorMode)||p.rgb>0xffffff||p.blendMode<0||p.blendMode>25||p.priority<0||p.priority>=0x1000000||p.variantX<0||p.variantY<0||p.variantX>32767||p.variantY>32767)throw mahoyo::save::Error("Invalid saved effect initial properties");
                }else{
                    //Old saves have no original property baseline. Defaults
                    //are recoverable only for unselected channels; an active
                    //property track cannot be reconstructed from its result.
                    const auto& clip=effectClips.at(e.clip);
                    constexpr uint64_t propertyMask=0x10002ull|0x20004ull|0x02000000ull|0x200ull|0x20000000ull|0x40000000ull|0x40ull|0x100ull|0x1000ull|1ull;
                    if(clip.propertiesMetadata&&(clip.descriptor&propertyMask))throw mahoyo::save::Error("Legacy active effect lacks initial property state; use a stable dialogue checkpoint");
                    p=effectProperties(savedLayer,e.sourceW,e.sourceH);
                }
                if(hostSchema>=12){if(!(data>>e.drawOffsetX>>e.drawOffsetY>>e.uniformScale>>e.axisScaleX>>e.axisScaleY)||!std::isfinite(e.drawOffsetX)||!std::isfinite(e.drawOffsetY)||!std::isfinite(e.uniformScale)||!std::isfinite(e.axisScaleX)||!std::isfinite(e.axisScaleY))throw mahoyo::save::Error("Invalid saved effect separated geometry");}
                else{e.drawOffsetX=-e.sourceW*e.pivotX;e.drawOffsetY=-e.sourceH*e.pivotY;e.uniformScale=1;e.axisScaleX=e.w/e.sourceW;e.axisScaleY=e.h/e.sourceH;if(hostSchema>=11){p.originX=-p.originX;p.originY=-p.originY;}}
                if(hostSchema>=19&&!(data>>p.decimalMode))throw mahoyo::save::Error("Invalid saved effect noise baseline");savedEffectNoise(e,hostSchema,savedLayer);
                mahoyo::effects::prepareTime(effectClips.at(e.clip),elapsed);
                e.begin=tick-elapsed;restoredEffects.push_back(e);}
            if(hostSchema>=7)restoredPreloads=readPreloads(data);
            if(hostSchema>=8){std::string tag;if(!(data>>tag>>restoredWindowVisible>>restoredWindowAlpha)||(tag!="WINDOWFADE1"&&tag!="WINDOWFADE2")||restoredWindowAlpha<0||restoredWindowAlpha>65536)throw mahoyo::save::Error("Invalid saved window fade state");if(tag=="WINDOWFADE2"&&(!(data>>restoredWindowMode)||(restoredWindowMode!=0&&restoredWindowMode!=1)))throw mahoyo::save::Error("Invalid saved text mode");}
            if(hostSchema>=9){std::string tag;size_t count;if(!(data>>tag>>count)||tag!="EFFECTFRAMES1"||count>28141)throw mahoyo::save::Error("Invalid saved keyframe overrides");
                std::set<int> available;for(const auto& pair:effectClips)for(const auto& frame:pair.second.frames)if(frame.globalIndex>=0)available.insert(frame.globalIndex);
                for(size_t i=0;i<count;++i){int index;mahoyo::effects::FrameOverride f;if(!(data>>index>>f.x>>f.y>>f.alpha>>f.window[0]>>f.window[1]>>f.window[2]>>f.window[3])||!available.count(index)||f.x<INT16_MIN||f.x>INT16_MAX||f.y<INT16_MIN||f.y>INT16_MAX||f.alpha<0||f.alpha>255||!restoredFrames.emplace(index,f).second)throw mahoyo::save::Error("Invalid saved captured keyframe");for(auto v:f.window)if(v<INT16_MIN||v>INT16_MAX)throw mahoyo::save::Error("Invalid saved captured window");}}
            if(hostSchema>=10){std::string tag,epoch;if(!(data>>tag>>restoredIntermission)||tag!="TEATIME1"||restoredIntermission<0||restoredIntermission>13||!(data>>tag>>epoch)||tag!="COMMON1"||epoch.empty()||epoch.size()>20||epoch.find_first_not_of("0123456789")!=std::string::npos)throw mahoyo::save::Error("Invalid saved intermission/shared context");restoredCommonGeneration=std::stoull(epoch);}
            if(hostSchema>=12){std::string tag;
                if(data>>tag){size_t count;if(tag!="GEOMETRYMODES1"||!(data>>count)||count!=restored.size())throw mahoyo::save::Error("Invalid saved scale matrix modes");std::set<int> ids;for(size_t i=0;i<count;++i){int id,mode;if(!(data>>id>>mode)||!restored.count(id)||!ids.insert(id).second||(mode!=0&&mode!=1))throw mahoyo::save::Error("Invalid saved scale matrix mode");restored.at(id).matrixMode=mode;}}
                else if(hostSchema>=13)throw mahoyo::save::Error("Missing saved geometry footer");
            }
            if(hostSchema>=13){std::string tag;size_t count;if(!(data>>tag>>count)||tag!="WAVES1"||count>64)throw mahoyo::save::Error("Invalid saved wave footer");
                std::vector<mahoyo::wave::Record> records;for(size_t i=0;i<count;++i){mahoyo::wave::Record w;uint64_t phase;if(!(data>>w.layer>>w.preset>>w.fadeMs>>w.strengthElapsed>>w.stopping>>w.drawStarted>>phase)||phase>UINT32_MAX)throw mahoyo::save::Error("Invalid saved wave record");w.phaseMs=uint32_t(phase);validateWaveLayer(restored,w.layer,w.preset);records.push_back(w);}
                restoredWaves.restore(records,tick);
            }
            if(hostSchema>=14){std::string tag;if(!(data>>tag>>restoredPulseCache.x>>restoredPulseCache.y)||tag!="LOCAL270CACHE1"||!std::isfinite(restoredPulseCache.x)||!std::isfinite(restoredPulseCache.y))throw mahoyo::save::Error("Invalid saved local270 cache");}
            if(hostSchema>=15){std::string tag;if(!(data>>tag>>restoredFontCarry)||tag!="FONTCARRY1"||(restoredFontCarry!=0&&restoredFontCarry!=1))throw mahoyo::save::Error("Invalid saved font carry");}
            if(hostSchema>=22){std::string tag;int applied;if(!(data>>tag>>applied)||tag!="EXACTLAYOUT1"||(applied!=0&&applied!=1))throw mahoyo::save::Error("Invalid saved exact layout marker");restoredExactLayout=applied!=0;}
            if(hostSchema>=16){std::string tag,generation;size_t count;
                if(!(data>>tag>>generation>>count)||tag!="SCREENWAVE1"||generation.empty()||generation.size()>20||generation.find_first_not_of("0123456789")!=std::string::npos||count>1)throw mahoyo::save::Error("Invalid saved Screen Raster footer");
                const auto identity=std::stoull(generation);std::optional<mahoyo::screen_wave::Record> record;
                if(count){mahoyo::screen_wave::Record r;r.id=identity;int stopping,started;uint64_t phase;
                    if(!(data>>r.preset>>r.fadeMs>>r.strengthElapsed>>stopping>>started>>phase)||(stopping!=0&&stopping!=1)||(started!=0&&started!=1)||phase>UINT32_MAX)throw mahoyo::save::Error("Invalid saved Screen Raster record");
                    r.stopping=stopping!=0;r.drawStarted=started!=0;r.phaseMs=uint32_t(phase);record=r;validateScreenWave(restored);}
                restoredScreen.restore(record,identity,tick);
            }
            if(hostSchema>=17)restoredHistoryMeta=mahoyo::history::read(data,document.history.size());
            if(hostSchema>=18)restoredWaitClock=mahoyo::wait_clock::Clock::read(data,tick);
            if(hostSchema>=18)restoredEffectBases=readStoppedEffectBases(data,restored,restoredEffects,hostSchema);
            if(hostSchema>=19){restoredInput=mahoyo::input_region::Controller::read(data,[&](uint32_t target){candidate->validateExternalTarget(target);});std::set<int> ids;for(const auto& pair:restored)ids.insert(pair.first);restoredBitmapSizes=mahoyo::bitmap_geometry::readSavedSizes(data,ids);}
            if(hostSchema>=21){
                std::string tag;size_t count;
                if(!(data>>tag>>count)||tag!="AUDIOPAN1"||count!=document.audio.size())throw mahoyo::save::Error("Invalid saved audio pan footer");
                std::set<std::string> channels;for(const auto& a:document.audio)if(!channels.insert(a.channel).second)throw mahoyo::save::Error("Duplicate saved audio channel");
                for(size_t i=0;i<count;++i){std::string channel;double pan;
                    if(!(data>>std::quoted(channel)>>pan)||!std::isfinite(pan)||pan<0||pan>128||channels.erase(channel)!=1)throw mahoyo::save::Error("Invalid saved audio pan");
                    restoredAudioPans.emplace(channel,pan);
                }
            }
            if(hostSchema>=20){std::vector<int> activeIds;for(const auto& e:restoredEffects)activeIds.push_back(e.id);std::set<int> layerIds;for(const auto& pair:restored)layerIds.insert(pair.first);restoredHost20=mahoyo::host_state20::read(data,activeIds,layerIds);}
            if(hostSchema>=23){restoredLanguage=mahoyo::language::readTextState(data,document.history.size());if((restoredLanguage.edition!="vanilla"&&restoredLanguage.edition!="korean")||!mahoyo::language::supported(restoredLanguage.edition,restoredLanguage.language))throw mahoyo::save::Error("Saved game edition/language mismatch");}
            std::string trailing;if(data>>trailing)throw mahoyo::save::Error("Unexpected saved effect data");
        }
        validateParents(restored);
        if(hostSchema<5)for(auto& pair:restored){auto& l=pair.second;auto oldSize=legacyIntrinsicSize(l);auto extent=anchorExtent(l,&oldSize);double oldX=l.pivotX,oldY=l.pivotY;l.pivotX*=extent[0];l.pivotY*=extent[1];l.x+=l.w*(oldX-l.pivotX);l.y+=l.h*(oldY-l.pivotY);for(auto& e:restoredEffects)if(e.id==l.id){e.x+=e.w*(oldX-l.pivotX);e.y+=e.h*(oldY-l.pivotY);e.pivotX=l.pivotX;e.pivotY=l.pivotY;e.properties=effectProperties(l,e.sourceW,e.sourceH);}}
        if(hostSchema<5)for(auto& pair:restored){auto& l=pair.second;auto size=legacyIntrinsicSize(l);l.drawOffsetX=-size[0]*l.pivotX;l.drawOffsetY=-size[1]*l.pivotY;l.uniformScale=1;l.axisScaleX=l.w/size[0];l.axisScaleY=l.h/size[1];for(auto& e:restoredEffects)if(e.id==l.id){e.drawOffsetX=-e.sourceW*e.pivotX;e.drawOffsetY=-e.sourceH*e.pivotY;e.uniformScale=1;e.axisScaleX=e.w/e.sourceW;e.axisScaleY=e.h/e.sourceH;}}
        for(auto& pair:restored)migrateBitmapLayer(pair.second,hostSchema>=19?std::optional<mahoyo::bitmap_geometry::Size>(restoredBitmapSizes.at(pair.first)):std::nullopt);
        for(auto& e:restoredEffects)migrateBitmapEffect(e,restored);
        for(auto& pair:restoredEffectBases){auto& e=pair.second;migrateBitmapEffect(e,restored);auto base=effectBase(restored.at(e.id),e);auto size=intrinsicSize(base);if(e.sourceW!=size[0]||e.sourceH!=size[1])throw mahoyo::save::Error("Invalid saved stopped effect intrinsic geometry");}
        // Migration cannot invent completed registrations, paused jobs or old
        // pools/RNG absent from3..19. State defaults encode the port seed1/off policy.
        if(hostSchema<20)for(const auto& e:restoredEffects)restoredHost20.registeredEffects.insert(e.id);
        for(auto& e:restoredEffects){e.paused=restoredHost20.pausedEffects.count(e.id)!=0;e.pausedAt=e.paused?tick:0;}
        auto restoredParticles=particleOverlays;restoredParticles.restore(restoredHost20.fallNote,restoredHost20.focusLine,tick);
        // Validate resources before flushShared/current-state mutation, while
        // keeping the same prepared bitmap bounds as commitParticles().
        const auto restoredParticleFrame=restoredParticles.frame();const std::array<std::array<int,2>,3> fallSizes{{{{13512,187}},{{4500,112}},{{3600,150}}}};
        for(size_t i=0;i<restoredParticleFrame.fallNote.resources.size();++i)if(imageSize(restoredParticleFrame.fallNote.resources[i])!=fallSizes[i])throw mahoyo::save::Error("Unsupported saved FallNote bitmap geometry");
        if(!restoredParticleFrame.focusLine.resources.empty()&&imageSize(restoredParticleFrame.focusLine.resources.front())!=std::array<int,2>{{960,2}})throw mahoyo::save::Error("Unsupported saved FocusLine bitmap geometry");
        if(hostSchema>=5&&!restoredExactLayout){
            for(auto& pair:restored)migrateExactLayout(pair.second);
            for(auto& e:restoredEffects){auto base=effectBase(restored.at(e.id),e);migrateExactLayout(base);captureEffectBase(e,base);}
            for(auto& pair:restoredEffectBases){auto& e=pair.second;auto base=effectBase(restored.at(e.id),e);migrateExactLayout(base);captureEffectBase(e,base);}
        }
        auto restoredText=legacy?mahoyo::text::resolveReferences(document.text,table).text:document.text;auto restoredHistory=document.history;
        if(legacy)for(auto& item:restoredHistory)item=mahoyo::text::resolveReferences(item,table).text;
        restoredText=mahoyo::text::compactDialogueSpacing(mahoyo::text::stripDisplayControls(restoredText));

        for(auto& item:restoredHistory)item=mahoyo::text::compactDialogueSpacing(mahoyo::text::stripDisplayControls(item));
        if(hostSchema>=23){
            restoredText=mahoyo::language::renderSources(restoredLanguage.paragraph,table);
            for(size_t i=0;i<restoredHistory.size();++i)restoredHistory[i]=restoredLanguage.history[i].prefix+mahoyo::language::displayText(restoredLanguage.history[i].raw,table);
        }else{
            // Legacy displayed strings have no reliable source identity. Preserve them exactly.
            restoredLanguage.paragraph.push_back({restoredText,""});
            for(const auto& item:restoredHistory)restoredLanguage.history.push_back({item,""});
        }
        const bool restoredParagraphItalic=mahoyo::text::endingItalic(restoredText);
        // Cross-edition loads preserve PC/stack only when every module's code is identical.
        if(languageAssets.edition()=="korean"||(hostSchema>=23&&restoredLanguage.edition!=languageAssets.edition()))candidate->rebindModuleStrings(mahoyo::language::scriptPrefixes());
        int restoredArchive=0,restoredCheckpoint=0;
        if(candidate->variable(500)==1&&candidate->variable(26)>0){restoredArchive=candidate->variable(26);restoredCheckpoint=candidate->variable(459);if(!catalog().find(restoredArchive)||restoredCheckpoint<0||restoredCheckpoint>13)throw mahoyo::save::Error("Invalid saved Archive context");}
        flushShared();
        const bool titleOnlyCommon=sharedLoaded&&onlyInitialTitleCommon();
        if(titleOnlyCommon)pendingFrontendFlags.insert(321);
        bool useShared=sharedLoaded&&!titleOnlyCommon&&sharedState.generation>=restoredCommonGeneration;
        restoredNoise.reconcile(noiseConfiguration(restored,restoredWaves,candidate.get()));
        if(useShared)candidate->restoreShared(sharedState.variables,sharedState.flags);
        else{sharedState={restoredCommonGeneration,candidate->sharedVariables(),candidate->sharedFlags()};sharedLoaded=restoredCommonGeneration!=0;sharedWritePending=true;}
        intermission.reset();opening.stop();alphaRules.remove(-1);pendingEffectCaptures.clear();frameOverrides=std::move(restoredFrames);resetGlobalShake();machine=std::move(candidate);layers=std::move(restored);preloads=std::move(restoredPreloads);preloadsDirty=true;paragraph=std::move(restoredText);paragraphSources=std::move(restoredLanguage.paragraph);historySources=std::move(restoredLanguage.history);paragraphItalic=restoredParagraphItalic;fontCarryItalic=restoredFontCarry!=0;history=std::move(restoredHistory);textLine=int(document.textLine);hidden=document.hidden;
        choices.reset();historyMeta=std::move(restoredHistoryMeta);historyChanged();resetHistorySession();
        waitClock=restoredWaitClock;inputRegion.restore(restoredInput);inputJumpRelease=0;
        windowFade.reset(restoredWindowVisible,restoredWindowAlpha);windowMode=restoredWindowMode;pendingWindowMode.reset();
        reveal.reset(paragraph,tick,true);
        screenWave=std::move(restoredScreen);waitingScreenWave=false;wavePlatformSeen=false;
        archiveReplay=restoredArchive;archiveSelected=restoredArchive;archiveCheckpoint=restoredCheckpoint;
        mode="play";wait="input";transition.active=false;error.clear();cancelLayer(-1);stoppedEffectBases=std::move(restoredEffectBases);layerPulseCache=restoredPulseCache;noise=std::move(restoredNoise);waves=std::move(restoredWaves);effects=std::move(restoredEffects);resetEffectRegistry();registeredEffectIds=std::move(restoredHost20.registeredEffects);particleOverlays=std::move(restoredParticles);randomState=restoredHost20.randomState;automatic=false;skipping=false;deadline=tick+uint64_t(autoDelay);resetAudio();activeSlot=slot;
        for(const auto& a:document.audio){sound(a.name,a.channel,a.loop);audio.back().volume=a.volume;audio.back().pan=hostSchema>=21?restoredAudioPans.at(a.channel):64;}
        if(restoredIntermission){intermission.restoreReady(++intermissionGeneration,restoredIntermission);mode="teatime";wait="teatime";havePlatformTime=false;}
        if(flushShared())notice="Loaded";++version;
    }
    Flow invoke(uint32_t op,Machine& vm) override {
        //Only the proven nonvisual253 -> normal240/reset/delete transaction is supported.
        if(!pendingColors.empty()&&op!=0x253&&op!=0x240&&op!=0x241)
            throw mahoyo::vm::Error("Unsupported unresolved nonvisual color request");
        if(inputRegion.restricted()&&op>=0x160&&op<=0x164)throw mahoyo::vm::Error("Unreviewed script choice/input-region combination");
        switch(op) {
        case 0x12c:return Flow::Halt;
        case 0x12a:vm.setVariable(0x462,vm.pop());return Flow::Continue;
        case 0x128:vm.setFlag(uint32_t(vm.pop()),true,true);return Flow::Continue;
        case 0x118:
            inputRegion.begin118();automatic=skipping=false;wait="inputRegionReady";havePlatformTime=false;++version;return Flow::Yield;
        case 0x12f:vm.push(mahoyo::language::slot(languageAssets.current()));return Flow::Continue;
        case 0x120:
            if(!inputRegion.begin120())return Flow::Continue;
            automatic=skipping=false;wait="inputRegionReady";havePlatformTime=false;++version;return Flow::Yield;
        case 0x121:inputRegion.release121();return Flow::Continue;
        case 0x122:{
            bool enabled=vm.peek()>=0;auto target=uint32_t(vm.peek(1));
            if(enabled)vm.validateExternalTarget(target);
            vm.pop();vm.pop();inputRegion.request122(enabled,target);return Flow::Continue;
        }
        case 0x130:vm.push(archiveReplay?1:0);return Flow::Continue; // Native context replay mode, separate from StoryId.
        case 0x131:vm.push(0);return Flow::Continue;
        case 0x138:unlockTrophy(vm.pop(),vm);flushShared();return Flow::Continue;
        case 0x13c:{int selector=vm.pop();intermission.enter(++intermissionGeneration,selector,tick);
            if((selector==1||selector==8||selector==9)&&vm.sharedVariables().size()>26){auto seen=uint32_t(vm.variable(26,true));vm.setVariable(26,int32_t(seen|(uint32_t(1)<<selector)),true);}
            flushShared();mode="teatime";wait="teatime";automatic=false;skipping=false;havePlatformTime=false;++version;return Flow::Yield;}
        case 0x300:
            // shortcut: only the proven neutral global filter; add state/prepare/save support before accepting nonzero.
            if(vm.peek()!=0)throw mahoyo::vm::Error("Unsupported non-neutral global image filter");
            vm.pop();return Flow::Continue;
        case 0x340: {
            mahoyo::effects::GlobalShakeRequest request;request.amplitudeX=vm.pop();request.amplitudeY=vm.pop();request.periodMs=vm.pop();request.signedCycles=vm.pop();request.delayMs=vm.pop();
            globalShake.queue(request);if(globalShakeClear)globalShakeAfterClear=request;return Flow::Continue;
        }
        case 0x390:case 0x391:case 0x3a0:
            if(opening.active())throw mahoyo::vm::Error("Unsupported particles/movie combination");
            if(op==0x390)particleOverlays.requestFallNote(0);
            else if(op==0x391)particleOverlays.requestFallNote391(vm.pop());
            else{for(int i=0;i<5;++i)vm.pop();particleOverlays.requestFocusStart();}
            return Flow::Continue;
        case 0x392:particleOverlays.requestFallNote(-1);return Flow::Continue;
        case 0x3a1:particleOverlays.requestFocusStop();return Flow::Continue;
        case 0x380:{int first=vm.pop(),hour=vm.pop(),minute=vm.pop(),x=vm.pop(),y=vm.pop(),scale=vm.pop(),color=vm.pop();createClock(first,hour,minute,x,y,scale,color);return Flow::Continue;}
        case 0x381:{int first=vm.pop(),hour=vm.pop(),minute=vm.pop();if(uint32_t(first)<=58){auto target=mahoyo::clock::hands(hour,minute);for(int i=1;i<=4;++i)pendingClockRotations.insert_or_assign(first+i,target[size_t((i-1)/2)]);}return Flow::Continue;}
        case 0x341:vm.pop();globalShakeClear=true;globalShakeAfterClear.reset();return Flow::Continue;
        case 0x112:waitClock.request();return Flow::Continue;
        case 0x110: {int duration=vm.pop(),noInterrupt=vm.pop(),clockMode=vm.pop();(void)noInterrupt;pulseWaitLayer.reset();effectWaitLayer.reset();
            if(inputRegion.stable().pendingDirty||alphaRules.pending()||waitClock.pending()||!pendingNoise.empty()||!pendingLayerPulses.empty()||screenWave.pending()||!pendingEffectClears.empty()||!pendingEffectCaptures.empty()||!pendingRelativeMotions.empty()||!pendingEffectRegistrations.empty()||!pendingEffectRegistryDeletes.empty()||!pendingEffectPause.empty()||particleOverlays.pending()){
                //Native07ED60→084590→160410→1460C0 kind7→146570.
                //Use the existing prepared-scene acknowledgement, excluding
                //resource loading from each local job's delay/playback clock.
                prepareAlphaRules();commitEffectCaptures();commitWindow();prepareNoise();transition=Transition{++transitionId,tick,0,true,"[]","[]",1};transition.waitAfterReady="frame";transition.wait110=true;transition.wait110Duration=duration;transition.wait110Mode=clockMode;wait="transitionReady";return Flow::Yield;
            }
            const auto remaining=mahoyo::wait_clock::Clock::remaining(duration,clockMode,prepareWaitClock());
            wait="frame";deadline=tick+remaining;return remaining?Flow::Yield:Flow::Continue;}
        case 0x111: {int duration=vm.pop(),alternative=vm.pop();if(duration<0)duration=alternative;wait="frame";deadline=tick+uint64_t(std::max(0,duration));return duration>0?Flow::Yield:Flow::Continue;}
        case 0x141:
            paragraph.clear();paragraphSources.clear();paragraphItalic=false;reveal.reset(paragraph,tick);wait="frame";deadline=tick+16;++version;return Flow::Yield;
        case 0x142:historyMeta.addBreak();paragraph+='\n';paragraphSources.push_back({"\n",""});reveal.append(paragraph,tick);++version;return Flow::Continue;
        case 0x14b:fontCarryItalic=false;return Flow::Continue;
        case 0x228: {recordSeenImage(vm.popString());return Flow::Continue;}
        case 0x251: {int id=vm.pop();int value=vm.pop();if(id>=0&&id<64){
            layers[id].z=value;
            auto active=std::find_if(effects.begin(),effects.end(),[&](const Effect& e){return e.id==id;});
            if(active!=effects.end()){effectBaseBeforePendingEdits.emplace(id,*active);effectPendingBaseEdits[id].push_back([=](Layer& base){base.z=value;});}
            else if(stoppedEffectBases.count(id))editEffectBase(id,[=](Layer& base){base.z=value;});
            }return Flow::Continue;}
        case 0x250: {
            int id=vm.pop(),x=vm.pop(),y=vm.pop(),rotation=vm.pop();auto& l=layers.at(id);auto size=l.image.empty()?std::array<int,2>{1920,1080}:imageSize(l.image);
            double px=l.pivotX*size[0]/3.,py=l.pivotY*size[1]/3.;
            if(x!=INT32_MIN)px=double((int64_t(x)*640)/480);
            if(y!=INT32_MIN)py=double((int64_t(y)*480)/360);
            setResourceOrigin(l,px,py);
            editPendingEffectBase(id,[=](Layer& base){auto size=intrinsicSize(base);double bx=base.pivotX*size[0]/3.,by=base.pivotY*size[1]/3.;if(x!=INT32_MIN)bx=double((int64_t(x)*640)/480);if(y!=INT32_MIN)by=double((int64_t(y)*480)/360);setResourceOrigin(base,bx,by);if(rotation!=INT32_MIN)base.rotation=nativeAngle(rotation);});
            if(rotation!=INT32_MIN)l.rotation=nativeAngle(rotation);return Flow::Continue;
        }
        case 0x25a: {int id=vm.pop(),value=vm.pop();if(id>=0&&id<64)layers[id].enabled=value!=0;return Flow::Continue;}
        case 0x252: {int id=vm.pop(),raw=vm.pop();vm.pop();int flag=vm.pop();int blend=raw>=1&&raw<=25?raw:0;if(!supportedRootBlendMode(blend))throw mahoyo::vm::Error("Unsupported blend mode "+std::to_string(blend));auto& l=layers.at(id);int normalizedFlag=flag==1||(flag==-1&&raw==5)?1:0;if(l.parent>=0&&(!supportedWindowChildBlend(blend,normalizedFlag)))throw mahoyo::vm::Error("Unsupported native window child blend mode/flag");l.blend=blend;l.blendFlag=normalizedFlag;editPendingEffectBase(id,[=](Layer& base){base.blend=blend;});return Flow::Continue;}
        case 0x253: {
            int id=vm.pop(),mode=vm.pop(),rgb=vm.pop();
            if(!supportedColorMode(mode))throw mahoyo::vm::Error("Unsupported color mode "+std::to_string(mode));
            if(uint32_t(id)>=64)return Flow::Continue; //Native unsigned layer guard, after3 pops.
            auto found=layers.find(id);
            if(found==layers.end()){pendingColors[id]=PendingColor{mode,rgb&0xffffff};return Flow::Continue;}
            auto& l=found->second;l.colorMode=mode;l.rgb=rgb&0xffffff;
            editPendingEffectBase(id,[=](Layer& base){base.colorMode=mode;base.rgb=rgb&0xffffff;});
            return Flow::Continue;
        }
        case 0x254: {int id=vm.pop(),value=vm.pop();layers.at(id).colorValue=value;editPendingEffectBase(id,[=](Layer& base){base.colorValue=value;});return Flow::Continue;}
        case 0x255: {int id=vm.pop(),value=vm.pop();layers.at(id).brightness=value;editPendingEffectBase(id,[=](Layer& base){base.brightness=value;});return Flow::Continue;}
        case 0x25b: {
            if(vm.stack().size()<4)throw mahoyo::vm::Error("Native alpha-rule operand underflow");
            int id=vm.peek(),rule=vm.peek(1),middle=vm.peek(2),strength=vm.peek(3);
            if(uint32_t(id)<64){if(rule==-1&&middle==0&&strength==256)alphaRules.clear(id);
            else{auto at=layers.find(id);if(at==layers.end())throw mahoyo::vm::Error("Mask7 missing layer");alphaRules.request(id,rule,middle,strength,alphaContext(at->second));}}
            for(int n=0;n<4;++n)vm.pop();return Flow::Continue;}
        case 0x259: {
            int id=vm.pop(),x=vm.pop(),y=vm.pop();vm.pop();vm.pop();int divisor=vm.pop();vm.pop();
            auto& l=layers.at(id);l.targetOffsetX=x;l.targetOffsetY=y;l.targetOffsetDivisor=divisor;return Flow::Continue;
        }
        //Native noise config is prepared at scene barriers; GL owns raster phase.
        case 0x257: {int id=vm.pop(),value=vm.pop();if(id>=0&&id<64){mahoyo::noise::decode(value);layers.at(id).decimalMode=value;editPendingEffectBase(id,[value](Layer& base){base.decimalMode=value;});pendingNoise.insert(id);}return Flow::Continue;}
        case 0x270: {
            int id=vm.pop(),x=vm.pop(),y=vm.pop(),period=vm.pop(),duration=vm.pop(),delay=vm.pop(),mode=vm.pop();
            if(id>=64)return Flow::Continue;
            if((mode!=0&&mode!=1&&mode!=2&&mode!=3&&mode!=4&&mode!=5&&mode!=6)||((mode==3||mode==4||mode==5||mode==6)&&id<0))throw mahoyo::vm::Error("Unsupported advanced shake variant");
            mahoyo::effects::LayerShakeMode2 parameters{x,y,period,duration};
            if(x||y){if(delay<0)throw mahoyo::vm::Error("Unsupported negative layer shake delay");if(mode==3)mahoyo::effects::sampleLayerShakeMode3(parameters,0,{});else if(mode==2)mahoyo::effects::sampleLayerShakeMode2(parameters,0,{});else{mahoyo::effects::LayerShakeRandom probe;probe.sample(parameters,0,{},[]{return uint32_t(0);});}}
            if(id<0&&mode==1&&(delay<0||period<=0))throw mahoyo::vm::Error("Unsupported global follower shake timing");
            if(id<0){for(int layer=0;layer<64;++layer){auto p=parameters;if(mode==1&&layer>=2)p.amplitudeX=p.amplitudeY=-1;pendingLayerPulses.insert_or_assign(layer,PendingLayerPulse{p,delay,false,mode==2?2:0});}}
            else pendingLayerPulses.insert_or_assign(id,PendingLayerPulse{parameters,delay,false,mode==6?6:mode==5?5:mode==4?4:mode==3?3:mode==2?2:0});
            return Flow::Continue;
        }
        case 0x274: {int id=vm.pop();if(id>=0&&id<64)pendingLayerPulses.insert_or_assign(id,PendingLayerPulse{{-1,-1,0,-1},0,false,LayerPulseMotion274});return Flow::Continue;}
        case 0x272: {int id=vm.pop();if(id<0){for(int i=0;i<64;++i)pendingLayerPulses.insert_or_assign(i,PendingLayerPulse{{},0,true});}else if(id<64)pendingLayerPulses.insert_or_assign(id,PendingLayerPulse{{},0,true});return Flow::Continue;}
        case 0x241: {int id=vm.pop();auto retainedCache=layerPulseCache;cancelLayer(id);if(id<0){layers.clear();layerPulseCache=retainedCache;}else{for(auto& pair:layers)if(pair.second.parent==id)pair.second.parent=-1;layers.erase(id);}++version;return Flow::Continue;}
        case 0x245: {
            int id=vm.pop(),start=vm.pop(),end=vm.pop(),duration=vm.pop(),curve=vm.pop();
            ease(0,curve);
            if(alphaRules.contains(id)){alphaRules.fade(id,start,end,duration,curve,layers.at(id).alpha);return Flow::Continue;}
            auto& l=layers[id];double from=start==INT32_MIN?l.alpha:std::clamp(start/255.,0.,1.);
            double to=end==INT32_MIN?l.alpha:std::clamp(end/255.,0.,1.);
            fades.erase(std::remove_if(fades.begin(),fades.end(),[&](const Animation&a){return a.id==id;}),fades.end());
            l.alpha=duration>0?from:to;if(duration<=0)editPendingEffectBase(id,[=](Layer& base){base.alpha=to;});
            if(duration>0)fades.push_back(Animation{id,from,to,tick,tick+uint64_t(duration),curve});
            return Flow::Continue;
        }
        case 0x247: {
            std::array<int,7> args{};for(auto& n:args)n=vm.pop();
            auto& l=layers[args[0]];ease(0,args[4]);
            double from=args[1]==INT32_MIN?l.rotation:nativeAngle(args[1]);double to=args[2]==INT32_MIN?l.rotation:nativeAngle(args[2]);
            transforms.erase(std::remove_if(transforms.begin(),transforms.end(),[&](const Transform&a){return a.id==args[0]&&a.mask==4;}),transforms.end());
            l.rotation=args[3]>0?from:to;if(args[3]<=0)editPendingEffectBase(args[0],[=](Layer& base){base.rotation=to;});
            if(args[3]>0)transforms.push_back(Transform{args[0],4,from,0,to,0,tick,tick+uint64_t(args[3]),args[4]});
            return Flow::Continue;
        }
        case 0x246: {
            std::array<int,8> args{};for(auto& n:args)n=vm.pop();
            auto& l=layers[args[0]];auto size=l.image.empty()?std::array<int,2>{1920,1080}:imageSize(l.image);ease(0,args[6]);
            if(args[7]!=INT32_MIN){if(args[7]!=0&&args[7]!=1)throw mahoyo::vm::Error("Unsupported native scale matrix mode");l.matrixMode=args[7];}
            double x0=args[1]==INT32_MIN?l.axisScaleX:args[1]/1000.,y0=args[2]==INT32_MIN?l.axisScaleY:args[2]/1000.;
            double x1=args[3]==INT32_MIN?l.axisScaleX:args[3]/1000.,y1=args[4]==INT32_MIN?l.axisScaleY:args[4]/1000.;
            transforms.erase(std::remove_if(transforms.begin(),transforms.end(),[&](const Transform&a){return a.id==args[0]&&a.mask==2;}),transforms.end());
            setAxisScale(l,args[5]>0?x0:x1,args[5]>0?y0:y1);if(args[5]<=0)editPendingEffectBase(args[0],[=](Layer& base){setAxisScale(base,x1,y1);});
            if(args[5]>0)transforms.push_back(Transform{args[0],2,x0,y0,x1,y1,tick,tick+uint64_t(args[5]),args[6],l.matrixMode});
            return Flow::Continue;
        }
        case 0x258: {
            int layer=vm.pop(),preset=vm.pop(),fade=vm.pop();
            if(layer>=64)return Flow::Continue;
            if(layer<0){screenWave.request(preset,fade);if(preset<=0)waves.request(layer,preset,fade);}
            else waves.request(layer,preset,fade);
            return Flow::Continue;
        }
        case 0x27f: {
            commitEffectCaptures();commitWindow();
            int id=vm.pop(),mask=vm.pop(),noInterrupt=vm.pop();(void)noInterrupt;
            uint64_t end=tick;if(mask&8)for(const auto& a:fades)if(a.id==id)end=std::max(end,a.end);
            if(mask&8)end=std::max(end,tick+alphaRules.remaining(id,tick));
            if(mask&1)for(const auto& a:motions)if(a.id==id)end=std::max(end,a.end);
            for(const auto& a:transforms)if(a.mask!=4&&a.id==id&&(mask&a.mask))end=std::max(end,a.end);
            //Native15D610 skips normal looping backend2 events without
            //removing their registration or cancelling the running effect.
            effectWaitLayer=(mask&32)?std::optional<int>(id):std::nullopt;
            if(mask&32){queryEffectRegistry(id);for(const auto& a:effects)if(id<0||a.id==id){const auto& clip=effectClips.at(a.clip);if(clip.loopStart||a.paused)continue;end=std::max(end,a.begin+clip.duration+uint64_t(clip.timeRemap));}}
            //Native27f prepares pending graphics before querying effect jobs.
            //Even an empty/loop-only query must cross this resource barrier;
            //the acknowledgement resumes immediately if no finite job remains.
            prepareNoise();
            transition=Transition{++transitionId,tick,0,true,"[]","[]",1};transition.waitAfterReady="animation";transition.deadlineAfterReady=end;transition.waveWait=(mask&64)!=0;transition.waveWaitLayer=id;transition.clockWait=(mask&4)!=0;transition.clockWaitLayer=id;transition.nonrotationDeadline=end;transition.pulseWait=(mask&128)!=0;transition.pulseWaitLayer=id;
            wait="transitionReady";++version;return Flow::Yield;
        }
        case 0x243: {
            int type=vm.pop(),id=vm.pop(),x0=vm.pop(),y0=vm.pop(),x1=vm.pop(),y1=vm.pop(),duration=vm.pop(),curve=vm.pop();auto anchor=vm.popString();
            if(type!=0)throw mahoyo::vm::Error("Unsupported position animation variant");ease(0,curve);
            pendingRelativeMotions.erase(id);unpreparedAbsoluteMotions.insert(id);
            auto& l=layers[id];auto pivot=imagePivot(l,anchor);double px=pivot[0],py=pivot[1];
            auto oldAnchor=worldAnchor(l);double fromX=x0==INT32_MIN?oldAnchor[0]:x0*4.,fromY=y0==INT32_MIN?oldAnchor[1]:y0*4.;
            double toX=x1==INT32_MIN?oldAnchor[0]:x1*4.,toY=y1==INT32_MIN?oldAnchor[1]:y1*4.;
            motions.erase(std::remove_if(motions.begin(),motions.end(),[&](const Motion&m){return m.id==id;}),motions.end());
            l.pivotX=px;l.pivotY=py;auto dimensions=intrinsicSize(l);l.drawOffsetX=-dimensions[0]*px;l.drawOffsetY=-dimensions[1]*py;placeAt(l,duration>0?fromX:toX,duration>0?fromY:toY);
            editPendingEffectBase(id,[=](Layer& base){auto a=worldAnchor(base);auto dimensions=intrinsicSize(base);base.pivotX=px;base.pivotY=py;base.drawOffsetX=-dimensions[0]*px;base.drawOffsetY=-dimensions[1]*py;placeAt(base,duration>0?a[0]:toX,duration>0?a[1]:toY);});
            if(duration>0)motions.push_back(Motion{id,fromX,fromY,toX,toY,tick,tick+uint64_t(duration),curve,px,py});
            return Flow::Continue;
        }
        case 0x244: {
            if(vm.stack().size()<7)throw mahoyo::vm::Error("Relative position operand underflow");
            int type=vm.pop(),id=vm.pop(),dx=vm.pop(),dy=vm.pop(),duration=vm.pop(),curve=vm.pop();vm.pop();
            //Native emits a diagnostic for absolute type0, with no state write.
            if(type==0||uint32_t(id)>=64)return Flow::Continue;
            if(unpreparedAbsoluteMotions.count(id))throw mahoyo::vm::Error("Unsupported mixed absolute/relative position before prepare");
            if(duration>0)ease(0,curve);
            auto convert=[](int value,uint32_t numerator,int divisor){uint32_t bits=uint32_t(value)*numerator;int64_t product=bits&0x80000000u?int64_t(bits)-0x100000000ll:int64_t(bits);return float(product/divisor);};
            auto& pending=pendingRelativeMotions[id];pending.x=float(pending.x+convert(dx,640,480));pending.y=float(pending.y+convert(dy,480,360));pending.duration=duration;pending.curve=curve;
            return Flow::Continue;
        }
        case 0x27a: {int raw=vm.pop();for(int id:registeredEffectIds)pendingEffectPause.insert_or_assign(id,raw!=0);return Flow::Continue;}
        case 0x279: {
            alphaRules.rejectCapture();
            int id=vm.pop(),clip=vm.pop();if(id<0||id>=64||clip<=0)return Flow::Continue;
            if(!effectClips.count(clip))throw mahoyo::vm::Error("Unsupported native effect ID "+std::to_string(clip));
            auto found=layers.find(id);if(found==layers.end())throw mahoyo::vm::Error("Unsupported effect capture without graphics");
            pendingEffectCaptures.insert_or_assign(id,EffectCapture{clip,found->second,tick});return Flow::Continue;
        }
        case 0x278: {
            int id=vm.pop(),clip=vm.pop(),autoShow=vm.pop();vm.pop();vm.pop();
            if(uint32_t(id)>=64)return Flow::Continue;
            pendingEffectPause.erase(id);pendingEffectRegistryDeletes.insert(id);pendingEffectRegistrations.erase(id);
            if(clip<=0){if(autoShow!=0)throw mahoyo::vm::Error("Unsupported effect cancellation text-window mode");pendingEffectClears.insert(id);return Flow::Continue;}
            if(!effectClips.count(clip))throw mahoyo::vm::Error("Unsupported native effect ID "+std::to_string(clip));
            pendingEffectClears.erase(id);pendingEffectRegistrations.insert(id);
            auto& l=layers[id];effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect&e){return e.id==id;}),effects.end());
            auto baseline=l;auto stopped=stoppedEffectBases.find(id);if(stopped!=stoppedEffectBases.end())baseline=effectBase(l,stopped->second);
            Effect playback{id,clip,tick};captureEffectBase(playback,baseline);stoppedEffectBases.erase(id);effects.push_back(playback);
            if(effectBaseBeforePendingEdits.count(id))effectBaseBeforePendingEdits.insert_or_assign(id,playback);
            applyEffect(playback);return Flow::Continue;
        }
        case 0x151:case 0x158:
            //Same prepared text cleanup as141, plus a pending hide. The native
            //controller has no fixed duration; this CPU-only page cleanup
            //finishes at our next frame boundary. History remains independent.
            windowFade.requestHide(0);paragraph.clear();paragraphSources.clear();paragraphItalic=false;reveal.reset(paragraph,tick);
            wait="frame";deadline=tick+16;++version;return Flow::Yield;
        case 0x152:windowFade.requestHide(vm.pop());return Flow::Continue;
        case 0x153:windowFade.requestShow(vm.pop());return Flow::Continue;
        case 0x154:{int value=vm.pop();pendingWindowMode=value?0:1;if(!value)windowFade.requestHide(0);return Flow::Continue;}
        case 0x10e: {
            vm.pop();
            if(paragraph.empty())return Flow::Continue;
            wait="input";deadline=tick+(skipping?0:uint64_t(autoDelay));++version;return Flow::Yield;
        }
        case 0x200: {
            std::array<int,6> args{};for(auto& v:args)v=vm.pop();
            if((args[0]!=0&&args[0]!=1&&args[0]!=2)||args[1]<0||args[1]>86400000)throw mahoyo::vm::Error("Unsupported native scene transition variant");
            if(args[0]==2&&!reviewedMode2Transition(args))throw mahoyo::vm::Error("Unsupported Native mode2 transition profile");
            commitEffectCaptures();
            commitPreloads();prepareNoise();
            bool fade=(args[0]==0||args[0]==2)&&args[1]>0;
            transition=Transition{++transitionId,tick,fade?args[1]:0,true,fade?presentedLayers:"[]",fade?presentedText:"[]",fade?0:1};wait="transitionReady";
            transition.nativeMode=args[0];transition.nativeStepMs=args[5]==1?17:50;
            transition.movie=fade?presentedMovie:"null";transition.sourceScene=presentedScene;transition.sourceModule=presentedModule;transition.screenWave=fade?presentedScreenWave:"null";transition.particles=fade?presentedParticles:"null";
            if(fade&&args[2]>=1&&args[2]<=14){auto found=masks.find(args[2]);if(found==masks.end())throw mahoyo::vm::Error("Missing prepared native mask map");if(args[4]<0||args[4]>256)throw mahoyo::vm::Error("Unsupported native mask strength");transition.maskId=args[2];transition.strength=args[4];transition.mask=found->second;}
            ++version;return Flow::Yield;
        }
        case 0x201:for(int i=0;i<5;++i)vm.pop();return Flow::Continue;
        case 0x140: {
            if(vm.peek()==0)throw mahoyo::vm::Error("Unsupported null dialogue pointer; native history flush is a separate operation");
            auto text=vm.popString();const auto rawText=text;const size_t paragraphBefore=paragraph.size();auto voice=vm.popString();int style=vm.pop(),waitFlag=vm.pop(),pageFlag=vm.pop();
            (void)voice;
            //140 has its own native scene-prepare subjob. Our already prepared
            //CPU text path consumes that barrier here;200 uses the GL ack.
            //There are no uncommitted native text-buffer records in this port.
            //Keep an explicit153 request rather than replacing its duration.
            const bool preparedRelative=!pendingRelativeMotions.empty();commitEffectCaptures();const bool preparedNoise=prepareNoise();
            const bool preparedInput=inputRegion.stable().pendingDirty;
            const auto& pendingWindow=windowFade.pending();
            if(pendingWindowMode.value_or(windowMode)==0&&((!windowFade.visible()&&(!pendingWindow||!pendingWindow->visible))||
               (windowFade.visible()&&pendingWindow&&!pendingWindow->visible&&!windowFade.active())))windowFade.requestAutoShow(300);
            commitWindow();
            auto resolved=mahoyo::text::resolveReferences(text,table);text=std::move(resolved.text);if(resolved.lastId>=0)textLine=resolved.lastId;
            const std::string dummy="＆ダミー＆";auto groupAt=text.compare(0,dummy.size(),dummy)==0?dummy.size():size_t(0);
            const bool startsGroup=text.compare(groupAt,2,"@x")==0;
            // Completed native140 jobs advance the line; non-wait/non-page runs continue inline.
            // The saved history flags already retain this boundary across process restarts.
            if(!text.empty()&&!paragraph.empty()&&paragraph.back()!='\n'&&!historyMeta.rows.empty()){
                const auto& previous=historyMeta.rows.back();
                if(previous.known&&(previous.wait||previous.page))paragraph+='\n';
            }
            text=mahoyo::text::compactDialogueSpacing(mahoyo::text::stripDisplayControls(text),paragraph);
            bool noWait=false;
            for(const auto* suffix:{"@n","@e","@k"})if(text.size()>=2&&text.compare(text.size()-2,2,suffix)==0){if(std::string(suffix)!="@k"){noWait=true;waitFlag=0;if(std::string(suffix)=="@n")pageFlag=0;}text.resize(text.size()-2);}
            const bool initialItalic=fontCarryItalic;
            const bool endingItalic=mahoyo::text::endingItalic(text,initialItalic);
            if(!text.empty()){
                if(paragraphItalic!=initialItalic)paragraph+=initialItalic?"@i":"@r";
                paragraphSources.push_back({paragraph.substr(paragraphBefore),rawText});paragraph+=text;paragraphItalic=endingItalic;
            }
            //Native07FEA0 carries only non-page/non-wait completions. This is
            //the state used after the current input is resumed, including saves.
            fontCarryItalic=waitFlag==0&&pageFlag==0?endingItalic:false;
            reveal.append(paragraph,tick);if(skipping)reveal.finish();
            mahoyo::history::Row historyRow;historyRow.startsGroup=startsGroup;historyRow.endsGroup=waitFlag!=0||pageFlag!=0;historyRow.wait=waitFlag!=0;historyRow.page=pageFlag!=0;historyRow.initialItalic=initialItalic;historyRow.style=style;appendHistory(text,std::move(historyRow),rawText);
            ++textLine;++version;
            if(preparedInput||waitClock.pending()||preparedNoise||preparedRelative||!pendingLayerPulses.empty()||!waves.pending().empty()||screenWave.pending()||!pendingClockRotations.empty()||particleOverlays.pending()){
                transition=Transition{++transitionId,tick,0,true,"[]","[]",1};transition.waitAfterReady=noWait||waitFlag==0?"frame":"input";
                transition.deadlineAfterReady=noWait||waitFlag==0?tick:tick+(skipping?0:uint64_t(autoDelay));wait="transitionReady";return Flow::Yield;
            }
            prepareWaitClock();
            if(noWait||waitFlag==0)return Flow::Continue;
            wait="input";deadline=tick+(skipping?0:uint64_t(autoDelay));return Flow::Yield;
        }
        case 0x1a0:case 0x1a8: {
            bool present=vm.peek()!=0;auto name=vm.popString();vm.pop();
            if(!present){historyMeta.pendingVoices.clear();return Flow::Continue;}
            mahoyo::history::Voice voice;voice.active=true;voice.name=name+".hw";voice.volume=100;voice.pan=64;voice.variant=op==0x1a8?1:0;mahoyo::history::validate(voice);
            // An explicit @k@e continuation must not truncate an earlier clip in its group.
            if(!skipping&&continuedVoicePlaying()){deferredVoice=std::move(voice);wait="voice";++version;return Flow::Yield;}
            startNormalVoice(voice);return Flow::Continue;
        }
        case 0x1c0:if(particleOverlays.active()||particleOverlays.pending())throw mahoyo::vm::Error("Unsupported particles/movie combination");if(screenWave.active()||screenWave.pending())throw mahoyo::vm::Error("Unsupported Screen Raster/movie combination");movieActions(opening.start(++movieGeneration));wait="movieReady";havePlatformTime=false;return Flow::Yield;
        case 0x1c1:if(movieActions(opening.wait(opening.state().id)))return Flow::Continue;wait="movie";return Flow::Yield;
        case 0x160: {
            if(vm.stack().size()<2)throw mahoyo::vm::Error("Choice operand underflow");
            auto raw=vm.stringAt(uint32_t(vm.peek(1)));if(!mahoyo::choices::validUtf8(raw))throw mahoyo::vm::Error("Unsupported non-UTF8 choice label");int value=vm.pop();vm.pop();choices.append(value,std::move(raw));return Flow::Continue;
        }
        case 0x161:return beginChoice(vm,false);
        case 0x164:return beginChoice(vm,true);
        case 0x162: {
            if(vm.stack().size()<21||vm.peek()!=2||vm.peek(1)!=0)throw mahoyo::vm::Error("Unsupported choice branch destination");
            int kind=vm.pop();int index=vm.pop();(void)kind;std::array<uint32_t,19> targets{};for(auto& target:targets)target=uint32_t(vm.pop());const int value=vm.variable(size_t(index));
            if(value>=0&&value<19)vm.jump(targets[size_t(value)]);return Flow::Continue;
        }
        case 0x180: {
            int id=vm.pop(),volume=vm.pop(),loop=vm.pop(),fade=vm.pop(),oldFade=vm.pop();
            if(uint32_t(id)<65&&size_t(256+id)<vm.sharedFlags().size())vm.setFlag(size_t(256+id),true,true);
            if(loop!=0&&loop!=1)throw mahoyo::vm::Error("Unsupported BGM loop request");
            if(id==0){fadeSound("bgm",fade<50?50:(oldFade<=0?fade:oldFade),0,true);return Flow::Continue;}
            auto entry=music.find(id);if(entry==music.end())throw mahoyo::vm::Error("Unknown music table ID "+std::to_string(id));
            auto name=lower(entry->second);if(name.find('.')==std::string::npos)name+=".hw";
            auto current=std::find_if(audio.rbegin(),audio.rend(),[](const Audio& a){return a.channel=="bgm";});
            const double target=std::clamp(volume/100.,0.,1.);
            if(current!=audio.rend()&&current->name==name&&current->loop==bool(loop)){
                fadeSound("bgm",std::max(0,fade),target,false);
            }else{
                sound(name,"bgm",bool(loop));audio.back().volume=fade>0?0:target;
                audio.back().oldFadeOutMs=fade<50?50:(oldFade<=0?fade:oldFade);
                if(fade>0)fadeSound("bgm",fade,target,false);
            }
            return Flow::Continue;
        }
        case 0x185: {vm.pop();int duration=vm.pop(),waitFlag=vm.pop();fadeSound("bgm",duration,0,true);if(waitFlag&&duration>0){wait="frame";deadline=tick+uint64_t(duration);return Flow::Yield;}return Flow::Continue;}
        case 0x184:fadeSound("bgm",400,0,true);return Flow::Continue;
        case 0x186: {vm.pop();int duration=vm.pop(),value=vm.pop();fadeSound("bgm",duration,std::clamp(value/100.,0.,1.),false);return Flow::Continue;}
        case 0x190: {
            int slot=vm.pop(),backend=vm.pop();auto pointer=uint32_t(vm.pop());auto name=vm.stringAt(pointer);int volume=vm.pop(),pan=vm.pop(),fade=vm.pop(),delay=vm.pop();
            if(slot<0||slot>=20)throw mahoyo::vm::Error("Invalid sound effect slot");
            auto channel="se"+std::to_string(slot);
            if(!pointer){fadeSound(channel,50,0,true);return Flow::Continue;}
            if(backend!=0&&backend!=1)throw mahoyo::vm::Error("Unsupported sound effect backend");
            if(!safeName(name))throw mahoyo::vm::Error("Invalid audio asset name");if(backend==1)requireSoundLoopMetadata(name);
            pendingSounds.erase(std::remove_if(pendingSounds.begin(),pendingSounds.end(),[&](const PendingSound& p){return p.channel==channel;}),pendingSounds.end());
            double target=std::clamp(volume/100.,0.,1.);double stereo=std::clamp(pan,0,128);
            pendingSounds.push_back(PendingSound{lower(name),channel,backend==1,fade>0?0:target,target,stereo,stereo,tick+uint64_t(std::max(0,delay)),uint64_t(std::max(0,fade))});startDueSounds();return Flow::Continue;
        }
        case 0x194:case 0x195: {int slot=vm.pop();if(op==0x195)vm.pop();int duration=vm.pop();vm.pop();for(int i=0;i<20;++i)if(slot<0||i==slot)fadeSound("se"+std::to_string(i),duration,0,true);return Flow::Continue;}
        case 0x196: {
            int slot=vm.pop();vm.pop();vm.pop();int volume=vm.pop(),duration=vm.pop(),delay=vm.pop();
            if(slot<0||slot>=20)return Flow::Continue;
            auto channel="se"+std::to_string(slot);
            pendingVolumeFades.erase(std::remove_if(pendingVolumeFades.begin(),pendingVolumeFades.end(),[&](const PendingVolumeFade& p){return p.channel==channel;}),pendingVolumeFades.end());
            auto current=std::find_if(audio.rbegin(),audio.rend(),[&](const Audio& a){return a.channel==channel;});
            if((current==audio.rend()||current->name.empty())&&std::none_of(pendingSounds.begin(),pendingSounds.end(),[&](const PendingSound& p){return p.channel==channel;}))return Flow::Continue;
            pendingVolumeFades.push_back(PendingVolumeFade{channel,std::clamp(volume/100.,0.,1.),tick+uint64_t(std::max(0,delay)),uint64_t(std::max(1,duration))});
            startDueVolumeFades();return Flow::Continue;
        }
        case 0x197: {
            int slot=vm.pop();auto name=vm.popString();int backend=vm.pop(),v0=vm.pop(),v1=vm.pop(),p0=vm.pop(),p1=vm.pop(),duration=vm.pop(),delay=vm.pop();
            if(slot<0||slot>=20||(backend!=0&&backend!=1)||duration<0||delay<0||duration>86400000||delay>86400000)throw mahoyo::vm::Error("Unsupported delayed sound effect variant");
            if(name.empty())return Flow::Continue;if(!safeName(name))throw mahoyo::vm::Error("Invalid audio asset name");
            if(backend==1)requireSoundLoopMetadata(name);
            if(v0<0&&v1<0)v0=v1=100;else if(v0<0)v0=v1;else if(v1<0)v1=v0;
            if(p0<0&&p1<0)p0=p1=64;else if(p0<0)p0=p1;else if(p1<0)p1=p0;
            auto channel="se"+std::to_string(slot);
            pendingSounds.erase(std::remove_if(pendingSounds.begin(),pendingSounds.end(),[&](const PendingSound& p){return p.channel==channel;}),pendingSounds.end());
            pendingSounds.push_back(PendingSound{lower(name),channel,backend==1,std::clamp(v0/100.,0.,1.),std::clamp(v1/100.,0.,1.),double(std::clamp(p0,0,128)),double(std::clamp(p1,0,128)),tick+uint64_t(delay),uint64_t(duration)});startDueSounds();return Flow::Continue;
        }
        case 0x220: {
            int id=vm.pop();auto pointer=uint32_t(vm.pop());auto name=vm.stringAt(pointer);ImageHint h;for(auto& v:h.range)v=vm.pop();
            if(id<0||id>=64||!pointer)return Flow::Continue;
            h.layerId=id;h.base=lower(name)+".mzp";if(h.base.size()>4096||!safeName(h.base))throw mahoyo::vm::Error("Invalid preload image name");
            auto found=layers.find(id);if(found!=layers.end()){const auto& l=found->second;h.brightness=l.brightness;h.contrast=l.colorValue;h.colorMode=l.colorMode;h.rgb=l.rgb;}
            preloads.pendingRanges.insert_or_assign(id,std::move(h));preloadsDirty=true;return Flow::Continue;
        }
        case 0x221:preloads.releaseOld=true;preloadsDirty=true;return Flow::Continue;
        case 0x222: {
            ImageHint h;h.tag=vm.pop();auto pointer=uint32_t(vm.pop());auto name=vm.stringAt(pointer);
            h.brightness=vm.pop();h.contrast=vm.pop();h.variantX=vm.pop();h.variantY=vm.pop();h.colorMode=vm.pop();h.rgb=vm.pop()&0xffffff;
            if(!pointer)return Flow::Continue;
            h.base=lower(name)+".mzp";h.range={h.variantX,h.variantY,h.variantX,h.variantY};
            if(h.base.size()>4096||!safeName(h.base))throw mahoyo::vm::Error("Invalid preload image name");
            if(preloads.pending.size()+(preloads.releaseOld?0:preloads.retained.size())>=MaxImageHints)throw mahoyo::vm::Error("Preload image queue exceeds limit");
            preloads.pending.push_back(std::move(h));preloadsDirty=true;return Flow::Continue;
        }
        case 0x223:return Flow::Continue; // Native009C00 has no operands or side effects.
        case 0x240:case 0x242: {
            int id=vm.pop();auto name=vm.popString();int x=vm.pop(),y=vm.pop(),scale=vm.pop(),alpha=vm.pop(),z=vm.pop(),blend=vm.pop();
            auto anchor=vm.popString();std::array<int,9> rest{};for(auto& v:rest)v=vm.pop();
            //Window geometry uses original canvas pixels, independent of image anchor/scale.
            int source=op==0x242?vm.pop():-1;
            if(pendingColors.count(id)&&name.empty())throw mahoyo::vm::Error("Unsupported nonvisual color reset without an image");
            if(id<0||id>=64||(source>=0&&!layers.count(source)))throw mahoyo::vm::Error("Invalid image layer index");
            Layer next=source>=0?cloneSource(source):Layer{};
            const auto before=next;const auto oldSize=source>=0?intrinsicSize(before):std::array<int,2>{1920,1080};
            auto center=source>=0?worldAnchor(before):std::array<double,2>{0,0};
            if(noise.contains(id))pendingNoise.insert(id);
            cancelLayer(id,false);next.id=id;next.kind="event";next.decimalMode=0;
            if(!name.empty()){next.baseImage=lower(name);if(next.baseImage.find('.')==std::string::npos)next.baseImage+=".mzp";next.color.clear();}
            if(rest[0]!=INT32_MIN)next.variantX=rest[0];if(rest[1]!=INT32_MIN)next.variantY=rest[1];
            next.image=variantName(next);auto size=imageSize(next.image);
            if(source>=0){
                const auto oldOrigin=defaultOrigin(before),newOrigin=defaultOrigin(next);
                next.drawOffsetX-=3.*(newOrigin[0]-oldOrigin[0]);next.drawOffsetY-=3.*(newOrigin[1]-oldOrigin[1]);
                double px=before.pivotX*oldSize[0],py=before.pivotY*oldSize[1];
                if(std::abs(px-3.*oldOrigin[0])<1e-6&&std::abs(py-3.*oldOrigin[1])<1e-6){px=3.*newOrigin[0];py=3.*newOrigin[1];}
                next.pivotX=px/size[0];next.pivotY=py/size[1];
                if(scale!=INT32_MIN)next.uniformScale=scale/1000.;
                //The native clone bypasses the normal anchor-string block.
            }else{
                next.uniformScale=scale==INT32_MIN?1.:scale/1000.;next.axisScaleX=next.axisScaleY=1;
                if(anchor.size()==2){auto pivot=imagePivot(next,anchor);next.pivotX=pivot[0];next.pivotY=pivot[1];next.drawOffsetX=-size[0]*next.pivotX;next.drawOffsetY=-size[1]*next.pivotY;}
                else{auto extent=anchorExtent(next);next.pivotX=next.pivotY=0;next.drawOffsetX=size[0]*extent[0]*.5;next.drawOffsetY=size[1]*extent[1]*.5;}
            }
            next.w=size[0]*next.uniformScale*next.axisScaleX;next.h=size[1]*next.uniformScale*next.axisScaleY;
            if(z!=INT32_MIN)next.z=z;if(blend!=INT32_MIN)next.blend=blend>=1&&blend<=25?blend:0;
            if(alpha!=INT32_MIN)next.alpha=std::clamp(alpha/255.,0.,1.);next.baseAlpha=next.alpha;
            placeAt(next,x==INT32_MIN?center[0]:x*4.,y==INT32_MIN?center[1]:y*4.);
            if(lower(name)=="black"||lower(name)=="white"){next.image.clear();next.color=lower(name)=="black"?"#000000":"#ffffff";next.x=0;next.y=0;next.w=1920;next.h=1080;next.drawOffsetX=-1920*next.pivotX;next.drawOffsetY=-1080*next.pivotY;next.uniformScale=1;next.axisScaleX=next.axisScaleY=1;}
            if(rest[2]!=INT32_MIN){if(rest[2]< -1||rest[2]>=64||rest[2]==id)throw mahoyo::vm::Error("Invalid native parent layer");next.parent=rest[2];}
            const std::array<int32_t,6> windowUpdate{{rest[3],rest[4],rest[5],rest[6],rest[8],rest[7]}};
            for(size_t i=0;i<6;++i)if(windowUpdate[i]!=INT32_MIN)next.window[i]=windowUpdate[i];validateWindow(next.window);
            if(!supportedRootBlendMode(next.blend)||(next.parent>=0&&(!supportedWindowChildBlend(next.blend,next.blendFlag))))throw mahoyo::vm::Error("Unsupported native image blend mode");
            layers[id]=std::move(next);
            ++version;return Flow::Continue;
        }
        case 0x256: {
            int id=vm.pop(),x=vm.pop(),y=vm.pop();auto& l=layers.at(id);
            editPendingEffectBase(id,[=](Layer& base){setVariant(base,x,y);});setVariant(l,x,y);return Flow::Continue;
        }
        default: {
            std::ostringstream message;message.imbue(std::locale::classic());message<<"Unsupported host opcode 0x"<<std::hex<<op<<" in "<<vm.moduleName()<<" +0x"<<vm.modulePc()-4<<" line "<<std::dec<<vm.sourceLine()<<" stack [";
            for(auto value:vm.stack()){message<<value<<',';try{auto s=vm.stringAt(uint32_t(value));if(!s.empty()&&s.size()<80)message<<quote(s)<<',';}catch(const std::exception&){} }
            message<<']';throw mahoyo::vm::Error(message.str());
        }
        }
    }
    void applyPendingLanguage(){
        if(pendingLanguage.empty())return;
        if(pendingLanguage==languageAssets.current()){pendingLanguage.clear();return;}
        if(opening.active()||transition.active||choices.active()||inputRegion.restricted()||
           (mode!="title"&& !((mode=="play"||mode=="history")&&wait=="input")))return;
        auto selected=languageAssets;selected.select(pendingLanguage);
        const auto raw=readBytes(root/std::filesystem::u8path(selected.tablePath()));
        std::vector<std::string> nextTable;std::istringstream lines(std::string(raw.begin(),raw.end()));std::string line;
        while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();nextTable.push_back(mahoyo::text::normalizeCtdLine(line));}
        if(nextTable.size()!=table.size())throw mahoyo::vm::Error("Language text row count mismatch");
        auto nextParagraph=mahoyo::language::renderSources(paragraphSources,nextTable);auto nextHistory=history;
        for(size_t i=0;i<historySources.size();++i)nextHistory[i]=historySources[i].prefix+mahoyo::language::displayText(historySources[i].raw,nextTable);
        auto nextLayers=layers;auto nextEffects=effects;auto nextStopped=stoppedEffectBases;
        auto nextBeforeEdits=effectBaseBeforePendingEdits;auto nextCaptures=pendingEffectCaptures;
        std::map<int,mahoyo::bitmap_geometry::Size> oldSizes,captureSizes;
        for(const auto& p:layers){auto size=intrinsicSize(p.second);oldSizes.emplace(p.first,mahoyo::bitmap_geometry::Size{size[0],size[1]});}
        for(const auto& p:pendingEffectCaptures){auto size=intrinsicSize(p.second.layer);captureSizes.emplace(p.first,mahoyo::bitmap_geometry::Size{size[0],size[1]});}
        const auto previous=languageAssets;languageAssets=std::move(selected);
        try{
            for(auto& p:nextLayers)migrateBitmapLayer(p.second,oldSizes.at(p.first));
            for(auto& p:nextCaptures)migrateBitmapLayer(p.second.layer,captureSizes.at(p.first));
            for(auto& e:nextEffects)migrateBitmapEffect(e,nextLayers);
            for(auto& p:nextStopped)migrateBitmapEffect(p.second,nextLayers);
            for(auto& p:nextBeforeEdits)migrateBitmapEffect(p.second,nextLayers);
            if(machine&&languageAssets.edition()=="korean"&&(previous.current()=="ko"||languageAssets.current()=="ko"))machine->rebindModuleStrings(mahoyo::language::scriptPrefixes());
        }
        catch(...){languageAssets=previous;throw;}
        layers=std::move(nextLayers);effects=std::move(nextEffects);stoppedEffectBases=std::move(nextStopped);
        effectBaseBeforePendingEdits=std::move(nextBeforeEdits);pendingEffectCaptures=std::move(nextCaptures);
        table=std::move(nextTable);paragraph=std::move(nextParagraph);history=std::move(nextHistory);reveal.remap(paragraph,tick);
        paragraphItalic=mahoyo::text::endingItalic(paragraph);historyChanged();resetHistorySession();preloadsDirty=true;
        archiveCatalog.reset();galleryCatalog.reset();musicCatalog.reset();pendingLanguage.clear();
    }
    void command(const std::string& type,const std::string& payload) {
        try {
            if(type=="language"){if(!mahoyo::language::supported(languageAssets.edition(),payload))throw mahoyo::vm::Error("Language not available in this edition");pendingLanguage=payload;applyPendingLanguage();++version;return;}
            if(type=="particlesReady"){particleOverlays.acknowledge(version,payload,tick,paused);return;}
            notice.clear();
            if(inputRegion.restricted()&&(type=="advance"||type=="auto"||type=="skip"||type=="hide"||type=="history"||type=="title"||type=="action"||type=="load"||type=="movieSkip"))return;
            if(type=="inputUiReady"||type=="inputJumpReleased"||type=="inputPointer"||type=="inputCancel"){
                inputCommand(type,payload);++version;return;
            }
            if(type=="choiceSelect"){selectChoice(payload);return;}
            if(choices.active()&&(type=="advance"||type=="skip"||type=="auto"))return;
            if((mode=="gallery"||mode=="music")&&(type=="action"||type=="advance"||type=="auto"||type=="skip"||type=="hide"||type=="history"||type=="title")){++version;return;}
            if(intermission.active()&&(type=="action"||type=="advance"||type=="auto"||type=="skip"||type=="hide"||type=="history"||type=="title")){++version;return;}
            if(type=="modalAction"){
                std::istringstream row(payload);uint64_t id,revision;std::string action,extra;
                if(!(row>>id>>revision>>action)||row>>extra)throw mahoyo::vm::Error("Invalid modal action");
                if(action=="archive"){
                    const auto& state=intermission.state();if(state&&state->id==id&&state->revision==revision&&state->phase==mahoyo::teatime::Phase::Ready)openArchive(0,state->selector);
                }else{auto result=intermission.action(id,revision,action,tick,safeSceneSave());if(result==mahoyo::teatime::Result::Title)title();}
            }
            else if(type=="galleryOpen"){if(mode=="title")openGallery();}
            else if(type=="musicOpen"){if(mode=="title")openMusic();}
            else if(type=="galleryAction"){
                std::istringstream row(payload);uint64_t id;std::string action,extra;int group=-1,variant=-1;
                if(!(row>>id>>action)||((action=="select"||action=="variant"||action=="next"||action=="previous")&&!(row>>group))||(action=="variant"&&!(row>>variant))||row>>extra)throw mahoyo::vm::Error("Invalid gallery action");
                galleryAction(id,action,group,variant);
            }
            else if(type=="musicAction"){
                std::istringstream row(payload);uint64_t id;std::string action,extra;int selected=-1;
                if(!(row>>id>>action)||(action=="play"&&!(row>>selected))||row>>extra)throw mahoyo::vm::Error("Invalid music action");musicAction(id,action,selected);
            }
            else if(type=="archiveOpen"){if(mode=="title")openArchive(0,0);}
            else if(type=="archivePlay"||type=="archiveBack"){
                std::istringstream row(payload);uint64_t id;int story=0;std::string extra;if(!(row>>id)||(type=="archivePlay"&&!(row>>story))||row>>extra)throw mahoyo::vm::Error("Invalid Archive action");
                if(mode=="archive"&&id==archiveGeneration){if(type=="archiveBack")returnFromArchive();else startArchive(story);}
            }
            else if(type=="historyReplay")requestHistoryReplay(payload);
            else if(type=="historyReplayEnded"||type=="historyReplayFailed"||type=="historyReplayStop")finishHistoryReplay(payload,type=="historyReplayFailed");
            else if(type=="historyClose"){if(mode=="history"&&payload==std::to_string(historyRevision)){mode="play";resetHistorySession();historyChanged();havePlatformTime=false;}}
            else if(type=="action") {if(payload=="start")play();else if(payload=="load"||payload=="continue"){try{loadGame(activeSlot);}catch(const std::exception& e){notice=e.what();}}else if(payload=="extra"){if(mode=="title")openArchive(0,0);}else if(payload=="exit")wait="exit";}
            else if(type=="pause"){inputRegion.physicalReset();flushShared();paused=true;movieActions(opening.setPaused(true));}
            else if(type=="resume"){paused=false;havePlatformTime=false;movieActions(opening.setPaused(false));}
            else if(type=="title")title();
            else if(type=="audioEnded"){
                std::istringstream row(payload);std::string channel,extra;uint64_t id;
                if(!(row>>channel>>id)||row>>extra)throw mahoyo::vm::Error("Invalid audio end event");
                auto current=std::find_if(audio.rbegin(),audio.rend(),[&](const Audio& a){return a.channel==channel;});
                if(current!=audio.rend()&&current->id==id&&!current->loop){
                    current->name.clear();current->volume=0;
                    audioFades.erase(std::remove_if(audioFades.begin(),audioFades.end(),[&](const AudioFade& a){return a.channel==channel;}),audioFades.end());
                    if(channel=="bgm")musicEnded();
                }
            }
            else if(type=="audioError"){
                std::istringstream row(payload);std::string channel,message;uint64_t id;
                if(!(row>>channel>>id))throw mahoyo::vm::Error("Invalid audio error event");
                std::getline(row,message);
                auto current=std::find_if(audio.rbegin(),audio.rend(),[&](const Audio& a){return a.channel==channel;});
                if(current!=audio.rend()&&current->id==id&&!current->name.empty()){
                    error="Audio playback failed ("+channel+"):"+message;wait="error";automatic=false;skipping=false;
                    if(mode=="music"){musicRow=musicPending=-1;musicPhase="idle";musicDue=0;}
                    intermission.reset();
                }
            }
            else if(type.rfind("movie",0)==0){
                std::istringstream row(payload);uint64_t id;if(!(row>>id))throw mahoyo::vm::Error("Invalid movie event ID");
                if(opening.active()&&id==opening.state().id){
                    uint64_t position=0,duration=0,serial=0;
                    if(type=="moviePrepared"){if(!(row>>duration)||!duration||duration>86400000)throw mahoyo::vm::Error("Invalid movie duration");movieActions(opening.prepared(id,duration));}
                    else if(type=="movieSkip")movieActions(opening.skip(id));
                    else if(type=="movieError"){std::string message;std::getline(row,message);auto failed=opening.fail(id);if(failed.failed){error="Opening playback failed:"+message;wait="error";}}
                    else{if(!(row>>position>>duration>>serial)||position>86400000||duration>86400000)throw mahoyo::vm::Error("Invalid movie position event");
                        if(type=="moviePosition")movieActions(opening.position(id,position,serial));
                        else if(type=="movieSeeked")movieActions(opening.seeked(id,serial,position));
                        else if(type=="movieEnded"){movieActions(opening.position(id,position,serial));movieActions(opening.eof(id,serial,position));}
                        else throw mahoyo::vm::Error("Unknown movie event");
                    }
                }
            }
            else if(type=="transitionReady"){if(transition.active&&wait=="transitionReady"&&payload==(alphaRules.pending()?alphaRules.ack(transition.id,version):std::to_string(transition.id))&&(!alphaRules.pending()||!paused)){if(alphaRules.pending()){prepareAlphaRules();alphaRules.prepared(tick);}captureWaitClock();const auto capturedAge=waitClock.pending()?0:transition.waitClockAge;waitClock.prepared(transition.waitClockAge,tick);if(transition.wait110)transition.deadlineAfterReady=tick+mahoyo::wait_clock::Clock::remaining(transition.wait110Duration,transition.wait110Mode,capturedAge);commitGlobalShake();commitLayerPulses();commitWindow();commitWaves();commitClockRotations();pulseWaitLayer=transition.pulseWait?std::optional<int>(transition.pulseWaitLayer):std::nullopt;if(transition.clockWait){transition.deadlineAfterReady=transition.nonrotationDeadline;for(const auto& a:transforms)if(a.mask==4&&(transition.clockWaitLayer<0||a.id==transition.clockWaitLayer))transition.deadlineAfterReady=std::max(transition.deadlineAfterReady,a.end);}waitingScreenWave=transition.waveWait&&transition.waveWaitLayer<0;if(transition.waveWait)transition.deadlineAfterReady=std::max(transition.deadlineAfterReady,tick+waves.remainingWait(transition.waveWaitLayer));transition.begin=tick;if(!transition.waitAfterReady.empty()){deadline=transition.deadlineAfterReady;wait=transition.waitAfterReady;transition.active=false;sampleLayerPulses();if(wait=="choice"){choices.prepared();}else if(deadline<=tick&&(!waitingScreenWave||screenWave.remainingWait()==0)&&(!pulseWaitLayer||!finiteLayerShakeBusy(*pulseWaitLayer))&&(!effectWaitLayer||!finiteEffectWaitBusy(*effectWaitLayer)))resume();}else{deadline=tick+uint64_t(transition.mode==1?16:transition.duration);wait=transition.mode==1?"frame":"transition";}havePlatformTime=false;}}
            else if(type=="textSpeed"){reveal.speed(std::stoi(payload),tick);if(!reveal.active()&&wait=="input")deadline=tick+uint64_t(autoDelay);}
            else if(type=="autoDelay"){int value=std::stoi(payload);if(value<500||value>10000)throw mahoyo::vm::Error("Auto delay must be 500..10000 ms");autoDelay=value;if(wait=="input")deadline=tick+uint64_t(autoDelay);}
            else if(type=="advance") {if(opening.active())movieActions(opening.skip(opening.state().id));else if(mode=="play"&&(wait=="input"||wait=="voice")){if(reveal.finish())deadline=tick+uint64_t(autoDelay);else resume(wait=="voice");}else if(mode=="history"){mode="play";resetHistorySession();historyChanged();}}
            else if(type=="tick") {if(!paused&&wait!="transitionReady"&&wait!="movieReady"){
                auto now=std::stoull(payload);auto delta=havePlatformTime&&now>=platformLast?now-platformLast:uint64_t(tickBootstrapMs);if(delta>UINT64_MAX-tick)throw mahoyo::vm::Error("Logical clock overflow");waitClock.age(tick+delta);tick+=delta;inputRegion.sample(tick,!paused&&mode=="play");if((mode=="play"||mode=="teatime")&&!opening.active())noiseClockMs+=delta;platformLast=now;havePlatformTime=true;
                alphaRules.sample(tick);for(auto& p:layers)if(alphaRules.contains(p.first)&&alphaRules.json(p.first)=="null")p.second.alpha=1.;
                // Native14C840 caps each fader0/2 update; unrelated clocks retain their own rules.
                if(transition.active&&wait=="transition"&&(transition.nativeMode==0||transition.nativeMode==2))transition.nativePhaseMs+=std::min<uint64_t>(delta,uint64_t(transition.nativeStepMs));
                windowFade.sample(tick);
                waves.sample(tick,mode!="history");screenWave.sample(tick,mode!="history"&&mode!="archive");
                bool revealing=reveal.active();reveal.tick(tick);if(revealing&&!reveal.active()&&wait=="input")deadline=tick+uint64_t(autoDelay);
                for(const auto& a:fades){double p=ease(double(tick-a.begin)/double(a.end-a.begin),a.curve);layers[a.id].alpha=a.from+(a.to-a.from)*p;}
                for(const auto& a:transforms){double p=a.curve==70?mahoyo::effects::progress70(float(tick-a.begin),float(a.end-a.begin)):ease(double(tick-a.begin)/double(a.end-a.begin),a.curve);auto& l=layers[a.id];double x=a.fromX+(a.toX-a.fromX)*p,y=a.fromY+(a.toY-a.fromY)*p;if(a.curve==70){x=mahoyo::clock::add(mahoyo::clock::mul(mahoyo::clock::f(float(a.toX)-float(a.fromX)),float(p)),float(a.fromX));y=mahoyo::clock::add(mahoyo::clock::mul(mahoyo::clock::f(float(a.toY)-float(a.fromY)),float(p)),float(a.fromY));}if(a.mask==2){setAxisScale(l,x,y);l.matrixMode=a.matrixMode;}else l.rotation=x;}
                for(const auto& m:motions){double p=ease(double(tick-m.begin)/double(m.end-m.begin),m.curve);auto& l=layers[m.id];placeAt(l,m.x0+(m.x1-m.x0)*p,m.y0+(m.y1-m.y0)*p);}
                applyEffects();
                startDueSounds();
                for(const auto& a:audioFades){auto found=std::find_if(audio.rbegin(),audio.rend(),[&](const Audio& v){return v.channel==a.channel;});if(found!=audio.rend()){double p=std::min(1.,double(tick-a.begin)/double(a.end-a.begin));if(a.volumeActive){double volume=a.from+(a.to-a.from)*p;found->volume=a.quantizeVolume?std::trunc(volume*100.+1e-9)/100.:volume;}if(a.panActive)found->pan=a.fromPan+(a.toPan-a.fromPan)*p;}if(tick>=a.end&&a.stop)audio.push_back(Audio{++audioId,"",a.channel,false,0});}
                startDueVolumeFades();
                globalShakeOffset=globalShake.sample(tick);
                sampleLayerPulses();
                for(auto& s:shakes){auto phase=(tick-s.begin)/uint64_t(s.period);if(phase!=s.phase){s.phase=phase;auto random=[&](){randomState=randomState*214013u+2531011u;return (randomState>>16)&32767u;};for(int i=0;i<2;++i){s.dx[i]=(s.x*62.5-2*(random()%uint32_t(s.x*62.5+1)))/100.*3.;s.dy[i]=(s.y*77.77778-2*(random()%uint32_t(s.y*77.77778+1)))/100.*3.;}}}
                particleOverlays.step(tick,error.empty()&&(mode=="play"||mode=="teatime")&&!opening.active(),paused,[&](){randomState=randomState*214013u+2531011u;return (randomState>>16)&32767u;});
                for(const auto& a:fades)if(tick>=a.end)editEffectBase(a.id,[&](Layer& base){base.alpha=a.to;});
                for(const auto& a:transforms)if(tick>=a.end)editEffectBase(a.id,[&](Layer& base){if(a.mask==2){setAxisScale(base,a.toX,a.toY);base.matrixMode=a.matrixMode;}else base.rotation=a.toX;});
                for(const auto& m:motions)if(tick>=m.end)editEffectBase(m.id,[&](Layer& base){placeAt(base,m.x1,m.y1);});
                fades.erase(std::remove_if(fades.begin(),fades.end(),[&](const Animation&a){return tick>=a.end;}),fades.end());
                motions.erase(std::remove_if(motions.begin(),motions.end(),[&](const Motion&m){return tick>=m.end;}),motions.end());
                transforms.erase(std::remove_if(transforms.begin(),transforms.end(),[&](const Transform&a){return tick>=a.end;}),transforms.end());
                audioFades.erase(std::remove_if(audioFades.begin(),audioFades.end(),[&](const AudioFade&a){return tick>=a.end;}),audioFades.end());
                advanceMusic();
                effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const Effect&e){return effectFinished(e);}),effects.end());
                shakes.erase(std::remove_if(shakes.begin(),shakes.end(),[&](const Shake&s){return s.duration>=0&&tick-s.begin>=uint64_t(s.duration);}),shakes.end());
                if(intermission.tick(tick)==mahoyo::teatime::Result::ResumeCurrentVm){mode="play";havePlatformTime=false;resume();}
                if(!inputRegion.jumpQueued()&&wait=="transition"&&((transition.nativeMode==0||transition.nativeMode==2)?transition.nativePhaseMs>=uint64_t(transition.duration):tick>=deadline)){presentedLayers=layerJson();presentedScreenWave=screenWaveJson();presentedParticles=particleOverlays.json();transition.active=false;resume();}
                else if(!inputRegion.jumpQueued()&&(wait=="frame"||wait=="animation")&&tick>=deadline&&(!waitingScreenWave||screenWave.remainingWait()==0)&&(!pulseWaitLayer||!finiteLayerShakeBusy(*pulseWaitLayer))&&(!effectWaitLayer||!finiteEffectWaitBusy(*effectWaitLayer))){pulseWaitLayer.reset();resume();}else if(!inputRegion.jumpQueued()&&wait=="voice"&&(skipping||!voicePlaying()))resume();else if(!inputRegion.jumpQueued()&&(wait=="budget"||((skipping||(automatic&&!reveal.active()&&!voicePlaying()))&&wait=="input"&&tick>=deadline)))resume();
            }}
            else if(type=="auto"){automatic=!automatic;if(automatic&&wait=="input")deadline=tick+uint64_t(autoDelay);}
            else if(type=="skip"){skipping=payload=="1"?true:payload=="0"?false:!skipping;if(skipping){reveal.finish();deadline=tick;}}
            else if(type=="hide")hidden=!hidden;
            else if(type=="history"){if(mode=="play"||mode=="history"){mode=mode=="history"?"play":"history";resetHistorySession();historyChanged();havePlatformTime=false;}}
            else if(type=="save"||type=="load"){try{int slot=payload.empty()?activeSlot:std::stoi(payload);if(type=="save")saveGame(slot);else loadGame(slot);}catch(const std::exception& e){notice=e.what();}}
            if(inputRegion.jumpQueued()&&!paused&&error.empty()){effectWaitLayer.reset();finishInputJump();}
            if(!inputRegion.jumpQueued()&&!paused&&error.empty()&&((wait=="movieReady"&&opening.state().prepared)||(wait=="movie"&&opening.state().status==mahoyo::movie::Status::Finished))){havePlatformTime=false;resume();}
            if(error.empty())sampleLayerPulses(); //A resumed script may change its quad at the same tick.
            for(const auto& p:layers)if(alphaRules.contains(p.first))mahoyo::alpha_rule::validate(alphaContext(p.second));
            waves.sample(tick,!paused&&mode!="history"&&wait!="transitionReady"&&wait!="movieReady");screenWave.sample(tick,!paused&&mode!="history"&&mode!="archive"&&wait!="transitionReady"&&wait!="movieReady");
            if(screenWave.active())validateScreenWave(layers);
            for(const auto& pair:noise.states())if(layers.count(pair.first)){validateNoiseCallback(machine.get(),layers.at(pair.first));validateNoiseLayer(layers.at(pair.first),waves);}
            applyPendingLanguage();
            ++version;
        }catch(const std::exception& e){error=e.what();++version;}
    }
    std::string layerJson() {
        std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9)<<'[';
        bool first=true;std::vector<Layer> ordered;for(const auto& entry:layers)ordered.push_back(entry.second);std::stable_sort(ordered.begin(),ordered.end(),[](const Layer&a,const Layer&b){return a.z<b.z;});
        for(auto l:ordered){applyOutputParents(l);applyOutputShakes(l);if(l.image.empty()&&l.color.empty())continue;if(!first)s<<',';first=false;bool preload=(transition.active&&transition.mode==1)||std::any_of(effects.begin(),effects.end(),[&](const Effect&e){return e.id==l.id;})||std::any_of(fades.begin(),fades.end(),[&](const Animation&a){return a.id==l.id&&a.to>0;});s<<"{\"id\":"<<l.id<<",\"z\":"<<l.z<<",\"image\":"<<quote(resourceName(l.image))<<",\"kind\":"<<quote(l.kind)<<",\"color\":"<<quote(l.color)<<",\"x\":"<<l.x<<",\"y\":"<<l.y<<",\"w\":"<<l.w<<",\"h\":"<<l.h<<",\"rotation\":"<<l.rotation<<",\"pivotX\":"<<l.pivotX<<",\"pivotY\":"<<l.pivotY<<",\"alpha\":"<<(l.enabled?alphaRules.opacity(l.id,l.alpha):0)<<",\"colorMode\":"<<l.colorMode<<",\"colorValue\":"<<l.colorValue<<",\"rgb\":"<<l.rgb<<",\"brightness\":"<<l.brightness<<",\"blend\":"<<l.blend<<",\"blendFlag\":"<<l.blendFlag<<",\"decimalMode\":"<<l.decimalMode<<",\"pixelOffsetX\":"<<double(l.targetOffsetX)/(l.targetOffsetDivisor?l.targetOffsetDivisor:1)<<",\"pixelOffsetY\":"<<double(l.targetOffsetY)/(l.targetOffsetDivisor?l.targetOffsetDivisor:1)<<",\"preload\":"<<(preload?"true":"false");s<<",\"parent\":"<<l.parent;if(alphaRules.contains(l.id))s<<",\"logicalAlpha\":"<<l.alpha<<",\"alphaRule\":"<<alphaRules.json(l.id);if(nativeFullBitmapGeometryReviewed(l))s<<",\"nativeFullBitmapGeometryReviewed\":true";if(l.matrixMode==1){auto size=intrinsicSize(l);s<<",\"matrixMode\":1,\"intrinsicWidth\":"<<size[0]<<",\"intrinsicHeight\":"<<size[1];}if(l.window[2]&&l.window[3])s<<",\"window\":{\"x\":"<<l.window[0]<<",\"y\":"<<l.window[1]<<",\"width\":"<<l.window[2]<<",\"height\":"<<l.window[3]<<",\"colorMode\":"<<l.window[4]<<",\"softEdge\":"<<l.window[5]<<'}';if(l.parent>=0)s<<",\"groupOpacityByte\":"<<((int32_t(float(std::clamp(l.alpha,0.,1.)*65536.))*255)>>16);if(waves.contains(l.id)){auto wave=waves.draw(l.id,tick,wavePlatformEnabled?(localWaveClockRunning()&&l.enabled):(error.empty()&&!paused&&mode!="history"&&!opening.active()&&l.enabled&&l.alpha>.0001),wavePlatformEnabled);s<<",\"wave\":{\"preset\":"<<wave.preset<<",\"strength\":"<<wave.strength<<",\"elapsedMs\":"<<wave.phaseMs<<'}';if(wave.preset==40)s<<",\"enabled\":"<<(l.enabled?"true":"false");}if(noise.contains(l.id)){const auto& n=noise.at(l.id);s<<",\"noise\":{\"token\":"<<quote(n.token)<<",\"selector\":"<<n.config.selector<<",\"periodMs\":"<<n.config.periodMs<<",\"clockMs\":"<<noiseClockMs;if(int profile=noiseProfile(machine.get(),l))s<<",\"profile\":"<<profile;s<<'}';}if(int finalMode=finalCallback28(machine.get(),l))s<<",\"finalCallback28\":"<<finalMode;if(l.hasCrop)s<<",\"source\":["<<l.crop[0]<<','<<l.crop[1]<<','<<l.crop[2]<<','<<l.crop[3]<<']';s<<'}';}
        s<<']';return s.str();
    }
    size_t activeTextStart() const {
        const size_t total=mahoyo::text::visibleLength(paragraph);size_t active=0;
        for(size_t i=history.size();i>0;--i){active+=mahoyo::text::visibleLength(history[i-1]);if(i==1||!historyMeta.rows[i-1].known||historyMeta.rows[i-2].wait||historyMeta.rows[i-2].page)break;}
        return total>active?total-active:0;
    }
    std::string snapshot(bool historyPreview=false) {
        bool scheduled=(!paused&&error.empty()&&mode!="history"&&wait!="transitionReady"&&wait!="movieReady"&&((inputRegion.needsTick()|| !opening.active()&&(alphaRules.running()||particleOverlays.needsTimedUpdates()||waveScheduled()||globalShakeBusy()||layerShakeBusy()||intermission.needsTick()||musicNeedsTick()||windowFade.active()||reveal.active()||!fades.empty()||std::any_of(effects.begin(),effects.end(),[](const Effect& e){return !e.paused;})||!motions.empty()||!transforms.empty()||!shakes.empty()))||!audioFades.empty()||!pendingSounds.empty()||!pendingVolumeFades.empty()||wait=="frame"||wait=="animation"||wait=="transition"||wait=="budget"||(wait=="voice"&&(skipping||!voicePlaying()))||((skipping||(automatic&&(tick<deadline||!voicePlaying())))&&wait=="input")));int noiseWake=(!paused&&error.empty()&&(mode=="play"||mode=="teatime")&&!opening.active()&&wait!="transitionReady"&&wait!="movieReady")?noise.minimumPeriod():0;int nextTick=scheduled?16:noiseWake;tickBootstrapMs=nextTick?nextTick:16;if(!nextTick)havePlatformTime=false;
        const auto currentScreenWave=screenWaveJson();const auto currentParticles=particleOverlays.json();
        std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9);
        s<<"{\"alphaRulePrepareAck\":"<<(alphaRules.pending()&&transition.active&&wait=="transitionReady"?quote(alphaRules.ack(transition.id,version)):"null")<<",\"windowMode\":"<<windowMode<<",\"windowAlphaFixed16\":"<<windowFade.currentFixed16()<<",\"windowVisible\":"<<(windowFade.visible()?"true":"false")<<",\"version\":"<<version<<",\"designWidth\":1920,\"designHeight\":1080,\"scene\":"<<quote(mode)<<",\"waiting\":"<<quote(wait)<<",\"error\":"<<quote(error)<<",\"notice\":"<<quote(notice)<<",\"automatic\":"<<(automatic?"true":"false")<<",\"skipping\":"<<(skipping?"true":"false")<<",\"revealing\":"<<(reveal.active()?"true":"false")<<",\"canSave\":"<<(canSaveNow()?"true":"false")<<",\"nextTickMs\":"<<nextTick<<",\"layers\":"<<layerJson();
        s<<",\"inputRegion\":"<<inputRegionJson();
        s<<",\"text\":[";if(!hidden&&!paragraph.empty()&&mode=="play"&&!opening.active())s<<"{\"text\":"<<quote(paragraph)<<",\"visibleCharacters\":"<<reveal.visible()<<",\"activeFrom\":"<<activeTextStart()<<",\"cursor\":"<<(wait=="input"&&!reveal.active()?"true":"false")<<",\"alpha\":"<<windowFade.currentFixed16()/65536.<<",\"x\":300,\"y\":120,\"w\":1320,\"h\":840,\"fontSize\":52,\"color\":\"#ffffff\"}";
        s<<"],\"actions\":[";
        if(mode=="title"){const char* ids[]={"start","load","continue","settings","extra","exit"};for(int i=0;i<6;++i){if(i)s<<',';s<<"{\"id\":"<<quote(ids[i])<<",\"label\":\"\",\"x\":1280,\"y\":"<<455+i*89<<",\"w\":495,\"h\":89}";}}
        s<<"],\"audio\":[";std::map<std::string,Audio> currentAudio;for(const auto& a:audio)currentAudio.insert_or_assign(a.channel,a);bool first=true;for(const auto& pair:currentAudio){const auto& a=pair.second;if(!first)s<<',';first=false;s<<"{\"id\":"<<a.id<<",\"name\":"<<quote(a.name)<<",\"path\":"<<quote(a.name)<<",\"channel\":"<<quote(a.channel)<<",\"loop\":"<<(a.loop?"true":"false")<<",\"volume\":"<<a.volume<<",\"pan\":"<<a.pan;
            if(a.channel=="bgm"&&a.oldFadeOutMs)s<<",\"oldFadeOutMs\":"<<a.oldFadeOutMs;
            if(a.loop){
                auto found=audioLoops.find(a.name);
                if(found!=audioLoops.end()){
                    const auto& region=found->second;auto end=region.end&&region.end<region.sampleCount?region.end:region.sampleCount;
                    // Whole-file ambience also needs a continuous PCM loop, not a codec seek at EOF.
                    bool wholeAmbient=a.channel.compare(0,2,"se")==0&&region.sampleRate<=192000&&region.channels<=2&&uint64_t(end)*region.channels*2<=48*1024*1024;
                    if(region.start||end<region.sampleCount||wholeAmbient){
                        if(region.start>=end)throw mahoyo::vm::Error("Invalid audio loop range: "+a.name);
                        s<<",\"loopRegion\":{\"sampleCount\":"<<region.sampleCount<<",\"sampleRate\":"<<region.sampleRate<<",\"channels\":"<<region.channels<<",\"startFrame\":"<<region.start<<",\"endFrame\":"<<end<<'}';
                    }
                }
            }
            s<<'}';}
        s<<"],\"history\":"<<historyJson(historyPreview)<<",\"historyRevision\":"<<quote(std::to_string(historyRevision))<<",\"historyReplay\":"<<historyReplayJson();
        s<<",\"particles\":"<<currentParticles<<",\"movie\":"<<movieJson()<<",\"transition\":";
        if(transition.active){auto elapsed=transition.mode==1||wait=="transitionReady"?uint64_t(0):std::min(uint64_t(transition.duration),(transition.nativeMode==0||transition.nativeMode==2)?transition.nativePhaseMs:tick-transition.begin);double opacity=transition.mode==1?0:double(65536-(elapsed*65536)/uint64_t(transition.duration))/65536.;s<<"{\"id\":"<<transition.id<<",\"mode\":"<<transition.mode<<",\"nativeMode\":"<<transition.nativeMode<<",\"nativeStepMs\":"<<transition.nativeStepMs<<",\"durationMs\":"<<transition.duration<<",\"elapsedMs\":"<<elapsed<<",\"oldOpacity\":"<<(transition.maskId?1:opacity)<<",\"maskId\":"<<transition.maskId<<",\"mask\":"<<quote(transition.mask)<<",\"strength\":"<<transition.strength<<",\"progress256\":"<<mahoyo::effects::maskProgress(elapsed,transition.duration,transition.strength)<<",\"old\":{\"scene\":"<<quote(transition.sourceScene)<<",\"module\":"<<quote(transition.sourceModule)<<",\"language\":"<<quote(languageAssets.current())<<",\"designWidth\":1920,\"designHeight\":1080,\"layers\":"<<transition.layers<<",\"text\":"<<transition.text<<",\"movie\":"<<transition.movie<<",\"particles\":"<<transition.particles<<",\"screenWave\":"<<transition.screenWave<<"}}";}else s<<"null";
        s<<",\"choice\":"<<choiceJson()<<",\"screenWave\":"<<currentScreenWave<<",\"gallery\":"<<galleryJson()<<",\"musicRoom\":"<<musicJson()<<",\"archive\":"<<archiveJson()<<",\"modal\":"<<intermissionJson()<<",\"sharedGeneration\":"<<sharedState.generation<<",\"preloads\":"<<preloadJson()<<",\"seenImageCount\":"<<seenImages.size()<<",\"trophyCount\":"<<trophies.size()<<",\"platinumUnlocked\":"<<(trophies.size()==26?"true":"false")<<",\"edition\":"<<quote(languageAssets.edition())<<",\"language\":"<<quote(languageAssets.current())<<",\"textLine\":"<<textLine<<",\"module\":"<<quote(machine?machine->moduleName():"")<<",\"instructions\":"<<(machine?machine->instructionsExecuted():0)<<",\"sourceLine\":"<<(machine?machine->sourceLine():0)<<",\"modulePc\":"<<(machine?machine->modulePc():0)<<'}';result=s.str();auto start=result.find("\"layers\":")+9,end=result.find(",\"text\":",start);presentedLayers=result.substr(start,end-start);start=end+8;end=result.find(",\"actions\":",start);presentedText=result.substr(start,end-start);presentedMovie=movieJson();presentedScene=mode;presentedModule=machine?machine->moduleName():"";presentedScreenWave=currentScreenWave;presentedParticles=currentParticles;return result;
    }
};
const char* errorSnapshot(Runtime& runtime,const char* reason){
    //Do not recurse into snapshot/layerJson or discard VM/save state on a render guard failure.
    runtime.error=reason;runtime.wait="error";runtime.automatic=false;runtime.skipping=false;runtime.havePlatformTime=false;std::ostringstream out;out.imbue(std::locale::classic());
    out<<"{\"alphaRulePrepareAck\":null,\"windowMode\":"<<runtime.windowMode
       <<",\"windowAlphaFixed16\":0,\"windowVisible\":false,\"version\":"<<runtime.version
       <<",\"designWidth\":1920,\"designHeight\":1080,\"scene\":"<<quote(runtime.mode)
       <<",\"waiting\":\"error\",\"error\":"<<quote(runtime.error)
       <<",\"notice\":"<<quote(runtime.notice)
       <<",\"automatic\":false,\"skipping\":false,\"revealing\":false,\"canSave\":false,\"nextTickMs\":0"
       <<",\"layers\":[],\"inputRegion\":null,\"text\":[],\"actions\":[],\"audio\":[],\"history\":[]"
       <<",\"historyRevision\":\"0\",\"historyReplay\":null,\"movie\":null,\"transition\":null,\"choice\":null"
       <<",\"screenWave\":null,\"gallery\":null,\"musicRoom\":null,\"archive\":null,\"modal\":null"
       <<",\"sharedGeneration\":"<<runtime.sharedState.generation
       <<",\"preloads\":[],\"seenImageCount\":"<<runtime.seenImages.size()
       <<",\"trophyCount\":"<<runtime.trophies.size()<<",\"platinumUnlocked\":false"
       <<",\"textLine\":"<<runtime.textLine<<",\"module\":"<<quote(runtime.machine?runtime.machine->moduleName():"")
       <<",\"instructions\":"<<(runtime.machine?runtime.machine->instructionsExecuted():0)
       <<",\"sourceLine\":"<<(runtime.machine?runtime.machine->sourceLine():0)
       <<",\"modulePc\":"<<(runtime.machine?runtime.machine->modulePc():0)<<'}';
    runtime.result=out.str();return runtime.result.c_str();
}
const char* guardedSnapshot(Runtime& runtime,bool historyPreview=false){
    try{runtime.snapshot(historyPreview);return runtime.result.c_str();}
    catch(const std::exception& error){return errorSnapshot(runtime,error.what());}
    catch(...){return errorSnapshot(runtime,"Unknown runtime snapshot failure");}
}
std::mutex mutex;
std::map<uint64_t,std::unique_ptr<Runtime>> runtimes;
uint64_t nextHandle=1;
const char* invalid="{\"error\":\"Invalid runtime handle\"}";
}
extern "C" uint64_t woh_create(const char* root) {std::lock_guard<std::mutex> lock(mutex);auto id=nextHandle++;runtimes[id]=std::make_unique<Runtime>(root);return id;}
extern "C" uint64_t woh_create_configured(const char* root,const char* edition,const char* language) {std::lock_guard<std::mutex> lock(mutex);auto id=nextHandle++;runtimes[id]=std::make_unique<Runtime>(root,edition,language);return id;}
extern "C" const char* woh_command(uint64_t id,const char* type,const char* payload) {std::lock_guard<std::mutex> lock(mutex);auto i=runtimes.find(id);if(i==runtimes.end())return invalid;if(std::string(type?type:"")=="historyPreview")return guardedSnapshot(*i->second,true);i->second->command(type?type:"",payload?payload:"");return guardedSnapshot(*i->second);}
extern "C" void woh_wave_clock(uint64_t id,uint64_t now) {std::lock_guard<std::mutex> lock(mutex);auto i=runtimes.find(id);if(i!=runtimes.end())i->second->sampleWaveClock(now);}
extern "C" const char* woh_snapshot(uint64_t id) {std::lock_guard<std::mutex> lock(mutex);auto i=runtimes.find(id);if(i==runtimes.end())return invalid;return guardedSnapshot(*i->second);}
extern "C" void woh_destroy(uint64_t id) {std::lock_guard<std::mutex> lock(mutex);runtimes.erase(id);}
