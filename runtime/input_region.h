#pragma once
#include <algorithm>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace mahoyo::input_region {
inline uint64_t identity(){static uint64_t value=0;if(value==UINT64_MAX)throw std::runtime_error("Input identity exhausted");return ++value;}
struct Stable {
    bool active=false,currentEnabled=false,pendingDirty=false,pendingEnabled=false;
    uint32_t currentTarget=0,pendingTarget=0;
};
//Native120/122 stable state plus volatile Android readiness/physical input.
//Android UI closure/ack is port policy, not a name assigned to Native slot43.
class Controller {
    Stable state_;
    uint64_t generation_=identity(),request_=0,trigger_=0,epoch_=identity(),last_=0;
    int readyKind_=0,latch_=0,elapsed_=0,phase_=2;
    bool held_=false,jump_=false;
    uint32_t jumpTarget_=0;
public:
    const Stable& stable()const{return state_;}
    uint64_t generation()const{return generation_;}uint64_t request()const{return request_;}
    uint64_t trigger()const{return trigger_;}uint64_t epoch()const{return epoch_;}
    int readyKind()const{return readyKind_;}int elapsed()const{return elapsed_;}
    int progressLeft()const{return 4+elapsed_*192/3000;}
    bool restricted()const{return state_.active;}
    bool readyPending()const{return readyKind_!=0;}
    bool needsTick()const{return !jump_&&!readyPending()&&((held_&&latch_==1)||phase_==3);}
    bool jumpQueued()const{return jump_;}uint32_t jumpTarget()const{return jumpTarget_;}
    bool safe()const{return !readyPending()&&!state_.pendingDirty&&!held_&&phase_!=3&&!jump_;}
    bool matches(uint64_t gen,uint64_t trigger,uint64_t epoch)const{return gen==generation_&&trigger==trigger_&&epoch==epoch_&&trigger_!=0;}
    void physicalReset(){held_=false;latch_=0;elapsed_=0;epoch_=identity();}
    void reset(){state_={};generation_=identity();request_=trigger_=0;readyKind_=0;phase_=2;jump_=false;jumpTarget_=0;physicalReset();}
    bool begin120(){if(state_.active)return false;state_.active=true;request_=identity();readyKind_=1;physicalReset();return true;}
    void begin118(){request_=identity();readyKind_=1;}
    bool ready(uint64_t gen,uint64_t request){if(gen!=generation_||request!=request_||!readyPending())return false;readyKind_=0;request_=0;return true;}
    void release121(){state_.active=false;}
    void request122(bool enabled,uint32_t target){state_.pendingDirty=true;state_.pendingEnabled=enabled;state_.pendingTarget=enabled?target:0;}
    void commit(){
        if(!state_.pendingDirty)return;
        state_.currentEnabled=state_.pendingEnabled;state_.currentTarget=state_.pendingTarget;
        state_.pendingDirty=false;state_.pendingEnabled=false;state_.pendingTarget=0;
        physicalReset();
        //Native state3 has already selected typeE; script cancellation cannot
        //retract that edge. A session reset/load can invalidate old callbacks.
        if(phase_!=3&&!jump_){phase_=2;trigger_=state_.currentEnabled?identity():0;}
    }
    bool pointer(uint64_t gen,uint64_t trigger,uint64_t epoch,bool down,bool inside,uint64_t now){
        if(!matches(gen,trigger,epoch)||readyPending()||!state_.currentEnabled||jump_)return false;
        if(!down){held_=false;latch_=0;elapsed_=0;return true;}
        if(held_)return true;
        held_=true;latch_=inside?1:2;elapsed_=0;last_=now;return true;
    }
    void sample(uint64_t now,bool runnable){
        if(!runnable||readyPending()||jump_)return;
        if(phase_==3){jump_=true;held_=false;return;}
        if(!state_.currentEnabled||!held_||latch_!=1)return;
        if(now<last_)throw std::runtime_error("Input hold clock moved backwards");
        elapsed_=std::min(3000,elapsed_+int(std::min<uint64_t>(17,now-last_)));last_=now;
        if(elapsed_==3000){phase_=3;jumpTarget_=state_.currentTarget;}
    }
    void jumped(){jump_=false;phase_=2;jumpTarget_=0;physicalReset();trigger_=state_.currentEnabled?identity():0;}
    void write(std::ostream& out)const{
        if(!safe())throw std::runtime_error("Input region transaction is busy");
        out<<"INPUTREGION1 "<<state_.active<<' '<<state_.currentEnabled<<' '<<state_.currentTarget<<' '<<state_.pendingDirty<<' '<<state_.pendingEnabled<<' '<<state_.pendingTarget<<'\n';
    }
    template<class Validate> static Stable read(std::istream& in,Validate validate){
        std::string tag;int a,c,p,e;int64_t ct,pt;
        if(!(in>>tag>>a>>c>>ct>>p>>e>>pt)||tag!="INPUTREGION1"||a<0||a>1||c<0||c>1||p<0||p>1||e<0||e>1||ct<0||ct>INT32_MAX||pt<0||pt>INT32_MAX||(!c&&ct)||(!e&&pt)||(!p&&(e||pt)))throw std::runtime_error("Invalid saved input region");
        if(c)validate(uint32_t(ct));if(p&&e)validate(uint32_t(pt));
        return {a!=0,c!=0,p!=0,e!=0,uint32_t(ct),uint32_t(pt)};
    }
    void restore(const Stable& state){reset();state_=state;trigger_=state_.currentEnabled?identity():0;if(state_.active||state_.currentEnabled||state_.pendingDirty){request_=identity();readyKind_=2;}}
};
} //namespace mahoyo::input_region
