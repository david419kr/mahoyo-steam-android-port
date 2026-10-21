#include "machine.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

namespace mahoyo::vm {
namespace {
constexpr size_t MaxArena = 16 * 1024 * 1024;
std::string moduleFile(std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    if (name.find('/') != std::string::npos || name.find("..") != std::string::npos)
        throw Error("Invalid CHS module name");
    if (name.size() < 4 || name.substr(name.size()-4) != ".chs") name += ".chs";
    return name;
}
}
Machine::Machine(Host& host): host_(host), locals_(65536), strings_(4096),
    scratch_(65536), stackCells_(1024), variables_(1280), globals_(4096), flags_(2048), globalFlags_(16384) {}
uint32_t Machine::word(const std::vector<uint8_t>& bytes, size_t at) {
    if (at > bytes.size() || bytes.size()-at < 4) throw Error("Truncated CHS dword");
    return uint32_t(bytes[at]) | uint32_t(bytes[at+1]) << 8 |
        uint32_t(bytes[at+2]) << 16 | uint32_t(bytes[at+3]) << 24;
}
uint32_t Machine::load(const std::string& requested, bool references) {
    if (modules_.size() >= 64) throw Error("CHS module limit exceeded");
    const auto name = moduleFile(requested);
    auto bytes = host_.loadModule(name);
    size_t signature = 0;
    for (const auto* s : {"HunexCompiledScriptVer1.00", "BurikoCompiledScriptVer1.00"}) {
        const auto n = std::strlen(s) + 1;
        if (bytes.size() >= n && std::equal(s, s+std::strlen(s), bytes.begin()) &&
            bytes[n-1] == 0) { signature=n; break; }
    }
    if (!signature) throw Error("Invalid CHS signature: " + name);
    const uint32_t headerLength = word(bytes, signature);
    const size_t code = signature + size_t(headerLength);
    if (headerLength < 12 || code >= bytes.size()) throw Error("Invalid CHS header length: " + name);
    const size_t begin = (arena_.size()+15) & ~size_t(15);
    if (begin > MaxArena || bytes.size() > MaxArena-begin) throw Error("CHS code arena limit exceeded");
    arena_.resize(begin);
    arena_.insert(arena_.end(), bytes.begin(), bytes.end());
    const uint32_t index = uint32_t(modules_.size());
    modules_.push_back({name, uint32_t(begin), uint32_t(begin+bytes.size()), uint32_t(begin+code)});
    size_t at = signature+4;
    const auto refs = word(bytes, at); at += 4;
    auto readName = [&]() {
        const auto first = at;
        while (at < code && bytes[at]) ++at;
        if (at >= code) throw Error("Unterminated CHS module/export name");
        std::string result(bytes.begin()+first, bytes.begin()+at); ++at; return result;
    };
    for (uint32_t n=0; n<refs; ++n) {
        const auto reference = readName();
        if (references) load(reference, false);
    }
    const auto count = word(bytes, at); at+=4;
    if (count > 65536) throw Error("Invalid CHS export count");
    for (uint32_t n=0; n<count; ++n) {
        const auto symbol = readName(); const auto offset=word(bytes,at); at+=4;
        if (offset >= bytes.size()-code) throw Error("CHS export outside module");
        exports_[symbol] = {index,offset};
    }
    if (at > code) throw Error("CHS metadata overlaps code");
    return index;
}
void Machine::start(const std::string& module, uint32_t entryOffset) {
    arena_.clear(); modules_.clear(); exports_.clear(); stack_.clear(); frames_.clear(); scopes_.clear();
    std::fill(locals_.begin(),locals_.end(),0); std::fill(strings_.begin(),strings_.end(),0);
    std::fill(variables_.begin(),variables_.end(),0); std::fill(flags_.begin(),flags_.end(),0);
    std::fill(stackCells_.begin(),stackCells_.end(),0);
    localTop_=0; sourceLine_=0; instructions_=0; functionName_.clear(); debugMessage_.clear();
    currentModule_=load(module,true); pc_=modules_[currentModule_].code+entryOffset;
    returnValueValid_=false; flow_=Flow::Continue; jump(pc_);
}
void Machine::push(int32_t value) {
    if (stack_.size()>=1024) throw Error("VM evaluation stack overflow");
    stackCells_[stack_.size()] = value;
    stack_.push_back(value);
}
void Machine::rebindModuleStrings(const std::map<std::string,size_t>& prefixes) {
    struct Replacement {uint32_t begin;std::vector<uint8_t> bytes;};
    std::vector<Replacement> changes;
    for(const auto& module:modules_){
        auto bytes=host_.loadModule(module.name);const size_t length=module.end-module.begin;
        if(bytes.size()!=length)throw Error("Locale module size differs: "+module.name);
        if(std::equal(bytes.begin(),bytes.end(),arena_.begin()+module.begin))continue;
        const auto found=prefixes.find(module.name);
        if(found==prefixes.end()||found->second<module.code-module.begin||found->second>length||
           !std::equal(bytes.begin(),bytes.begin()+found->second,arena_.begin()+module.begin))
            throw Error("Locale variant changes executable code: "+module.name);
        changes.push_back({module.begin,std::move(bytes)});
    }
    // Validate every loaded module before touching the live arena/PC/stack/frames.
    for(const auto& change:changes)std::copy(change.bytes.begin(),change.bytes.end(),arena_.begin()+change.begin);
}
int32_t Machine::pop() { if(stack_.empty()) throw Error("VM evaluation stack underflow"); auto v=stack_.back(); stack_.pop_back(); return v; }
int32_t Machine::peek(size_t depth) const { if(depth>=stack_.size()) throw Error("VM peek underflow"); return stack_[stack_.size()-1-depth]; }
uint32_t Machine::readOperand() { auto v=word(arena_,pc_); pc_+=4; if(pc_>modules_.at(currentModule_).end) throw Error("Operand outside current module"); return v; }
uint32_t Machine::modulePc() const { return modules_.empty()?0:pc_-modules_.at(currentModule_).code; }
const std::string& Machine::moduleName() const { static const std::string empty; return modules_.empty()?empty:modules_.at(currentModule_).name; }
void Machine::jump(uint32_t address) {
    const auto& m=modules_.at(currentModule_);
    if(address<m.code || address>=m.end) throw Error("VM jump outside current module");
    pc_=address;
}
void Machine::validateExternalTarget(uint32_t address) const {
    if(modules_.empty())throw Error("External jump without VM module");
    const auto& m=modules_.at(currentModule_);
    if(address<m.code||address>=m.end||m.end-address<4||(address-m.code)%4)throw Error("External jump outside current code range");
    uint32_t at=m.code;
    while(at<address){
        const auto op=word(arena_,at);unsigned words=1;
        if(op<=3||op==8||op==9||op==10||op==0x19)words=2;
        else if(op==0x7b)words=4;
        else if(op==0x7f)words=3;
        if(op>0xffff||uint64_t(at)+words*4>m.end)throw Error("External jump crosses invalid instruction span");
        at+=words*4;
    }
    if(at!=address||word(arena_,address)>0xffff)throw Error("External jump is not a decoded instruction boundary");
}
uint32_t Machine::externalJump(uint32_t address) {
    validateExternalTarget(address);const auto previous=pc_;pc_=address;flow_=Flow::Continue;return previous;
}
const std::vector<uint8_t>& Machine::memory(uint32_t address,size_t bytes,size_t& offset) const {
    const auto tag=address>>28; offset=address&0xfffffff;
    const std::vector<uint8_t>* m=nullptr;
    switch(tag) {case 0:m=&arena_;break;case 1:m=&locals_;break;case 2:m=&strings_;break;case 4:m=&scratch_;break;
    default:throw Error("Unsupported VM pointer address space "+std::to_string(tag));}
    if(offset>m->size() || bytes>m->size()-offset) throw Error("VM memory range exceeded");
    return *m;
}
std::vector<uint8_t>& Machine::memory(uint32_t address,size_t bytes,size_t& offset) {
    return const_cast<std::vector<uint8_t>&>(static_cast<const Machine*>(this)->memory(address,bytes,offset));
}
uint32_t Machine::readMemory(uint32_t address,unsigned width) const {
    if(width>2) throw Error("Invalid VM load width"); size_t offset; const auto& m=memory(address,1u<<width,offset);
    if(width==0)return uint32_t(int32_t(int8_t(m[offset])));
    if(width==1)return uint32_t(int32_t(int16_t(uint16_t(m[offset])|uint16_t(m[offset+1])<<8)));
    return word(m,offset);
}
void Machine::writeMemory(uint32_t address,uint32_t value,unsigned width) {
    if(width>2) throw Error("Invalid VM store width"); size_t offset; auto& m=memory(address,1u<<width,offset);
    for(unsigned n=0;n<(1u<<width);++n)m[offset+n]=uint8_t(value>>(n*8));
}
std::string Machine::stringAt(uint32_t address) const {
    if(!address)return {}; size_t at; const auto& m=memory(address,1,at); auto end=at;
    while(end<m.size() && m[end] && end-at<1024*1024)++end;
    if(end==m.size()||end-at>=1024*1024)throw Error("Unterminated VM string");
    return std::string(m.begin()+at,m.begin()+end);
}
std::string Machine::popString(){return stringAt(uint32_t(pop()));}
int32_t Machine::variable(size_t index,bool global)const {const auto& v=global?globals_:variables_; if(index>=v.size())throw Error("VM variable index out of range");return v[index];}
void Machine::setVariable(size_t index,int32_t value,bool global){auto& v=global?globals_:variables_;if(index>=v.size())throw Error("VM variable index out of range");v[index]=value;host_.variableChanged(index,value,global);}
bool Machine::flag(size_t index,bool global) const {const auto& flags=global?globalFlags_:flags_;if(index>=flags.size())throw Error("VM flag index out of range");return flags[index]!=0;}
void Machine::setFlag(size_t index,bool value,bool global){auto& flags=global?globalFlags_:flags_;if(index>=flags.size())throw Error("VM flag index out of range");flags[index]=value?1:0;}
void Machine::restoreShared(const std::vector<int32_t>& variables,const std::vector<uint8_t>& flags){
    if(variables.empty()||variables.size()>4096||flags.empty()||flags.size()>16384||std::any_of(flags.begin(),flags.end(),[](uint8_t value){return value>1;}))throw Error("Invalid shared manager state");
    std::vector<int32_t> nextVariables(4096);std::vector<uint8_t> nextFlags(16384);
    std::copy(variables.begin(),variables.end(),nextVariables.begin());std::copy(flags.begin(),flags.end(),nextFlags.begin());
    globals_.swap(nextVariables);globalFlags_.swap(nextFlags);
}
void Machine::resume(){if(flow_==Flow::Yield)flow_=Flow::Continue;}
void Machine::callExport(const std::string& name) {
    auto found=exports_.find(name); if(found==exports_.end()) throw Error("Unknown CHS export: "+name);
    pushFrame(false);
    currentModule_=found->second.module; pc_=modules_[currentModule_].code+found->second.offset;
}
void Machine::pushFrame(bool unload) {
    if(localTop_>locals_.size()-12)throw Error("VM call buffer overflow");
    frames_.push_back({pc_,currentModule_,localTop_,unload,modules_.size()});
    writeMemory(0x10000000u|localTop_,pc_,2);
    writeMemory(0x10000000u|(localTop_+4),currentModule_,2);
    writeMemory(0x10000000u|(localTop_+8),unload?1:0,2);localTop_+=12;
}
void Machine::returnFromCall(){
    if(frames_.empty()){flow_=Flow::Halt;return;}const auto f=frames_.back();frames_.pop_back();
    pc_=f.pc;currentModule_=f.module;localTop_=f.localTop;
    if(f.unload){
        modules_.resize(f.moduleCount);arena_.resize(modules_.back().end);
        for(auto it=exports_.begin();it!=exports_.end();)if(it->second.module>=modules_.size())it=exports_.erase(it);else++it;
        functionName_.clear();
    }
}
Flow Machine::execute(uint32_t op) {
    if(op>=0x20&&op<=0x2b){const auto b=uint32_t(pop()); if(op==0x28){push(int32_t(~b));return Flow::Continue;}const auto a=uint32_t(pop());uint32_t v=0;
        switch(op){case 0x20:v=a+b;break;case 0x21:v=a-b;break;case 0x22:v=a*b;break;
        case 0x23:case 0x24:if(!b)throw Error("VM division by zero");if(a==0x80000000&&b==0xffffffff)throw Error("VM signed division overflow");v=op==0x23?uint32_t(int32_t(a)/int32_t(b)):uint32_t(int32_t(a)%int32_t(b));break;
        case 0x25:v=a&b;break;case 0x26:v=a|b;break;case 0x27:v=a^b;break;case 0x29:v=a<<(b&31);break;case 0x2a:v=a>>(b&31);break;case 0x2b:v=uint32_t(int32_t(a)>>(b&31));break;}push(int32_t(v));return Flow::Continue;}
    if((op>=0x30&&op<=0x35)||op==0x38||op==0x39||op==0x3a){const auto b=pop();if(op==0x3a){push(!b);return Flow::Continue;}const auto a=pop();bool v=false;
        switch(op){case 0x30:v=a==b;break;case 0x31:v=a!=b;break;case 0x32:v=a<=b;break;case 0x33:v=a>=b;break;case 0x34:v=a<b;break;case 0x35:v=a>b;break;case 0x38:v=a&&b;break;case 0x39:v=a||b;break;}push(v);return Flow::Continue;}
    switch(op){
    case 0:push(int32_t(readOperand()));break;
    case 1:case 3:push(int32_t(readOperand()+modules_.at(currentModule_).code));break;
    case 2:{const auto n=readOperand();if(n>localTop_)throw Error("VM local pointer below frame");push(int32_t(0x10000000u|(localTop_-n)));break;}
    case 8:{const auto width=readOperand();push(int32_t(readMemory(uint32_t(pop()),width)));break;}
    case 9:{const auto width=readOperand();const auto v=uint32_t(pop());const auto a=uint32_t(pop());writeMemory(a,v,width);break;}
    case 10:{const auto width=readOperand();const auto a=uint32_t(pop());const auto v=uint32_t(pop());writeMemory(a,v,width);break;}
    case 0x10:push(int32_t(localTop_));break;
    case 0x11:{const auto n=uint32_t(pop());if(n>locals_.size())throw Error("VM local buffer overflow");localTop_=n;break;}
    case 0x18:jump(uint32_t(pop()));break;
    case 0x19:{const auto type=readOperand();const auto target=uint32_t(pop());const auto condition=pop();if(type?condition!=0:condition==0)jump(target);break;}
    case 0x1a:{const auto target=uint32_t(pop());pushFrame(false);jump(target);break;}
    case 0x1b:returnFromCall();return flow_;
    case 0x1c:callExport(popString());break;
    case 0x1d:returnValue_=pop();returnValueValid_=true;break;
    case 0x1e:pop();if(scopes_.size()>=1024)throw Error("VM scope overflow");scopes_.push_back({stack_.size(),localTop_});break;
    case 0x1f:if(scopes_.empty())throw Error("VM scope underflow");stack_.assign(stackCells_.begin(),stackCells_.begin()+scopes_.back().stackSize);scopes_.pop_back();if(returnValueValid_){push(returnValue_);returnValueValid_=false;}break;
    case 0x3f:{const auto n=pop();if(n<0||size_t(n)>stack_.size())throw Error("VM reverse range");std::reverse(stack_.end()-n,stack_.end());std::copy(stack_.end()-n,stack_.end(),stackCells_.begin()+stack_.size()-n);break;}
    case 0x40:{const auto n=uint32_t(pop());const auto source=uint32_t(pop());const auto dest=uint32_t(pop());size_t a,b;const auto& s=memory(source,n,a);auto& d=memory(dest,n,b);std::memmove(d.data()+b,s.data()+a,n);break;}
    case 0x80:{const auto dest=uint32_t(pop());const auto source=uint32_t(pop());const auto n=uint32_t(pop());size_t a,b;const auto& s=memory(source,n,a);auto& d=memory(dest,n,b);std::memmove(d.data()+b,s.data()+a,n);break;}
    case 0x81:case 0x82:{const auto dest=uint32_t(pop());const auto n=uint32_t(pop());const auto value=op==0x82?uint8_t(pop()):uint8_t(0);size_t at;auto& m=memory(dest,n,at);std::fill(m.begin()+at,m.begin()+at+n,value);break;}
    case 0x83:{const auto a=uint32_t(pop());const auto b=uint32_t(pop());const auto n=uint32_t(pop());size_t x,y;const auto& m=memory(a,n,x);const auto& k=memory(b,n,y);push(std::equal(m.begin()+x,m.begin()+x+n,k.begin()+y));break;}
    case 0x90:{const auto dest=uint32_t(pop());const auto value=popString();size_t at;auto& m=memory(dest,value.size()+1,at);std::copy(value.begin(),value.end(),m.begin()+at);m[at+value.size()]=0;break;}
    case 0x94:push(int32_t(popString().size()));break;
    case 0x95:{const auto a=popString();const auto b=popString();push(a==b);break;}
    case 0x48:{const auto value=popString();const auto dest=uint32_t(pop());size_t at;auto& m=memory(dest,value.size()+1,at);std::copy(value.begin(),value.end(),m.begin()+at);m[at+value.size()]=0;break;}
    case 0x7b:readOperand();readOperand();readOperand();break;
    case 0x7f:readOperand();sourceLine_=readOperand();break;
    case 0xe0:case 0xe2:{const auto index=uint32_t(pop());const auto value=pop();setVariable(index,value,op==0xe2);break;}
    case 0xe1:case 0xe3:{const auto index=uint32_t(pop());push(variable(index,op==0xe3));break;}
    case 0xe4:case 0xe6:{const auto index=uint32_t(pop());const auto value=pop();setFlag(index,value!=0,op==0xe6);break;}
    case 0xe5:case 0xe7:{const auto index=uint32_t(pop());push(flag(index,op==0xe7));break;}
    case 0xe8:{const auto n=uint32_t(pop());if(n>=16)throw Error("VM string slot index");push(int32_t(0x20000000u|(n<<8)));break;}
    case 0xe9:{auto kind=pop();auto index=uint32_t(pop());popString();const auto value=pop();if(kind==2){kind=index>=600&&index<=700?1:0;if(kind)index-=600;}if(kind<0||kind>1)throw Error("VM variable kind");setVariable(index,value,kind!=0);break;}
    case 0xea:case 0xeb:{auto kind=pop();auto index=uint32_t(pop());if(kind==2){kind=index>=600&&index<=700?1:0;if(kind)index-=600;}if(kind<0||kind>1)throw Error("VM variable kind");if(op==0xea)push(variable(index,kind!=0));else{const auto v=uint32_t(pop())+uint32_t(variable(index,kind!=0));setVariable(index,int32_t(v),kind!=0);}break;}
    case 0xf0:{const auto name=popString();pushFrame(true);const auto index=load(name,false);currentModule_=index;pc_=modules_[index].code;break;}
    case 0xf3:{const auto name=popString();if(currentModule_+1==modules_.size()){
        arena_.resize(modules_.back().begin);modules_.pop_back();
        for(auto it=exports_.begin();it!=exports_.end();)if(it->second.module>=modules_.size())it=exports_.erase(it);else++it;
        }currentModule_=load(name,false);pc_=modules_[currentModule_].code;break;}
    case 0xf4:return Flow::Halt;
    case 0xf6:functionName_=popString();break;
    case 0xf7:push(popString()==functionName_);break;
    case 0xf8:debugMessage_+=std::to_string(pop());break;
    case 0xf9:debugMessage_+=popString();break;
    case 0xfe:popString();pop();break;
    default:return host_.invoke(op,*this);
    }
    return Flow::Continue;
}
Flow Machine::run(size_t budget) {
    if(flow_!=Flow::Continue)return flow_;
    for(size_t i=0;i<budget;++i){const auto at=pc_;const auto name=moduleName();const auto base=modules_.at(currentModule_).code;uint32_t op=0;
        try{op=readOperand();host_.trace(*this,op);++instructions_;flow_=execute(op);}
        catch(const std::exception& e){std::ostringstream text;text<<name<<" +0x"<<std::hex<<(at-base)<<" op 0x"<<op<<" line "<<std::dec<<sourceLine_<<": "<<e.what();flow_=Flow::Halt;throw Error(text.str());}
        if(flow_!=Flow::Continue)return flow_;
    }
    return Flow::Continue;
}
} // namespace mahoyo::vm
