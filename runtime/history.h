#pragma once
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mahoyo::history {
constexpr size_t MaxRows=512,MaxVoicesPerRow=64,MaxTotalVoices=(MaxRows+1)*MaxVoicesPerRow;
constexpr uint32_t MaxLeadingBreaks=4096;
struct Voice {bool active=false;int backend=0,volume=0,pan=0,variant=0;std::string name;};
struct Row {
    bool known=true;uint32_t leadingBreaks=0;
    bool startsGroup=false,endsGroup=false,initialItalic=false,wait=false,page=false;
    int32_t style=0;std::vector<Voice> voices;
};
inline bool safeVoiceName(const std::string& name){
    return name.size()<=256&&name.find("..") == std::string::npos&&
        std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127||c=='/'||c=='\\'||c==':';});
}
inline void validate(const Voice& voice){
    if(voice.backend!=0||voice.volume<0||voice.volume>100||voice.pan<0||voice.pan>128||
       voice.variant<0||voice.variant>1||!safeVoiceName(voice.name))throw std::runtime_error("Invalid saved history voice");
}
inline int actorId(const std::string& name){
    std::string prefix=name.substr(0,3);for(auto& c:prefix)if(c>='a'&&c<='z')c=char(c-'a'+'A');
    const char* names[]={"ZZZ","A10","A11","A40","A30","B70","B10","B11","B50","B60","B40","B41","B30","B20","A20","A21","A50","B80","B81","B90"};
    const int ids[]={0,1,1,2,3,4,5,5,6,7,8,8,9,10,11,11,12,13,13,14};
    for(size_t i=0;i<20;++i)if(prefix==names[i])return ids[i];return 0;
}
inline int actorGain(int stored){if(stored<0||stored>128)throw std::runtime_error("Invalid history actor volume");return (stored*100)>>7;}
struct State {
    std::vector<Row> rows;
    std::vector<Voice> pendingVoices;
    size_t pendingRows=0;uint32_t pendingBreaks=0;
    void clear(){rows.clear();pendingVoices.clear();pendingRows=0;pendingBreaks=0;}
    void legacy(size_t count){clear();if(count>MaxRows)throw std::runtime_error("Invalid saved history count");rows.resize(count);for(auto& r:rows){r.known=false;r.endsGroup=true;}}
    void addBreak(){if(pendingBreaks==MaxLeadingBreaks)throw std::runtime_error("History leading-break budget exceeded");++pendingBreaks;}
    void append(Row row){
        row.leadingBreaks=pendingBreaks;row.voices=pendingVoices.empty()?std::vector<Voice>{Voice{}}:pendingVoices;rows.push_back(std::move(row));
        ++pendingRows;pendingBreaks=0;pendingVoices.clear();
        if(rows.back().endsGroup)pendingRows=0;
        if(rows.size()>MaxRows){rows.erase(rows.begin());pendingRows=std::min(pendingRows,rows.size());}
    }
    bool flush(){if(!pendingRows)return false;rows.back().endsGroup=true;pendingRows=0;return true;}
    void validate(size_t count)const{
        if(rows.size()!=count||count>MaxRows||pendingRows>count||pendingBreaks>MaxLeadingBreaks||pendingVoices.size()>MaxVoicesPerRow)throw std::runtime_error("Invalid saved history metadata count");
        size_t total=pendingVoices.size();
        for(const auto& row:rows){
            if(row.leadingBreaks>MaxLeadingBreaks||row.voices.size()>MaxVoicesPerRow||(!row.known&&(!row.voices.empty()||row.leadingBreaks||row.startsGroup||row.initialItalic||row.wait||row.page||row.style||!row.endsGroup)))throw std::runtime_error("Invalid saved history row");
            if(row.known&&(row.wait||row.page)&&!row.endsGroup)throw std::runtime_error("Invalid saved history flush boundary");
            total+=row.voices.size();for(const auto& v:row.voices)history::validate(v);
        }
        for(const auto& v:pendingVoices)history::validate(v);
        if(total>MaxTotalVoices)throw std::runtime_error("Saved history voice budget exceeded");
        for(size_t i=rows.size()-pendingRows;i<rows.size();++i)if(!rows[i].known||rows[i].endsGroup)throw std::runtime_error("Invalid saved pending history boundary");
    }
};
inline void writeVoice(std::ostream& out,const Voice& v){out<<v.active<<' '<<v.backend<<' '<<v.volume<<' '<<v.pan<<' '<<v.variant<<' '<<std::quoted(v.name)<<'\n';}
inline bool readBool(std::istream& in){int value;if(!(in>>value)||(value!=0&&value!=1))throw std::runtime_error("Invalid saved history boolean");return value!=0;}
inline Voice readVoice(std::istream& in){Voice v;v.active=readBool(in);if(!(in>>v.backend>>v.volume>>v.pan>>v.variant>>std::quoted(v.name)))throw std::runtime_error("Invalid saved history voice");validate(v);return v;}
inline void write(std::ostream& out,const State& state,size_t count){
    state.validate(count);out<<"HISTORYMETA1 "<<count<<' '<<state.pendingRows<<' '<<state.pendingBreaks<<'\n';
    for(const auto& r:state.rows){out<<r.known<<' '<<r.leadingBreaks<<' '<<r.startsGroup<<' '<<r.endsGroup<<' '<<r.initialItalic<<' '<<r.wait<<' '<<r.page<<' '<<r.style<<' '<<r.voices.size()<<'\n';for(const auto& v:r.voices)writeVoice(out,v);}
    out<<"PENDINGVOICE1 "<<state.pendingVoices.size()<<'\n';for(const auto& v:state.pendingVoices)writeVoice(out,v);
}
inline State read(std::istream& in,size_t count){
    State state;std::string tag;size_t rows;
    if(!(in>>tag>>rows>>state.pendingRows>>state.pendingBreaks)||tag!="HISTORYMETA1"||rows!=count||rows>MaxRows)throw std::runtime_error("Invalid saved history metadata footer");
    size_t total=0;
    for(size_t i=0;i<rows;++i){Row r;r.known=readBool(in);if(!(in>>r.leadingBreaks))throw std::runtime_error("Invalid saved history breaks");r.startsGroup=readBool(in);r.endsGroup=readBool(in);r.initialItalic=readBool(in);r.wait=readBool(in);r.page=readBool(in);size_t voices;if(!(in>>r.style>>voices)||voices>MaxVoicesPerRow||(total+=voices)>MaxTotalVoices)throw std::runtime_error("Invalid saved history voices count");for(size_t j=0;j<voices;++j)r.voices.push_back(readVoice(in));state.rows.push_back(std::move(r));}
    if(!(in>>tag>>rows)||tag!="PENDINGVOICE1"||rows>MaxVoicesPerRow||(total+=rows)>MaxTotalVoices)throw std::runtime_error("Invalid saved pending voice footer");
    for(size_t i=0;i<rows;++i)state.pendingVoices.push_back(readVoice(in));state.validate(count);return state;
}
} // namespace mahoyo::history
