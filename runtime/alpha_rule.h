#pragma once
// Native transient RGBA alpha-rule. Save/clone/capture require its removal.
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
namespace mahoyo::alpha_rule {
struct Context {std::string image;int width,height,parent,blend,flag,color,contrast,brightness,rgb;bool crop,window,wave,noise,children;int sourceFormat=2;};
inline void validate(const Context& c){
    // Both operators are already supported independently. Color1 replaces RGB
    // with a constant and preserves alpha, so it commutes with the mask pass.
    bool source=!c.image.empty()&&c.width>0&&c.height>0&&c.width<=8192&&c.height<=8192&&c.sourceFormat==2;
    bool material=(c.blend==0||c.blend==16)&&!c.flag&&(c.color==0||c.color==1)&&!c.contrast&&!c.brightness&&
        (c.color==0?c.rgb==0:c.rgb>=0&&c.rgb<=0xffffff);
    if(!source||!material||c.parent>=0||c.crop||c.window||c.wave||c.noise||c.children)throw std::runtime_error("Unreviewed mask7 RGBA material/context");
}
inline uint64_t nextSession(){static uint64_t serial=0;if(serial==UINT64_MAX)throw std::runtime_error("Mask session exhausted");return ++serial;}
inline float f(float x){volatile float out=x;return out;}
struct Frame {bool enabled=false;int p=0,s=256,reference=1;};
struct State {uint64_t token=0,begin=0;bool pending=true,queued=false,started=false;Frame frame{true,0,37,0};int rule=7,duration=0,rawStrength=27;};
class Controller {
    std::map<int,State> values;uint64_t session=nextSession(),serial=0;
public:
    bool contains(int id)const{return values.count(id)!=0;}
    bool empty()const{return values.empty();}
    bool pending()const{for(const auto& p:values)if(p.second.pending)return true;return false;}
    bool running()const{for(const auto& p:values)if(p.second.started&&p.second.frame.enabled)return true;return false;}
    void request(int id,int rule,int middle,int strength,const Context& c){validate(c);if(id<0||id>=64||rule<1||rule>14||strength<0||strength>256||middle||contains(id))throw std::runtime_error("Unreviewed transient alpha-rule request");State s;s.token=++serial;s.rule=rule;s.rawStrength=strength;s.frame={true,0,strength<50?37:strength,0};values.emplace(id,s);}
    void fade(int id,int from,int to,int duration,int curve,double logicalAlpha){auto& s=values.at(id);if(!s.pending||s.queued||from!=INT32_MIN||to!=255||duration<=0||duration>86400000||curve||logicalAlpha!=0)throw std::runtime_error("Unreviewed mask7 alpha fade");s.duration=duration;s.queued=true;}
    void prepare()const{for(const auto& p:values)if(p.second.pending&&!p.second.queued)throw std::runtime_error("Mask7 prepare without reviewed fade");}
    std::string ack(uint64_t transition,uint64_t version)const{return std::to_string(transition)+":"+std::to_string(session)+":"+std::to_string(version);}
    void prepared(uint64_t tick){prepare();auto candidate=values;for(auto& p:candidate)if(p.second.pending){p.second.pending=false;p.second.started=true;p.second.begin=tick;}values.swap(candidate);}
    void sample(uint64_t tick){for(auto& p:values){auto& s=p.second;if(!s.started)continue;if(tick<s.begin)throw std::runtime_error("Mask7 backwards clock");auto t=tick-s.begin;if(t>=uint64_t(s.duration)){s.frame={};continue;}int q=int(f(f(float(t)/float(s.duration))*256.f));if(s.rawStrength>=50){s.frame={true,q,s.rawStrength,0};continue;}float u=f(float(q)*.00390625f);if(u<=f(.3f))s.frame={true,int(f(f(f(u*f(.18f))/f(.3f))*256.f)),37,0};else{float d=f(u-f(.3f));s.frame={true,int(f(f(f(d*.5f)+f(.18f))*256.f)),std::max(20,int(f(37.f-f(d*60.f)))),0};}}}
    uint64_t remaining(int id,uint64_t tick)const{uint64_t result=0;for(const auto& p:values)if(id<0||p.first==id){const auto& s=p.second;if(s.pending)result=std::max(result,uint64_t(s.duration));else if(s.started&&s.frame.enabled){if(tick<s.begin)throw std::runtime_error("Mask7 backwards clock");auto age=tick-s.begin;if(age<uint64_t(s.duration))result=std::max(result,uint64_t(s.duration)-age);}}return result;}
    void clear(int id){auto i=values.find(id);if(i!=values.end()&&i->second.started&&i->second.frame.enabled)throw std::runtime_error("Unreviewed early mask7 clear");values.erase(id);}
    void remove(int id){if(id<0){values.clear();session=nextSession();}else values.erase(id);}
    void rejectCapture()const{if(!empty())throw std::runtime_error("Transient alpha-rule save/clone/capture is unsupported");}
    double opacity(int id,double logical)const{return contains(id)?1.:logical;}
    std::string json(int id)const{const auto& s=values.at(id);if(!s.frame.enabled)return "null";std::ostringstream out;out<<"{\"token\":\""<<session<<':'<<s.token<<"\",\"rule\":"<<s.rule<<",\"image\":\"rule_"<<(s.rule<10?"0":"")<<s.rule<<".cbg\",\"sourceFormat\":2,\"P\":"<<s.frame.p<<",\"S\":"<<s.frame.s<<",\"reference\":0,\"liveAlphaFixed16\":65536}";return out.str();}
};
}
