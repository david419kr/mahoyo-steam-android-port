#include "shared_state.h"
#include "save_store.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs=std::filesystem;
using mahoyo::shared::State;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Work> static void rejects(Work work,const char* message){
    bool caught=false;try{work();}catch(const std::exception&){caught=true;}check(caught,message);
}
static std::vector<uint8_t> bytes(const fs::path& path){
    std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};
}
static void writeBytes(const fs::path& path,const std::vector<uint8_t>& data){
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));
    if(!file)throw std::runtime_error("Cannot write synthetic shared fixture");
}
static void put32(std::vector<uint8_t>& data,size_t at,uint32_t value){
    for(unsigned i=0;i<4;++i)data.at(at+i)=uint8_t(value>>(8*i));
}
static void equal(const State& a,const State& b){
    check(a.generation==b.generation&&a.variables==b.variables&&a.flags==b.flags,"Shared state roundtrip differs");
}

int main(){
    const auto root=fs::temp_directory_path()/("mahoyo-shared-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    try{
        const auto path=root/"shared.whp",broken=root/"broken.whp",large=root/"large.whp";
        check(!mahoyo::shared::read(path),"Missing registry returns nullopt");
        check(!mahoyo::shared::read(root/"absent"/"shared.whp"),"Missing parent returns nullopt");
        rejects([&]{mahoyo::shared::read({});},"Empty read path rejected");
        State value{0x0123456789abcdefull,{std::numeric_limits<int32_t>::min(),-1,0,1,std::numeric_limits<int32_t>::max()},{0,1,1,0}};
        mahoyo::shared::write(path,value);equal(*mahoyo::shared::read(path),value);
        const auto original=bytes(path);
        const auto document=mahoyo::save::read(path);
        const auto& payload=document.hostExtra;
        check(std::string(payload.begin(),payload.begin()+10)=="WOHSHARED1","Public tag is exact");
        check(payload.size()==26+value.variables.size()*4+value.flags.size(),"Payload has exact bounded length");
        check(payload[10]==0xef&&payload[17]==0x01&&payload[18]==5&&payload[22]==4,"Generation and counts are little endian");
        check(payload[26]==0&&payload[29]==0x80&&payload[30]==0xff&&payload[33]==0xff,"Signed int limits preserve their bits");
        auto invalid=value;
        const auto preserves=[&](const State& bad){
            rejects([&]{mahoyo::shared::write(path,bad);},"Invalid overwrite rejected");
            check(bytes(path)==original,"Invalid overwrite preserves previous registry");
        };
        invalid.generation=0;preserves(invalid);
        rejects([&]{mahoyo::shared::write(root/"invalid"/"shared.whp",invalid);},"Invalid state rejected before creating folders");
        check(!fs::exists(root/"invalid"),"Invalid write has no directory mutation");
        invalid=value;invalid.variables.clear();preserves(invalid);
        invalid=value;invalid.flags.clear();preserves(invalid);
        invalid=value;invalid.variables.resize(4097);preserves(invalid);
        invalid=value;invalid.flags.resize(16385);preserves(invalid);
        invalid=value;invalid.flags[0]=2;preserves(invalid);
        invalid.flags[0]=255;preserves(invalid);

        auto corrupt=original;corrupt.back()^=1;writeBytes(broken,corrupt);
        rejects([&]{mahoyo::shared::read(broken);},"Outer CRC corruption rejected");
        corrupt=original;corrupt.pop_back();writeBytes(broken,corrupt);
        rejects([&]{mahoyo::shared::read(broken);},"Truncated outer file rejected");
        const auto invalidDocument=[&](mahoyo::save::Document bad){
            mahoyo::save::writeAtomic(broken,bad);const auto before=bytes(broken);
            rejects([&]{mahoyo::shared::read(broken);},"Malformed shared payload rejected despite valid CRC");
            check(bytes(broken)==before,"Read validation never rewrites malformed input");
        };
        auto bad=document;bad.hostExtra[9]='2';invalidDocument(bad);
        bad=document;std::fill(bad.hostExtra.begin()+10,bad.hostExtra.begin()+18,0);invalidDocument(bad);
        bad=document;put32(bad.hostExtra,18,0);invalidDocument(bad);
        bad=document;put32(bad.hostExtra,18,4097);invalidDocument(bad);
        bad=document;put32(bad.hostExtra,22,0);invalidDocument(bad);
        bad=document;put32(bad.hostExtra,22,16385);invalidDocument(bad);
        bad=document;bad.hostExtra.back()=2;invalidDocument(bad);
        bad=document;bad.hostExtra.push_back(0);invalidDocument(bad);
        bad=document;bad.hostExtra.pop_back();invalidDocument(bad);
        bad=document;bad.text="unrelated slot field";invalidDocument(bad);
        bad=document;bad.vm={1};invalidDocument(bad);
        {std::ofstream file(large,std::ios::binary);file.seekp(40*1024);file.put(0);}
        rejects([&]{mahoyo::shared::read(large);},"Oversized physical file rejected");

#ifdef _WIN32
        HANDLE lock=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(lock!=INVALID_HANDLE_VALUE,"Lock synthetic destination against replacement");
        auto replacement=value;replacement.generation=2;
        rejects([&]{mahoyo::shared::write(path,replacement);},"OS replacement failure reported");
        CloseHandle(lock);check(bytes(path)==original,"OS replacement failure preserves old registry");
#endif
        State maximum{std::numeric_limits<uint64_t>::max(),std::vector<int32_t>(4096),std::vector<uint8_t>(16384)};
        maximum.variables.back()=std::numeric_limits<int32_t>::min();maximum.flags.back()=1;
        mahoyo::shared::write(path,maximum);equal(*mahoyo::shared::read(path),maximum);
        check(fs::file_size(path)<40*1024,"Largest accepted state fits physical cap");
        State lower{1,{7},{1}};mahoyo::shared::write(path,lower);equal(*mahoyo::shared::read(path),lower);
        check(mahoyo::shared::read(path)->generation==1,"Codec does not impose merge or monotonic-generation policy");
        for(const auto& item:fs::directory_iterator(root))
            check(item.path().filename().string().find(".tmp.")==std::string::npos,"No temporary files remain");
        fs::remove(path);fs::remove(broken);fs::remove(large);fs::remove(root);
        std::cout<<"Shared state roundtrip, limits, corruption and atomic replacement checks passed\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<" (fixtures retained at "<<root.string()<<")\n";return 1;
    }
}
