#include "shared_state.h"
#include "save_store.h"
#include <algorithm>
#include <string_view>

namespace mahoyo::shared {
namespace {
constexpr std::string_view Tag="WOHSHARED1";
constexpr size_t MaxVariables=4096,MaxFlags=16384,HeaderBytes=26,MaxFileBytes=40*1024;

void validate(const State& state) {
    if(state.generation==0)throw save::Error("Shared generation must be positive");
    if(state.variables.empty()||state.variables.size()>MaxVariables||state.flags.empty()||state.flags.size()>MaxFlags)
        throw save::Error("Invalid shared state counts");
    if(std::any_of(state.flags.begin(),state.flags.end(),[](uint8_t flag){return flag>1;}))
        throw save::Error("Invalid shared flag value");
}

void put(std::vector<uint8_t>& bytes,uint64_t value,unsigned count) {
    for(unsigned i=0;i<count;++i)bytes.push_back(uint8_t(value>>(8*i)));
}

uint64_t get(const std::vector<uint8_t>& bytes,size_t& at,unsigned count) {
    if(at>bytes.size()||count>bytes.size()-at)throw save::Error("Truncated shared state field");
    uint64_t value=0;
    for(unsigned i=0;i<count;++i)value|=uint64_t(bytes[at++])<<(8*i);
    return value;
}
} // namespace

void write(const std::filesystem::path& path,const State& state) {
    validate(state);
    save::Document document;
    auto& bytes=document.hostExtra;
    bytes.reserve(HeaderBytes+state.variables.size()*4+state.flags.size());
    bytes.insert(bytes.end(),Tag.begin(),Tag.end());
    put(bytes,state.generation,8);
    put(bytes,state.variables.size(),4);put(bytes,state.flags.size(),4);
    for(const auto value:state.variables)put(bytes,static_cast<uint32_t>(value),4);
    bytes.insert(bytes.end(),state.flags.begin(),state.flags.end());
    save::writeAtomic(path,document);
}

std::optional<State> read(const std::filesystem::path& path) {
    if(path.empty())throw save::Error("Invalid shared state filename");
    std::error_code error;
    const bool exists=std::filesystem::exists(path,error);
    if(error)throw save::Error("Cannot inspect shared state file");
    if(!exists)return std::nullopt;
    if(std::filesystem::file_size(path)>MaxFileBytes)throw save::Error("Shared state file exceeds size limit");
    const auto document=save::read(path);
    if(!document.vm.empty()||!document.layers.empty()||!document.audio.empty()||!document.history.empty()
       ||!document.text.empty()||document.textLine!=0||document.hidden)
        throw save::Error("Shared state container has unrelated fields");
    const auto& bytes=document.hostExtra;
    if(bytes.size()<HeaderBytes||!std::equal(Tag.begin(),Tag.end(),bytes.begin()))
        throw save::Error("Invalid shared state tag or header");
    size_t at=Tag.size();State result;
    result.generation=get(bytes,at,8);
    const auto variables=get(bytes,at,4),flags=get(bytes,at,4);
    if(variables==0||variables>MaxVariables||flags==0||flags>MaxFlags)
        throw save::Error("Invalid shared state counts");
    if(bytes.size()!=HeaderBytes+variables*4+flags)throw save::Error("Shared state length mismatch");
    result.variables.reserve(static_cast<size_t>(variables));
    for(uint64_t i=0;i<variables;++i) {
        const uint32_t bits=static_cast<uint32_t>(get(bytes,at,4));
        const int64_t value=bits<=0x7fffffffu?int64_t(bits):int64_t(bits)-0x100000000ll;
        result.variables.push_back(static_cast<int32_t>(value));
    }
    result.flags.assign(bytes.begin()+at,bytes.end());
    validate(result);
    return result;
}
} // namespace mahoyo::shared
