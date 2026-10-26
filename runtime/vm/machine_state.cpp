#include "machine.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace mahoyo::vm {
namespace {
uint32_t checksum(const std::vector<uint8_t>& bytes,size_t count){uint32_t h=2166136261u;for(size_t n=0;n<count;++n){h^=bytes[n];h*=16777619u;}return h;}
struct Writer {
    std::vector<uint8_t> bytes;
    void u32(uint32_t v){for(unsigned n=0;n<4;++n)bytes.push_back(uint8_t(v>>(n*8)));}
    void blob(const std::vector<uint8_t>& v){u32(uint32_t(v.size()));bytes.insert(bytes.end(),v.begin(),v.end());}
    void text(const std::string& v){u32(uint32_t(v.size()));bytes.insert(bytes.end(),v.begin(),v.end());}
    void ints(const std::vector<int32_t>& v){u32(uint32_t(v.size()));for(auto i:v)u32(uint32_t(i));}
};
struct Reader {
    const std::vector<uint8_t>& bytes;size_t at=8;
    uint32_t u32(){if(at>bytes.size()||bytes.size()-at<4)throw Error("Truncated VM save state");auto v=uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;at+=4;return v;}
    std::vector<uint8_t> blob(size_t max){const auto n=u32();if(n>max||at>bytes.size()||n>bytes.size()-at)throw Error("Invalid VM save blob length");std::vector<uint8_t> v(bytes.begin()+at,bytes.begin()+at+n);at+=n;return v;}
    std::string text(size_t max){auto v=blob(max);return std::string(v.begin(),v.end());}
    std::vector<int32_t> ints(size_t max){const auto n=u32();if(n>max)throw Error("Invalid VM save vector length");std::vector<int32_t> v;v.reserve(n);for(uint32_t i=0;i<n;++i)v.push_back(int32_t(u32()));return v;}
};
}
bool Machine::sameExecutionPoint(const Machine& other) const {
    if(instructions_!=other.instructions_||moduleName()!=other.moduleName()||modulePc()!=other.modulePc()||frames_.size()!=other.frames_.size()||localTop_!=other.localTop_||stack_.size()!=other.stack_.size()||scopes_.size()!=other.scopes_.size())return false;
    for(size_t i=0;i<frames_.size();++i){const auto& a=frames_[i];const auto& b=other.frames_[i];const auto& am=modules_.at(a.module);const auto& bm=other.modules_.at(b.module);
        if(am.name!=bm.name||a.pc-am.code!=b.pc-bm.code||a.localTop!=b.localTop||a.unload!=b.unload)return false;}
    return true;
}
std::vector<uint8_t> Machine::saveState() const {
    Writer w;for(char c:std::string("WOHVM002"))w.bytes.push_back(uint8_t(c));
    w.u32(pc_);w.u32(currentModule_);w.u32(localTop_);w.u32(sourceLine_);w.u32(uint32_t(flow_));
    w.u32(uint32_t(instructions_));w.u32(uint32_t(instructions_>>32));w.u32(uint32_t(returnValue_));w.u32(returnValueValid_);
    w.text(functionName_);w.text(debugMessage_);
    w.blob(arena_);w.blob(locals_);w.blob(strings_);w.blob(scratch_);
    w.ints(stack_);w.ints(stackCells_);w.ints(variables_);w.ints(globals_);w.blob(flags_);w.blob(globalFlags_);
    w.u32(uint32_t(modules_.size()));for(const auto& m:modules_){w.text(m.name);w.u32(m.begin);w.u32(m.end);w.u32(m.code);}
    w.u32(uint32_t(frames_.size()));for(const auto& f:frames_){w.u32(f.pc);w.u32(f.module);w.u32(f.localTop);w.u32(f.unload);w.u32(uint32_t(f.moduleCount));}
    w.u32(uint32_t(scopes_.size()));for(const auto& s:scopes_){w.u32(uint32_t(s.stackSize));w.u32(s.localTop);}
    w.u32(uint32_t(exports_.size()));for(const auto& e:exports_){w.text(e.first);w.u32(e.second.module);w.u32(e.second.offset);}
    w.u32(checksum(w.bytes,w.bytes.size()));return w.bytes;
}
void Machine::loadState(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<12||bytes.size()>18*1024*1024||std::memcmp(bytes.data(),"WOHVM002",8))throw Error("Invalid VM save signature");
    Reader tail{bytes,bytes.size()-4};if(tail.u32()!=checksum(bytes,bytes.size()-4))throw Error("VM save checksum mismatch");
    Reader r{bytes};Machine t(host_);
    t.pc_=r.u32();t.currentModule_=r.u32();t.localTop_=r.u32();t.sourceLine_=r.u32();
    const auto flow=r.u32();if(flow>2)throw Error("Invalid VM save flow");t.flow_=Flow(flow);
    t.instructions_=r.u32();t.instructions_|=uint64_t(r.u32())<<32;t.returnValue_=int32_t(r.u32());t.returnValueValid_=r.u32()!=0;
    t.functionName_=r.text(4096);t.debugMessage_=r.text(1024*1024);
    t.arena_=r.blob(16*1024*1024);t.locals_=r.blob(65536);t.strings_=r.blob(4096);t.scratch_=r.blob(65536);
    t.stack_=r.ints(1024);t.stackCells_=r.ints(1024);t.variables_=r.ints(1280);t.globals_=r.ints(4096);t.flags_=r.blob(2048);t.globalFlags_=r.blob(16384);
    if(t.locals_.size()!=65536||t.strings_.size()!=4096||t.scratch_.size()!=65536||t.stackCells_.size()!=1024||t.variables_.size()!=1280||t.globals_.size()!=4096||t.flags_.size()!=2048||t.globalFlags_.size()!=16384||t.localTop_>65536)throw Error("Invalid VM save memory geometry");
    auto count=r.u32();if(count>64)throw Error("Invalid VM saved module count");
    for(uint32_t i=0;i<count;++i){Module m;m.name=r.text(256);m.begin=r.u32();m.end=r.u32();m.code=r.u32();
        if(m.begin>=m.code||m.code>=m.end||m.end>t.arena_.size()||(i&&m.begin<t.modules_.back().end))throw Error("Invalid VM saved module bounds");t.modules_.push_back(m);}
    if(t.modules_.empty()||t.currentModule_>=count)throw Error("Invalid VM saved current module");
    if(t.pc_<t.modules_[t.currentModule_].code||t.pc_>t.modules_[t.currentModule_].end)throw Error("Invalid VM saved PC");
    count=r.u32();if(count>5461)throw Error("Invalid VM saved frame count");
    for(uint32_t i=0;i<count;++i){Frame f;f.pc=r.u32();f.module=r.u32();f.localTop=r.u32();f.unload=r.u32()!=0;f.moduleCount=r.u32();
        if(f.module>=t.modules_.size()||f.moduleCount>t.modules_.size()||f.localTop>65524||f.pc<t.modules_[f.module].code||f.pc>t.modules_[f.module].end)throw Error("Invalid VM saved frame");t.frames_.push_back(f);}
    count=r.u32();if(count>1024)throw Error("Invalid VM saved scope count");
    for(uint32_t i=0;i<count;++i){Scope s;s.stackSize=r.u32();s.localTop=r.u32();if(s.stackSize>1024||s.localTop>65536)throw Error("Invalid VM saved scope");t.scopes_.push_back(s);}
    count=r.u32();if(count>65536)throw Error("Invalid VM saved export count");
    for(uint32_t i=0;i<count;++i){auto name=r.text(4096);Export e;e.module=r.u32();e.offset=r.u32();if(e.module>=t.modules_.size()||e.offset>=t.modules_[e.module].end-t.modules_[e.module].code)throw Error("Invalid VM saved export");t.exports_[name]=e;}
    if(r.at!=bytes.size()-4)throw Error("Unexpected VM save trailing data");
    // Commit only after every field validates. Failed loads leave the current game untouched.
#define SWAP_STATE(name) std::swap(name,t.name)
    SWAP_STATE(arena_);SWAP_STATE(locals_);SWAP_STATE(strings_);SWAP_STATE(scratch_);SWAP_STATE(stack_);SWAP_STATE(stackCells_);SWAP_STATE(variables_);SWAP_STATE(globals_);SWAP_STATE(flags_);SWAP_STATE(globalFlags_);SWAP_STATE(modules_);SWAP_STATE(frames_);SWAP_STATE(scopes_);SWAP_STATE(exports_);SWAP_STATE(pc_);SWAP_STATE(currentModule_);SWAP_STATE(localTop_);SWAP_STATE(sourceLine_);SWAP_STATE(instructions_);SWAP_STATE(returnValue_);SWAP_STATE(returnValueValid_);SWAP_STATE(functionName_);SWAP_STATE(debugMessage_);SWAP_STATE(flow_);
#undef SWAP_STATE
}
} // namespace mahoyo::vm
