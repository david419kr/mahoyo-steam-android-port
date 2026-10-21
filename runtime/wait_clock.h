#pragma once
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace mahoyo::wait_clock {
//Native112 requests a reset of the manager's cached scaled-time baseline.
//The port persists a relative age, not an OS timestamp or a GPU handle.
class Clock {
    uint64_t age_=0,anchor_=0;
    bool pending_=false;
public:
    static constexpr uint64_t MaxAge=uint64_t(INT64_MAX);
    uint64_t age(uint64_t now)const{
        if(now<anchor_||now-anchor_>MaxAge-age_)throw std::runtime_error("Wait baseline clock overflow or reversal");
        return age_+(now-anchor_);
    }
    void request(){pending_=true;}
    bool pending()const{return pending_;}
    void clearRequest(){pending_=false;}
    void restore(uint64_t age,bool pending,uint64_t now){
        if(age>MaxAge)throw std::runtime_error("Invalid saved wait baseline age");
        age_=age;anchor_=now;pending_=pending;
    }
    //The snapshot predates dependency completion. A late acknowledgement
    //must not move the baseline forward to its arrival time.
    void prepared(uint64_t capturedAge,uint64_t now){
        const auto current=age(now);
        if(capturedAge>current)throw std::runtime_error("Invalid wait baseline capture");
        if(pending_){age_=current-capturedAge;anchor_=now;pending_=false;}
    }
    static uint64_t remaining(int32_t duration,int32_t mode,uint64_t capturedAge){
        const auto requested=uint64_t(duration>0?duration:0);
        return mode==1?(capturedAge>=requested?0:requested-capturedAge):requested;
    }
    void write(std::ostream& out,uint64_t now)const{out<<"WAITBASELINE1 "<<pending_<<' '<<age(now)<<'\n';}
    static Clock read(std::istream& input,uint64_t now){
        std::string tag,pending,age;
        if(!(input>>tag>>pending>>age)||tag!="WAITBASELINE1"||(pending!="0"&&pending!="1")||age.empty())throw std::runtime_error("Invalid saved wait baseline footer");
        uint64_t value=0;
        for(char c:age){if(c<'0'||c>'9'||value>(MaxAge-uint64_t(c-'0'))/10)throw std::runtime_error("Invalid saved wait baseline age");value=value*10+uint64_t(c-'0');}
        Clock result;result.restore(value,pending=="1",now);return result;
    }
};
} //namespace mahoyo::wait_clock
