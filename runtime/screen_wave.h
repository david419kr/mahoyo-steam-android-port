#pragma once
#include <cstdint>
#include <optional>

namespace mahoyo::screen_wave {
struct Record {
    uint64_t id=0;
    int preset=9,fadeMs=0;
    uint64_t strengthElapsed=0;
    bool stopping=false,drawStarted=false;
    uint32_t phaseMs=0;
};
struct Sample {uint64_t id;int preset;float strength;uint32_t phaseMs;};

//Native kind1 Screen Raster; independent from per-layer kind2 wave jobs.
class Controller {
public:
    void request(int preset,int fadeMs);
    bool pending()const{return pending_.has_value();}
    void commit(uint64_t now);
    void sample(uint64_t now,bool running=true);
    std::optional<Sample> draw(uint64_t now,bool running=true);
    bool active()const{return record_.has_value();}
    bool finite()const;
    bool safe()const{return !pending()&&!finite();}
    uint64_t remainingWait()const;
    uint64_t generation()const{return nextId_;}
    std::optional<Record> record()const{return record_;}
    void restore(const std::optional<Record>& record,uint64_t generation,uint64_t now);
    void reset();
private:
    struct Request {int preset,fadeMs;};
    std::optional<Request> pending_;
    std::optional<Record> record_;
    uint64_t last_=0,drawLast_=0,nextId_=0;
    bool clock_=false;
    static void validate(const Record& record);
    static float strength(const Record& record);
};
}
