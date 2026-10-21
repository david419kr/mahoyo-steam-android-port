#include "particle_overlay.h"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace mahoyo::particles {
namespace {
float f(float value){volatile float result=value;return result;}
std::array<float,8> quad(float x,float y,float w,float h,float nativeAngle=0){
    const float radians=float((-double(nativeAngle)*3.141592653589793)/180.0);
    const float c=std::cos(radians),s=std::sin(radians);
    std::array<float,8> out{};
    const std::array<float,8> corners{{0,0,0,h,w,0,w,h}};
    //074710 subtracts the parent center; 0749E0 adds it after projection1.
    const float tx=f(x-960.f),ty=f(y-540.f);
    for(size_t i=0;i<out.size();i+=2){
        out[i]=f(f(f(f(corners[i]*c)-f(corners[i+1]*s))+tx)+960.f);
        out[i+1]=f(f(f(f(corners[i+1]*c)+f(corners[i]*s))+ty)+540.f);
    }
    return out;
}
bool decimal(const std::string& text,uint64_t& value){
    if(text.empty()||text.size()>20||(text.size()>1&&text[0]=='0'))return false;
    for(char c:text)if(c<'0'||c>'9')return false;
    auto r=std::from_chars(text.data(),text.data()+text.size(),value);
    return r.ec==std::errc{}&&r.ptr==text.data()+text.size();
}
void familyJson(std::ostream& out,const Family& family){
    out<<"{\"revision\":\""<<family.revision<<"\",\"active\":"<<(family.active?"true":"false")
       <<",\"pendingResource\":"<<(family.pendingResource?"true":"false")<<",\"resources\":[";
    for(size_t i=0;i<family.resources.size();++i){if(i)out<<',';out<<std::quoted(family.resources[i]);}
    out<<"],\"sprites\":[";
    for(size_t i=0;i<family.sprites.size();++i){
        const auto& sprite=family.sprites[i];if(i)out<<',';
        out<<"{\"resource\":"<<std::quoted(sprite.resource)<<",\"sourceRect\":[";
        for(size_t n=0;n<4;++n){if(n)out<<',';out<<sprite.sourceRect[n];}
        out<<"],\"quad\":[";for(size_t n=0;n<8;++n){if(n)out<<',';out<<sprite.quad[n];}
        out<<"],\"opacity8\":"<<unsigned(sprite.opacity8)<<",\"priority\":"<<sprite.priority<<'}';
    }
    out<<"]}";
}
}
uint64_t Overlays::revision(){
    if(serial_==std::numeric_limits<uint64_t>::max())throw std::runtime_error("Particle revision exhausted");
    return ++serial_;
}
void Overlays::commit(){
    fall_.commit();focus_.commit(1920,1080);
    if(fall_.pendingResource()&&!fallAwaiting_){fallAwaiting_=true;fallRevision_=revision();}
    if(!fall_.active()&&!fall_.pendingResource()){fallAwaiting_=false;fallRevision_=0;}
    if(focus_.resourcesPending()&&!focusRevision_)focusRevision_=revision();
    if(!focus_.active())focusRevision_=0;
}
bool Overlays::acknowledge(uint64_t sceneVersion,const std::string& payload,uint64_t now,bool paused){
    if(paused)return false;
    std::istringstream input(payload);std::string version,fall,focus,extra;
    if(!(input>>version>>fall>>focus)||(input>>extra))return false;
    uint64_t v=0,a=0,b=0;if(!decimal(version,v)||!decimal(fall,a)||!decimal(focus,b)||v!=sceneVersion)return false;
    const auto expectedFall=fallAwaiting_?fallRevision_:0;
    const auto expectedFocus=focus_.resourcesPending()?focusRevision_:0;
    if(a!=expectedFall||b!=expectedFocus||(!a&&!b))return false;
    if(a){fall_.resourcesReady(now);fall_.rebase(now);fallAwaiting_=false;}
    if(b)focus_.resourcesReady(now);
    return true;
}
void Overlays::step(uint64_t now,bool ready,bool paused,const std::function<uint32_t()>& random){
    if(fall_.active()&&!fallAwaiting_)fall_.step(now,ready,paused,random);
    focus_.sample(now,ready,paused,random);
    if(!fall_.active()&&!fall_.pendingResource())fallRevision_=0;
    if(!focus_.active())focusRevision_=0;
}
void Overlays::restore(const fall_note::State& fall,const focus_line::State& focus,uint64_t now){
    fall_note::validate(fall);focus_line::validate(focus);
    auto candidate=*this;candidate.fall_.restore(fall);candidate.focus_.restore(focus,now);
    candidate.fallAwaiting_=candidate.fall_.active()||candidate.fall_.pendingResource();
    candidate.fallRevision_=candidate.fallAwaiting_?candidate.revision():0;
    candidate.focusRevision_=candidate.focus_.active()?candidate.revision():0;
    *this=std::move(candidate);
}
void Overlays::clear(){fall_=fall_note::Controller{};focus_.clear();fallAwaiting_=false;fallRevision_=focusRevision_=0;}
Frame Overlays::frame()const{
    Frame result;
    auto& fall=result.fallNote;fall.revision=fallRevision_;fall.pendingResource=fallAwaiting_;
    fall.active=fall_.active()&&!fallAwaiting_;
    if(fall.active||fall.pendingResource)fall.resources={"FallNote0.mzp","FallNote1.mzp","FallNote2.mzp"};
    if(fall.active)for(const auto& s:fall_.sprites()){
        const float x=f(f(1920.f*s.referenceX)/640.f),y=f(f(1080.f*s.referenceY)/448.f);
        fall.sprites.push_back(Sprite{fall.resources[size_t(s.type)],{s.left,s.top,s.width,s.height},quad(x,y,float(s.width),float(s.height)),s.opacity8,s.priority});
    }
    auto& focus=result.focusLine;focus.revision=focusRevision_;focus.pendingResource=focus_.resourcesPending();
    focus.active=focus_.active()&&!focus.pendingResource;
    if(focus.active||focus.pendingResource)focus.resources={"FocusLine.mzp"};
    if(focus.active){const auto sprites=focus_.sprites();const auto state=focus_.snapshot();
        for(size_t i=0;i<sprites.size();++i)if(state.slots[i].type>=0){const auto& s=sprites[i];
            focus.sprites.push_back(Sprite{"FocusLine.mzp",{0,0,960,2},quad(s.x,s.y,960.f,2.f,s.thetaDegrees),s.opacity8,focus_line::Priority});
        }
    }
    return result;
}
std::string Overlays::json()const{
    const auto value=frame();std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    out<<"{\"fallNote\":";familyJson(out,value.fallNote);out<<",\"focusLine\":";familyJson(out,value.focusLine);out<<'}';return out.str();
}
} // namespace mahoyo::particles
