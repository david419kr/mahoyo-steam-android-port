#include "choices.h"
#include <atomic>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace mahoyo::choices {
namespace {
std::atomic<uint64_t> counter{0};
uint64_t fresh(){auto value=counter.load();for(;;){if(value==UINT64_MAX)throw std::runtime_error("Choice revision exhausted");if(counter.compare_exchange_weak(value,value+1))return value+1;}}
void fail(const char* reason){throw std::invalid_argument(reason);}
}
bool validUtf8(const std::string& text){
    for(size_t i=0;i<text.size();){auto a=uint8_t(text[i++]);if(a<0x80)continue;unsigned n=0;uint32_t value=0,min=0;
        if(a>=0xc2&&a<=0xdf){n=1;value=a&31;min=0x80;}else if(a>=0xe0&&a<=0xef){n=2;value=a&15;min=0x800;}else if(a>=0xf0&&a<=0xf4){n=3;value=a&7;min=0x10000;}else return false;
        if(n>text.size()-i)return false;while(n--){auto b=uint8_t(text[i++]);if((b&0xc0)!=0x80)return false;value=(value<<6)|(b&63);}if(value<min||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
    }return true;
}
void Controller::validateRequest(const Request&r){if(r.kind!=2||r.index!=0||r.arg2!=2||r.arg3!=0)fail("Unsupported choice request; requires native[2,0,2,0]");}
void Controller::append(int32_t value,std::string raw){
    if(pending_)fail("Unsupported choice append during pending job");
    if(queue_.size()>=4096||raw.size()>1024*1024)fail("Choice port resource limit exceeded");
    if(!validUtf8(raw))fail("Unsupported non-UTF8 choice label");
    queue_.push_back({value,std::move(raw)});
}
void Controller::beginText(const Request&r,const std::vector<std::string>& labels){
    validateRequest(r);if(pending_)fail("Unsupported pending choice collision");if(queue_.empty())fail("Unsupported empty native text choice");if(labels.size()!=queue_.size())fail("Choice label count mismatch");
    State s;s.revision=fresh();s.request=r;s.profile=Profile::Text161;
    for(size_t i=0;i<queue_.size();++i){if(!validUtf8(labels[i]))fail("Invalid resolved choice UTF8");s.options.push_back({int(i),queue_[i].value,labels[i],true,std::nullopt});}pending_=std::move(s);
}
void Controller::beginWik(const Request&r,const std::vector<Layout>& rows){
    validateRequest(r);if(pending_)fail("Unsupported pending choice collision");if(rows.size()!=14)fail("Native WIK choice requires14 slots");
    State s;s.revision=fresh();s.request=r;s.profile=Profile::Wik164;
    for(size_t i=0;i<rows.size();++i){if(rows[i].slot!=int(i)||rows[i].enabled!=1)fail("Invalid Native WIK slot sequence");s.options.push_back({int(i),int32_t(i),"",true,rows[i]});}pending_=std::move(s);
}
void Controller::prepared(){if(!pending_)fail("Missing pending choice prepare");pending_->ready=true;}
std::optional<Completion> Controller::select(const std::string& revision,int ordinal,bool allowed)const{
    if(!allowed||!pending_||!pending_->ready||revision!=std::to_string(pending_->revision)||ordinal<0||size_t(ordinal)>=pending_->options.size())return std::nullopt;
    const auto& option=pending_->options[size_t(ordinal)];if(option.ordinal!=ordinal||!option.enabled)return std::nullopt;
    return Completion{pending_->request.kind,pending_->request.index,option.result,pending_->revision};
}
void Controller::complete(uint64_t revision){if(!pending_||pending_->revision!=revision)fail("Stale choice completion");if(pending_->profile==Profile::Text161)queue_.clear();pending_.reset();}
void Controller::reset()noexcept{pending_.reset();queue_.clear();}

std::vector<Layout> parseWikLayout(const std::string& text){
    std::istringstream in(text);std::string line;bool header=false;std::vector<Layout> rows;uint32_t priorities=0;
    const std::string expected="# WIKCHOICELAYOUT1 canvas=1920x1080 units=parent-canvas-pixels sourceUnits=source-pixels initialFocus=-1 monoState=0 colorState=1";
    while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;if(line[0]=='#'){if(!header){if(line!=expected)fail("Unsupported WIK choice layout schema");header=true;}continue;}
        if(!header||rows.size()>=14)fail("Invalid WIK choice layout count");Layout r;std::istringstream row(line);std::string extra;
        if(!(row>>r.slot>>r.mono>>r.color>>r.x>>r.y>>r.width>>r.height>>r.priority>>r.enabled>>r.hitMode>>r.anchorX>>r.anchorY>>r.sourceWidth>>r.sourceHeight)||(row>>extra))fail("Invalid WIK choice layout row");
        //14 values: twelve integer fields and two asset keys.
        std::ostringstream stem;stem<<"wik_sel"<<std::setfill('0')<<std::setw(2)<<rows.size();
        if(r.slot!=int(rows.size())||r.mono!=stem.str()+"_mono.mzp"||r.color!=stem.str()+".mzp"||r.enabled!=1||r.hitMode!=1||r.anchorX||r.anchorY||r.width<=0||r.height<=0||r.width>16384||r.height>16384||r.sourceWidth!=r.width||r.sourceHeight!=r.height||r.priority<0||r.priority>=14||(priorities&(1u<<r.priority)))fail("Unsupported native WIK choice profile");
        priorities|=1u<<r.priority;
        rows.push_back(std::move(r));
    }if(!header||rows.size()!=14)fail("Missing native WIK choice layout");return rows;
}
} //namespace mahoyo::choices
