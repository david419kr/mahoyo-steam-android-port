#include "effects.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace mahoyo::effects {
namespace {
constexpr uint64_t masks[]={0x40880,0x5100000,0x4000,0x80200000,0x100400000,0x88000,
    0x10000080,0x04000400,0x10000080,0x04000400,0x10,0x20,
    0x10002,0x20004,0x02000000,0x200,0x200,0x200,0x200,0x20000000,0x40000000,0x40,0x100,0x1000,1};
bool selected(const Clip& clip,Field field){return !(clip.windowMetadata||clip.propertiesMetadata)||(clip.descriptor&masks[size_t(field)])!=0;}
bool forcedStep(Field field){return field==WindowColor||field==BlendMode||field==ColorMode||(field>=ColorR&&field<=ColorB)||field==Brightness||field==Contrast||field==AuxResourceAndScale;}
bool sentinel(int value){return value==-32766||value==-32767;}
//Native084D90 default85F19 aliases within the stored nonnegative signed16 domain.
bool nativeLinearFrameCurve(int curve){return curve>=0&&curve<=INT16_MAX&&(curve<=1||(curve>=42&&curve<=69)||curve>=71);}
void checkMode(int mode){if(mode!=0&&mode!=1&&mode!=2&&!sentinel(mode))throw std::runtime_error("Unsupported native keyframe mode "+std::to_string(mode));}
int32_t integer(double value){
    value=float(value);
    if(!std::isfinite(value)||value<INT32_MIN||value>INT32_MAX)throw std::runtime_error("Native effect property exceeds integer range");
    return int32_t(value);
}
double grid(double value,Field field){
    if(field!=X&&field!=Y)return value;
    float v=float(value),numerator=field==X?640.f:480.f,denominator=field==X?480.f:360.f;
    float scaled=(v*numerator)/denominator;
    if(!std::isfinite(scaled)||double(scaled)<INT32_MIN||double(scaled)>INT32_MAX)throw std::runtime_error("Native position key exceeds integer range");
    return (float(int32_t(scaled))*denominator)/numerator;
}
//Volatile boundaries retain original SSE rounding and prevent arm64 FMA contraction.
float nativeFloat(double value){volatile float result=float(value);return result;}
float nativeAdd(float a,float b){volatile float result=a+b;return result;}
float nativeSub(float a,float b){volatile float result=a-b;return result;}
float nativeMul(float a,float b){volatile float result=a*b;return result;}
float nativeDiv(float a,float b){volatile float result=a/b;return result;}
double nativeDouble(double value){volatile double result=value;return result;}
float nativeBounce(float elapsed,float start,float delta,float duration){
    float u=nativeDiv(elapsed,duration);
    if(double(u)<.36363636363636365){
        double scaled=nativeDouble(nativeDouble(double(nativeMul(u,u))*7.5625)*double(delta));
        return nativeFloat(nativeDouble(scaled+double(start)));
    }
    float q;double level;
    if(double(u)<.7272727272727273){q=nativeFloat(double(u)-.5454545454545454);level=.75;}
    else if(double(u)<.9090909090909091){q=nativeFloat(double(u)-.8181818181818182);level=.9375;}
    else{q=nativeFloat(double(u)-.9545454545454546);level=.984375;}
    double squared=nativeDouble(nativeDouble(double(q)*7.5625)*double(q));
    double scaled=nativeDouble(nativeDouble(squared+level)*double(delta));
    return nativeFloat(nativeDouble(scaled+double(start)));
}
template<class ValueAt>
double modeOne(size_t count,size_t current,ValueAt valueAt,int hold,int skip,
               double initial,double startValue,double endValue,double progress){
    if(count>128)throw std::runtime_error("Native mode1 exceeds reviewed128-frame bound");
    std::array<float,128> points{},coefficient{},diagonal{},delta{};
    std::array<size_t,128> rawIndex{};
    auto resolve=[&](size_t from){
        for(size_t i=from+1;i-->0;){int value=valueAt(i);if(value!=hold&&value!=skip)return nativeFloat(value);}
        return nativeFloat(initial);
    };
    size_t size=1;points[0]=resolve(0);
    for(size_t i=1;i+1<count;++i){
        int value=valueAt(i);
        if(value==skip||(valueAt(i-1)==value&&value==valueAt(i+1)))rawIndex[i]=size-1;
        else {rawIndex[i]=size;points[size++]=resolve(i);}
    }
    if(count>1&&valueAt(count-2)!=valueAt(count-1)&&valueAt(count-1)!=hold){
        rawIndex[count-1]=size;points[size++]=resolve(count-1);
    }
    float start=nativeFloat(startValue),end=nativeFloat(endValue),t=nativeFloat(progress);
    float difference=nativeSub(end,start);
    if(size<3)return nativeAdd(start,nativeMul(difference,t));
    for(size_t i=0;i+1<size;++i)delta[i]=nativeSub(points[i+1],points[i]);
    diagonal[1]=4;coefficient[1]=nativeSub(delta[1],delta[0]);
    for(size_t i=1;i+2<size;++i){
        float reciprocal=nativeDiv(1,diagonal[i]);
        //A37F0 subtracts diagonal*reciprocal, not the usual previous RHS term.
        coefficient[i+1]=nativeSub(nativeSub(delta[i+1],delta[i]),nativeMul(diagonal[i],reciprocal));
        diagonal[i+1]=nativeSub(4,reciprocal);
    }
    coefficient[size-2]=nativeSub(coefficient[size-2],coefficient[size-1]);
    for(size_t i=size-1;i-->1;)coefficient[i]=nativeDiv(nativeSub(coefficient[i],coefficient[i+1]),diagonal[i]);
    size_t index=rawIndex[current-1];float a=coefficient[index],b=coefficient[index+1];
    float value=nativeAdd(nativeMul(nativeSub(b,a),t),nativeMul(a,3));
    value=nativeMul(value,t);
    value=nativeAdd(value,nativeSub(difference,nativeAdd(nativeMul(a,2),b)));
    return nativeAdd(nativeMul(value,t),start);
}
}
uint32_t maskProgress(uint64_t elapsed,uint64_t duration,int strength) {
    if(strength<0||strength>256)throw std::runtime_error("Invalid native mask strength");
    if(!duration||elapsed>=duration)return 256;
    uint32_t value=uint32_t(elapsed*256/duration);
    if(strength<100)value=256-uint32_t(std::pow(float(256-value)/256.f,strength<50?.25f:.5f)*256.f);
    return value;
}
double ease(double p,int curve) {
    p=std::clamp(p,0.,1.);
    // WoH VA0x140084d90, native easing: quartic cases10/13 use
    // pow(t,3)*t and quintic14/17 use pow(t,4)*t; preserve those degrees.
    switch(curve){
    case 0:case 1:return p;
    case 2:return p*p;
    case 3:return -p*(p-2);
    case 4:return p<.5?2*p*p:1-2*(p-1)*(p-1);
    case 5:{double t=2*p-1;return p<.5?(1-t*t)/2:(1+t*t)/2;}
    case 6:return p*p*p;
    case 7:return (p-1)*(p-1)*(p-1)+1;
    case 8:return p<.5?4*p*p*p:((2*p-2)*(2*p-2)*(2*p-2)+2)/2;
    case 9:{double t=2*p-1;return (t*t*t+1)/2;}
    case 10:return p*p*p*p;
    case 11:{double t=p-1;return 1-t*t*t*t;}
    case 12:{double t=p<.5?p:p-1;double value=8*t*t*t*t;return p<.5?value:1-value;}
    case 13:{double t=2*p-1;double value=t*t*t*t;return p<.5?(1-value)/2:(1+value)/2;}
    case 14:return p*p*p*p*p;
    case 15:{double t=p-1;return t*t*t*t*t+1;}
    case 16:{double t=p<.5?p:p-1;double value=16*t*t*t*t*t;return p<.5?value:1+value;}
    case 17:{double t=2*p-1;return (t*t*t*t*t+1)/2;}
    case 19:case 34:case 24:case 25:case 27:case 32:case 35:case 36:case 39:case 40:case 41:return interpolateNativeCurve(curve,nativeFloat(p),0,1,1);
    //084d90 case1f ->084990 with amplitude0/period0: native defaults are
    //amplitude=delta, period=.3*duration and phase=period/4; retain overshoot.
    case 31:return p==0?0:p==1?1:1+std::pow(2.,-10*p)*std::sin((p-.075)*(2*3.141592653589793)/.3);
    case 70:return progress70(nativeFloat(p),1);
    default:throw std::runtime_error("Unsupported animation easing curve "+std::to_string(curve));
    }
}
float interpolateNativeCurve(int curve,float elapsed,float start,float delta,float duration){
    if(!std::isfinite(elapsed)||!std::isfinite(start)||!std::isfinite(delta)||!std::isfinite(duration)||duration<=0||((curve!=32&&curve!=40)&&(elapsed<0||elapsed>duration)))throw std::runtime_error("Invalid native easing input");
    if(nativeLinearFrameCurve(curve))return nativeAdd(nativeDiv(nativeMul(elapsed,delta),duration),start);
    if(curve==19){
        float u=nativeDiv(elapsed,duration);
        double angle=nativeDouble(double(u)*1.5707963267948966);
        double scaled=nativeDouble(std::sin(angle)*double(delta));
        return nativeFloat(nativeDouble(scaled+double(start)));
    }
    if(curve==34){
        float u=nativeDiv(elapsed,duration);
        float scaled=nativeMul(nativeMul(u,delta),u);
        float back=nativeSub(nativeMul(u,2.70158f),1.70158f);
        return nativeAdd(nativeMul(scaled,back),start);
    }
    if(curve==4){
        float u=nativeFloat(nativeDouble(double(elapsed)/nativeDouble(double(duration)*.5)));
        double half=nativeDouble(double(delta)*.5);
        if(u<1){
            double scaled=nativeDouble(double(nativeMul(u,u))*half);
            return nativeFloat(nativeDouble(scaled+double(start)));
        }
        u=nativeSub(u,1);
        float polynomial=nativeSub(nativeMul(nativeSub(u,2),u),1);
        return nativeFloat(nativeDouble(double(start)-nativeDouble(double(polynomial)*half)));
    }
    if(curve==6||curve==10){
        float u=nativeDiv(elapsed,duration);
        float power=curve==6?nativeMul(u,u):nativeFloat(std::pow(double(u),3.));
        return nativeAdd(nativeMul(power,nativeMul(u,delta)),start);
    }
    if(curve==27){
        if(elapsed==duration)return nativeAdd(start,delta);
        float exponent=nativeDiv(nativeMul(elapsed,-10),duration);
        double power=std::pow(2.,double(exponent));
        double scaled=nativeDouble(nativeDouble(1.-power)*nativeDouble(double(delta)*1.001));
        return nativeFloat(nativeDouble(scaled+double(start)));
    }
    if(curve==39)return nativeBounce(elapsed,start,delta,duration);
    if(curve==41){
        float half=nativeFloat(double(delta)*.5),twice=nativeAdd(elapsed,elapsed);
        if(double(elapsed)<double(duration)*.5)return nativeBounce(twice,start,half,duration);
        float value=nativeBounce(nativeSub(duration,nativeSub(twice,duration)),0,half,duration);
        float middle=nativeFloat(nativeDouble(double(start)+nativeDouble(double(delta)*.5)));
        return nativeAdd(nativeSub(half,value),middle);
    }
    if(curve==32){
        if(elapsed==0)return start;
        float u=nativeFloat(double(elapsed)/(double(duration)*.5));
        if(u==2)return nativeAdd(start,delta);
        float period=nativeFloat(double(duration)*.44999999999999996);
        float quarter=nativeMul(period,.25f),shifted=nativeSub(u,1);
        float numerator=nativeSub(nativeMul(shifted,duration),quarter);
        double angle=nativeDouble(nativeDouble(double(numerator)*6.283185307179586)/double(period));
        double wave=std::sin(angle),power=std::pow(2.,double(nativeMul(shifted,u<1?10.f:-10.f)));
        double value=nativeDouble(nativeDouble(wave*nativeDouble(power*double(delta)))*.5);
        return nativeFloat(u<1?nativeDouble(double(start)-value):nativeDouble(nativeDouble(value+double(delta))+double(start)));
    }
    if(curve==40){
        float twice=nativeAdd(elapsed,elapsed);
        if(double(elapsed)<double(duration)*.5){
            float value=nativeBounce(nativeSub(duration,twice),0,delta,duration);
            float inverted=nativeAdd(nativeSub(delta,value),0);
            return nativeFloat(nativeDouble(double(start)+nativeDouble(double(inverted)*.5)));
        }
        float value=nativeBounce(nativeSub(twice,duration),0,delta,duration);
        double middle=nativeDouble(nativeDouble(double(value)*.5)+nativeDouble(double(delta)*.5));
        return nativeFloat(nativeDouble(double(start)+middle));
    }
    if(curve==2){
        float u=nativeDiv(elapsed,duration);
        return nativeAdd(nativeMul(nativeMul(u,delta),u),start);
    }
    if(curve==3){
        float u=nativeDiv(elapsed,duration);
        return nativeSub(start,nativeMul(nativeMul(u,delta),nativeSub(u,2)));
    }
    if(curve==7){
        float u=nativeSub(nativeDiv(elapsed,duration),1);
        return nativeAdd(nativeMul(nativeAdd(nativeMul(nativeMul(u,u),u),1),delta),start);
    }
    if(curve==11){
        float u=nativeSub(nativeDiv(elapsed,duration),1);
        float cube=nativeFloat(std::pow(double(u),3.));
        return nativeSub(start,nativeMul(nativeSub(nativeMul(cube,u),1),delta));
    }
    if(curve==35){
        float u=nativeSub(nativeDiv(elapsed,duration),1),squared=nativeMul(u,u);
        float back=nativeAdd(nativeMul(u,2.70158f),1.70158f);
        return nativeAdd(nativeMul(nativeAdd(nativeMul(back,squared),1),delta),start);
    }
    if(curve==24||curve==36){
        float u=nativeFloat(double(elapsed)/(double(duration)*.5));double half=double(delta)*.5;
        if(curve==24){
            if(u<1){float root=nativeFloat(std::sqrt(nativeSub(1,nativeMul(u,u))));return nativeFloat(double(start)-double(nativeSub(root,1))*half);}
            u=nativeSub(u,2);float root=nativeFloat(std::sqrt(nativeSub(1,nativeMul(u,u))));return nativeFloat(double(nativeAdd(root,1))*half+double(start));
        }
        float polynomial;
        if(u<1)polynomial=nativeMul(nativeMul(u,u),nativeSub(nativeMul(u,3.5949097f),2.5949097f));
        else{u=nativeSub(u,2);float back=nativeAdd(nativeMul(u,3.5949097f),2.5949097f);polynomial=nativeAdd(nativeMul(nativeMul(back,u),u),2);}
        return nativeFloat(double(polynomial)*half+double(start));
    }
    if(curve!=25)throw std::runtime_error("Unreviewed native circular/back easing curve");
    float half=nativeFloat(double(delta)*.5),twice=nativeAdd(elapsed,elapsed);
    if(double(duration)*.5<=double(elapsed)){
        float u=nativeDiv(nativeSub(twice,duration),duration);
        float root=nativeFloat(std::sqrt(nativeSub(1,nativeMul(u,u))));
        return nativeSub(nativeFloat(double(start)+double(delta)*.5),nativeMul(nativeSub(root,1),half));
    }
    float u=nativeSub(nativeDiv(twice,duration),1);
    float root=nativeFloat(std::sqrt(nativeSub(1,nativeMul(u,u))));
    return nativeAdd(nativeMul(root,half),start);
}
float progress70(float elapsed,float duration){
    if(!std::isfinite(elapsed)||!std::isfinite(duration)||elapsed<0||duration<0)throw std::runtime_error("Invalid native clock curve time");
    if(!duration||elapsed>=duration)return 1;
    if(!elapsed)return 0;
    float third=nativeDiv(duration,3);
    if(std::abs(elapsed)<std::abs(third))return nativeDiv(nativeMul(elapsed,1.3f),third);
    float t=nativeSub(nativeDiv(nativeSub(elapsed,third),nativeDiv(nativeAdd(duration,duration),3)),1);
    return nativeSub(1.3f,nativeMul(nativeAdd(nativeMul(nativeMul(t,t),t),1),.3f));
}
bool hasIdentityTimeRemap(int id){
    static constexpr int changed[]={3096,3097,3098,3099,3100,3101,3120,3121,3179,3185,3186,3187,3188,3384,3385,3386,3469,3534,3670,3906,3907,3908,4235,4342,4343,4546,4654,4655,4657,5192,5193,5229,5268,6673,6675};
    return id>=1&&id<=8372&&!std::binary_search(changed,changed+35,id);
}
float warpTime(int curve,float phase,float duration){
    if(!std::isfinite(phase)||!std::isfinite(duration)||phase<0||duration<=0)throw std::runtime_error("Invalid native effect remap time");
    //084D90(firstRawCurve,phase,0,D,D), preserving original SSE/double stages.
    switch(curve){
    case 0:return nativeDiv(nativeMul(phase,duration),duration);
    case 3:{float r=nativeDiv(phase,duration);return nativeSub(0,nativeMul(nativeMul(r,duration),nativeSub(r,2)));}
    case 4:case 8:{
        const double half=double(duration)*.5;
        float u=nativeFloat(double(phase)/half);
        if(curve==4){
            if(u<1)return nativeFloat(double(nativeMul(u,u))*half);
            u=nativeSub(u,1);float y=nativeSub(nativeMul(nativeSub(u,2),u),1);
            return nativeFloat(0.-double(y)*half);
        }
        if(u<1)return nativeFloat(double(nativeFloat(std::pow(double(u),3.)))*half);
        u=nativeSub(u,2);return nativeFloat(double(nativeAdd(nativeMul(nativeMul(u,u),u),2))*half);
    }
    case 6:{float r=nativeDiv(phase,duration);return nativeMul(nativeMul(r,r),nativeMul(r,duration));}
    case 7:{float r=nativeSub(nativeDiv(phase,duration),1);return nativeMul(nativeAdd(nativeMul(nativeMul(r,r),r),1),duration);}
    default:throw std::runtime_error("Unsupported overall native effect curve "+std::to_string(curve));
    }
}
PreparedTime prepareTime(const Clip& clip,uint64_t elapsed,bool done){
    PreparedTime time;time.segmentLinear=clip.timeRemap;
    if(!clip.duration){time.done=done;return time;}
    if(clip.loopStart>=clip.duration)throw std::runtime_error("Invalid native effect loop range");
    if(elapsed>clip.duration){
        if(clip.loopStart)elapsed=clip.loopStart+(elapsed-clip.duration)%(clip.duration-clip.loopStart);
        else {elapsed=clip.duration;done=true;}
    }
    time.phase=done?clip.duration:elapsed;time.done=done;
    time.nativeTime=clip.timeRemap?warpTime(clip.timeCurve,nativeFloat(time.phase),nativeFloat(clip.duration)):nativeFloat(time.phase);
    if(clip.timeRemap){
        if(!std::isfinite(time.nativeTime)||double(time.nativeTime)<INT32_MIN||double(time.nativeTime)>=2147483648.)throw std::runtime_error("Native effect remap exceeds integer time range");
        time.mapped=int32_t(time.nativeTime);
    }else{
        if(time.phase>INT32_MAX)throw std::runtime_error("Native effect phase exceeds integer time range");
        time.mapped=int32_t(time.phase);
    }
    return time;
}
std::map<int,Clip> load(const std::filesystem::path& path) {
    std::ifstream input(path);std::map<int,Clip> clips;
    if(!input)return clips;
    std::string line;size_t count=0;
    while(std::getline(input,line)) {
        if(line.empty()||line[0]=='#')continue;
        if(++count>100000||line.size()>512)throw std::runtime_error("Effect table exceeds limits");
        std::istringstream s(line);s.imbue(std::locale::classic());int id,time,loop,duration;Frame f;
        if(!(s>>id>>time>>f.values[X]>>f.values[Y]>>f.values[Alpha]>>f.values[ScaleX]>>f.values[ScaleY]>>f.values[Rotation]>>f.curve>>f.mode>>loop>>duration))throw std::runtime_error("Malformed effect table row");
        std::string extra;if(s>>extra)throw std::runtime_error("Unexpected effect table column");
        if(id<1||id>8372||time<0||loop<0||duration<=0||time>duration||loop>=duration)throw std::runtime_error("Invalid effect table range");
        checkMode(f.mode);
        f.time=uint32_t(time);auto& clip=clips[id];
        if(clip.frames.empty()){clip.timeRemap=!hasIdentityTimeRemap(id);clip.timeCurve=sentinel(f.curve)?0:f.curve;}
        if(!clip.frames.empty()&&(clip.duration!=uint32_t(duration)||clip.loopStart!=uint32_t(loop)||clip.frames.back().time>f.time))throw std::runtime_error("Inconsistent effect keyframes");
        clip.duration=uint32_t(duration);clip.loopStart=uint32_t(loop);clip.frames.push_back(f);
    }
    return clips;
}
void loadWindows(std::map<int,Clip>& clips,const std::filesystem::path& path) {
    std::ifstream input(path);if(!input)return;
    auto candidate=clips;std::map<int,size_t> counts;std::map<int,Frame> global;
    std::string line;size_t total=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        if(++total>100000||line.size()>512)throw std::runtime_error("Window effect table exceeds limits");
        std::istringstream row(line);int id,index,identity;std::string bits,extra;std::array<int32_t,6> values;
        if(!(row>>id>>index>>identity>>bits>>values[0]>>values[1]>>values[2]>>values[3]>>values[4]>>values[5])||row>>extra||!candidate.count(id)||index<0||size_t(index)>=candidate.at(id).frames.size()||identity<0||identity>=28141||bits.size()<3||bits.substr(0,2)!="0x")throw std::runtime_error("Malformed native window effect row");
        for(auto value:values)if(value<INT16_MIN||value>INT16_MAX)throw std::runtime_error("Native window keyframe outside signed16 range");
        size_t parsed=0;uint64_t descriptor=std::stoull(bits,&parsed,16);if(parsed!=bits.size())throw std::runtime_error("Invalid native effect descriptor");
        auto& clip=candidate.at(id);if(size_t(index)!=counts[id]++||(clip.windowMetadata&&clip.descriptor!=descriptor))throw std::runtime_error("Duplicate or unordered native window keyframe");
        clip.windowMetadata=true;clip.descriptor=descriptor;auto& frame=clip.frames[size_t(index)];frame.globalIndex=identity;frame.window=values;
        auto [it,inserted]=global.emplace(identity,frame);
        if(!inserted&&(it->second.time!=frame.time||it->second.values!=frame.values||it->second.curve!=frame.curve||it->second.mode!=frame.mode||it->second.window!=frame.window))throw std::runtime_error("Inconsistent shared native keyframe identity");
    }
    for(const auto& pair:candidate)if(counts[pair.first]!=pair.second.frames.size())throw std::runtime_error("Incomplete native window keyframe sidecar");
    clips=std::move(candidate);
}

void loadProperties(std::map<int,Clip>& clips,const std::filesystem::path& path) {
    std::ifstream input(path);if(!input)return;
    auto candidate=clips;std::map<int,size_t> counts;std::map<int,std::array<int32_t,13>> global;
    std::string line;size_t total=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        if(++total>100000||line.size()>1024)throw std::runtime_error("Effect property table exceeds limits");
        std::istringstream row(line);int id,index,identity;std::string bits,extra;std::array<int32_t,13> values;
        if(!(row>>id>>index>>identity>>bits)||!candidate.count(id)||index<0||size_t(index)>=candidate.at(id).frames.size()||identity<0||identity>=28141||bits.size()<3||bits.substr(0,2)!="0x")throw std::runtime_error("Malformed native effect property row");
        for(auto& value:values)if(!(row>>value)||value<INT16_MIN||value>INT16_MAX)throw std::runtime_error("Native effect property outside signed16 range");
        if(row>>extra)throw std::runtime_error("Unexpected native effect property column");
        size_t parsed=0;uint64_t descriptor=std::stoull(bits,&parsed,16);if(parsed!=bits.size())throw std::runtime_error("Invalid native effect descriptor");
        auto& clip=candidate.at(id);auto& frame=clip.frames[size_t(index)];
        if(size_t(index)!=counts[id]++||((clip.windowMetadata||clip.propertiesMetadata)&&clip.descriptor!=descriptor)||(frame.globalIndex>=0&&frame.globalIndex!=identity))throw std::runtime_error("Inconsistent native effect property identity");
        checkMode(frame.mode);
        if((descriptor&0x1000)&&!sentinel(values[size_t(AuxResourceAndScale-OriginX)])){auto v=values[size_t(AuxResourceAndScale-OriginX)];if(!((id==7007&&v==1)||(id==8372&&(v==1||v==21))))throw std::runtime_error("Unreviewed native effect noise field26");}
        clip.nativeId=id;
        clip.propertiesMetadata=true;clip.descriptor=descriptor;frame.globalIndex=identity;frame.properties=values;
        auto [it,inserted]=global.emplace(identity,values);
        if(!inserted&&it->second!=values)throw std::runtime_error("Inconsistent shared native effect property keyframe");
    }
    for(const auto& pair:candidate)if(counts[pair.first]!=pair.second.frames.size())throw std::runtime_error("Incomplete native effect property sidecar");
    clips=std::move(candidate);
}

bool isSelected(const Clip& clip,Field field){if(field<X||field>Priority)throw std::runtime_error("Invalid native effect field");return selected(clip,field);}

double sample(const Clip& clip,uint64_t elapsed,Field field,double initial,const FrameOverrides* overrides) {
    return samplePrepared(clip,prepareTime(clip,elapsed),field,initial,overrides);
}
double samplePrepared(const Clip& clip,const PreparedTime& time,Field field,double initial,const FrameOverrides* overrides) {
    if(field<X||field>Priority)throw std::runtime_error("Invalid native effect field");
    if(field>=OriginX&&!clip.propertiesMetadata)throw std::runtime_error("Missing native effect property sidecar");
    if(clip.frames.empty()||clip.duration==0)return initial;
    if(!selected(clip,field))return initial;
    const int64_t elapsed=time.mapped;
    auto at=std::lower_bound(clip.frames.begin(),clip.frames.end(),elapsed,[](const Frame& f,int64_t t){return int64_t(f.time)<t;});
    size_t index=at==clip.frames.end()?clip.frames.size()-1:size_t(at-clip.frames.begin());
    size_t metadata=index?index-1:0;int curve=0,mode=0;
    for(size_t i=metadata+1;i-->0;)if(!sentinel(clip.frames[i].curve)){curve=clip.frames[i].curve;break;}
    if(time.segmentLinear)curve=0;
    for(size_t i=metadata+1;i-->0;)if(!sentinel(clip.frames[i].mode)){mode=clip.frames[i].mode;break;}
    if(forcedStep(field))mode=2;checkMode(mode);
    if(mode==1&&clip.frames.size()>128)throw std::runtime_error("Native mode1 exceeds reviewed128-frame bound");
    const int skip=field==Rotation?-100000:-32766;
    const int hold=field==Rotation?-100001:-32767;
    auto valueAt=[&](size_t i){const auto& frame=clip.frames[i];
        int value=field<WindowX?frame.values[size_t(field)]:field<OriginX?frame.window[size_t(field-WindowX)]:frame.properties[size_t(field-OriginX)];
        if(overrides){auto found=overrides->find(frame.globalIndex);if(found!=overrides->end()){const auto& captured=found->second;
            if(field==X)value=captured.x;else if(field==Y)value=captured.y;else if(field==Alpha)value=captured.alpha;else if(field>=WindowX&&field<=WindowHeight)value=captured.window[size_t(field-WindowX)];}}
        return value;
    };
    size_t next=index;while(valueAt(next)==skip&&next+1<clip.frames.size())++next;
    auto previousValue=[&](size_t from){
        for(size_t i=from+1;i-->0;){int value=valueAt(i);if(value!=hold&&value!=skip)return grid(value,field);}
        return initial;
    };
    //Original A3E70 returns the supplied float initial at a skipped final raw key.
    //This is evaluated before endpoint backtracking and applies after one time normalization.
    if(valueAt(next)==skip&&next+1==clip.frames.size()&&clip.frames[next].time<=elapsed)return nativeFloat(initial);
    //Native starts with the first usable key even if its raw leading row is skip;
    //a leading hold remains initial until a real key is actually reached.
    if(index&&clip.frames[next].time<=elapsed&&mode==1&&valueAt(next)!=hold&&valueAt(next)!=skip){
        double endpoint=previousValue(next);
        return modeOne(clip.frames.size(),index,valueAt,hold,skip,initial,endpoint,endpoint,1);
    }
    if(!index||clip.frames[next].time<=elapsed)return previousValue(next);
    size_t previous=index-1;while(valueAt(previous)==skip&&previous)--previous;
    int endValue=valueAt(next),startValue=valueAt(previous);
    if(endValue==hold)return previousValue(index);
    if(startValue==hold){
        bool found=false;
        for(size_t i=previous+1;i-->0;){int value=valueAt(i);if(value!=hold&&value!=skip){startValue=value;found=true;break;}}
        if(!found)return initial;
    }
    if(startValue==skip||endValue==skip)return initial;
    double start=grid(startValue,field),end=grid(endValue,field);
    if(mode==2)return start;
    if(mode==0&&(nativeLinearFrameCurve(curve)||curve==19||curve==34||curve==2||curve==3||curve==4||curve==6||curve==10||curve==11||curve==27||curve==32||curve==39||curve==40||curve==41)){
        float p=interpolateNativeCurve(curve,nativeFloat(elapsed-clip.frames[previous].time),0,1,nativeFloat(clip.frames[next].time-clip.frames[previous].time));
        return nativeAdd(nativeMul(nativeSub(nativeFloat(end),nativeFloat(start)),p),nativeFloat(start));
    }
    if(mode==0&&(curve==7||curve==24||curve==25||curve==35||curve==36))return interpolateNativeCurve(curve,nativeFloat(elapsed-clip.frames[previous].time),nativeFloat(start),nativeSub(nativeFloat(end),nativeFloat(start)),nativeFloat(clip.frames[next].time-clip.frames[previous].time));
    double p=mode==1&&curve==7?interpolateNativeCurve(7,nativeFloat(elapsed-clip.frames[previous].time),0,1,nativeFloat(clip.frames[next].time-clip.frames[previous].time)):ease(double(elapsed-clip.frames[previous].time)/double(clip.frames[next].time-clip.frames[previous].time),nativeLinearFrameCurve(curve)?0:curve);
    if(mode==1)return modeOne(clip.frames.size(),index,valueAt,hold,skip,initial,start,end,p);
    return start+(end-start)*p;
}

Properties sampleProperties(const Clip& clip,uint64_t elapsed,const Properties& initial) {
    return samplePropertiesPrepared(clip,prepareTime(clip,elapsed),initial);
}
Properties samplePropertiesPrepared(const Clip& clip,const PreparedTime& time,const Properties& initial) {
    if(!clip.propertiesMetadata)throw std::runtime_error("Missing native effect property sidecar");
    Properties p=initial;
    auto get=[&](Field field,double value){return integer(samplePrepared(clip,time,field,value));};
    if(selected(clip,OriginX))p.originX=integer(float(samplePrepared(clip,time,OriginX,(float(initial.originX)*480.f)/640.f))*640.f/480.f);
    if(selected(clip,OriginY))p.originY=integer(float(samplePrepared(clip,time,OriginY,(float(initial.originY)*360.f)/480.f))*480.f/360.f);
    p.blendMode=get(BlendMode,initial.blendMode);if(selected(clip,BlendMode)&&(p.blendMode<1||p.blendMode>25))p.blendMode=0;
    p.colorMode=get(ColorMode,initial.colorMode);
    if(selected(clip,ColorMode)){
        p.rgb=0;
        if(p.colorMode==1){
            int r=get(ColorR,0),g=get(ColorG,0),b=get(ColorB,0);
            if(r<0||r>255||g<0||g>255||b<0||b>255)throw std::runtime_error("Unsupported native effect RGB range");
            p.rgb=uint32_t((r<<16)|(g<<8)|b);
        }
    }
    p.variantX=get(VariantX,initial.variantX);p.variantY=get(VariantY,initial.variantY);
    if(p.variantX<0||p.variantY<0)throw std::runtime_error("Unsupported negative native effect image variant");
    p.brightness=get(Brightness,initial.brightness);p.contrast=get(Contrast,initial.contrast);
    p.priority=get(Priority,initial.priority);
    if(p.priority<0||p.priority>=0x1000000)throw std::runtime_error("Invalid native effect draw priority");
    if(selected(clip,AuxResourceAndScale)&&std::any_of(clip.frames.begin(),clip.frames.end(),[](const Frame& f){return !sentinel(f.properties[size_t(AuxResourceAndScale-OriginX)]);})){if(clip.nativeId!=8372&&clip.nativeId!=7007)throw std::runtime_error("Unreviewed native effect noise identity");p.decimalMode=get(AuxResourceAndScale,initial.decimalMode);if(!((clip.nativeId==7007&&p.decimalMode==1)||(clip.nativeId==8372&&(p.decimalMode==1||p.decimalMode==21))))throw std::runtime_error("Unreviewed sampled native effect noise");}
    return p;
}
}
