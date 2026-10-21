#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

namespace mahoyo::noise {
struct Config {int selector=0,periodMs=50;bool operator==(const Config& other)const{return selector==other.selector&&periodMs==other.periodMs;}};
inline std::optional<Config> decode(int32_t raw){
    int ones=raw%10,tens=raw/10;
    if(!ones)return std::nullopt;
    if(ones!=1&&ones!=2)throw std::invalid_argument("Unsupported native noise selector "+std::to_string(ones));
    return Config{ones,tens==1?150:tens==2?100:50};
}
struct State {Config config;std::string token;};
//Configuration lifetime only. Native phase/due/UV advance belongs to the GL
//prepass, never to snapshot polling or this CPU controller.
class Controller {
public:
    bool reconcile(const std::map<int,Config>& desired){
        auto candidate=states_;bool changed=false;
        for(auto it=candidate.begin();it!=candidate.end();)if(!desired.count(it->first)){it=candidate.erase(it);changed=true;}else ++it;
        for(const auto& pair:desired){
            if(pair.first<0||pair.first>=64||(pair.second.selector!=1&&pair.second.selector!=2)||
               (pair.second.periodMs!=50&&pair.second.periodMs!=100&&pair.second.periodMs!=150))throw std::invalid_argument("Invalid native noise configuration");
            auto old=candidate.find(pair.first);
            if(old!=candidate.end()&&old->second.config==pair.second)continue;
            candidate.insert_or_assign(pair.first,State{pair.second,nextToken()});changed=true;
        }
        states_.swap(candidate);return changed;
    }
    void remove(int id){if(id<0)states_.clear();else states_.erase(id);}
    void reset(){states_.clear();}
    bool empty()const{return states_.empty();}
    bool contains(int id)const{return states_.count(id)!=0;}
    const State& at(int id)const{return states_.at(id);}
    const std::map<int,State>& states()const{return states_;}
    int minimumPeriod()const{int result=0;for(const auto& pair:states_)if(!result||pair.second.config.periodMs<result)result=pair.second.config.periodMs;return result;}
private:
    static std::string nextToken(){static const auto epoch=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());static std::atomic<uint64_t> serial{1};auto value=serial.fetch_add(1);if(!value||value==UINT64_MAX)throw std::overflow_error("Noise token exhausted");return "noise-"+std::to_string(epoch)+"-"+std::to_string(value);}
    std::map<int,State> states_;
};
}
