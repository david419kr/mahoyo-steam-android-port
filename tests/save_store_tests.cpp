#include "../runtime/save_store.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs=std::filesystem;
using mahoyo::save::Document;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static std::vector<uint8_t> bytes(const fs::path& path){std::ifstream stream(path,std::ios::binary);return {std::istreambuf_iterator<char>(stream),{}};}
static void writeBytes(const fs::path& path,const std::vector<uint8_t>& bytes){std::ofstream stream(path,std::ios::binary|std::ios::trunc);stream.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));if(!stream)throw std::runtime_error("Cannot write synthetic fixture");}
static void put32(std::vector<uint8_t>& bytes,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)bytes.at(at+i)=uint8_t(n>>(i*8));}
static void repairChecksum(std::vector<uint8_t>& bytes){uint32_t crc=0xffffffffu;for(size_t i=16;i<bytes.size();++i){crc^=bytes[i];for(unsigned k=0;k<8;++k)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}put32(bytes,12,crc^0xffffffffu);}
template<typename Work> static void rejects(Work work,const char* message){bool error=false;try{work();}catch(const std::exception&){error=true;}check(error,message);}
static Document fixture(){
    Document document;document.vm={0,1,255,0,9};
    mahoyo::save::Layer layer;layer.id=-7;layer.z=42;layer.image="image 日本語.mzp";layer.kind="background";layer.color="#102030";
    layer.x=-0.0;layer.y=-142.25;layer.w=2304.5;layer.h=1296.125;layer.alpha=0.625;layer.crop={-1,2,333,444};layer.hasCrop=true;document.layers.push_back(layer);
    document.audio.push_back({"voice 日本語.hw","voice",false,0.75});document.audio.push_back({"bgm.hw","bgm",true,0.125});
    document.history={"first paragraph","日本語の履歴",std::string("embedded\0null",13)};
    document.text="current text\n二行目";document.textLine=231;document.hidden=true;document.hostExtra={0,255,3,0,17};return document;
}
static void equal(const Document& actual,const Document& expected){
    check(actual.vm==expected.vm&&actual.history==expected.history&&actual.text==expected.text&&actual.textLine==expected.textLine&&actual.hidden==expected.hidden&&actual.hostExtra==expected.hostExtra,"document fields round trip");
    check(actual.layers.size()==expected.layers.size()&&actual.audio.size()==expected.audio.size(),"collection counts round trip");
    for(size_t i=0;i<actual.layers.size();++i){const auto& a=actual.layers[i];const auto& e=expected.layers[i];
        check(a.id==e.id&&a.z==e.z&&a.image==e.image&&a.kind==e.kind&&a.color==e.color&&a.x==e.x&&std::signbit(a.x)==std::signbit(e.x)&&a.y==e.y&&a.w==e.w&&a.h==e.h&&a.alpha==e.alpha&&a.crop==e.crop&&a.hasCrop==e.hasCrop,"all layer fields round trip");}
    for(size_t i=0;i<actual.audio.size();++i){const auto& a=actual.audio[i];const auto& e=expected.audio[i];check(a.name==e.name&&a.channel==e.channel&&a.loop==e.loop&&a.volume==e.volume,"all audio fields round trip");}
}
int main(){
    const auto root=fs::temp_directory_path()/("mahoyo-save-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    try{
        const auto path=root/"slot1.whs",broken=root/"broken.whs",large=root/"large.whs";
        auto document=fixture();mahoyo::save::writeAtomic(path,document);equal(mahoyo::save::read(path),document);
        const auto valid=bytes(path);
        auto badVersion=valid;badVersion[7]='1';writeBytes(broken,badVersion);rejects([&]{mahoyo::save::read(broken);},"legacy container version rejected");
        auto corrupt=valid;corrupt.back()^=1;writeBytes(broken,corrupt);rejects([&]{mahoyo::save::read(broken);},"bad checksum rejected");
        corrupt=valid;corrupt.resize(corrupt.size()-7);writeBytes(broken,corrupt);rejects([&]{mahoyo::save::read(broken);},"truncated file rejected");
        {std::ofstream stream(large,std::ios::binary);stream.seekp(64*1024*1024);stream.put(0);}
        rejects([&]{mahoyo::save::read(large);},"oversized physical file rejected before allocation");
        corrupt=valid;put32(corrupt,16,32*1024*1024+1);repairChecksum(corrupt);writeBytes(broken,corrupt);rejects([&]{mahoyo::save::read(broken);},"valid CRC with oversized VM field rejected");
        auto invalid=document;invalid.text.resize(1024*1024+1,'x');rejects([&]{mahoyo::save::writeAtomic(path,invalid);},"oversized string rejected");check(bytes(path)==valid,"invalid overwrite preserves previous file");
        invalid=document;invalid.hostExtra.resize(1024*1024+1,17);rejects([&]{mahoyo::save::writeAtomic(path,invalid);},"oversized host extra rejected");check(bytes(path)==valid,"invalid host extra overwrite preserves previous file");
        invalid=document;invalid.layers.resize(129);rejects([&]{mahoyo::save::writeAtomic(path,invalid);},"oversized layer list rejected");
        invalid=document;invalid.history.resize(513);rejects([&]{mahoyo::save::writeAtomic(path,invalid);},"oversized history list rejected");
        invalid=document;invalid.layers[0].alpha=std::numeric_limits<double>::quiet_NaN();rejects([&]{mahoyo::save::writeAtomic(path,invalid);},"nonfinite value rejected");check(bytes(path)==valid,"failed numeric validation preserves previous file");
        // Forge a valid CRC around a non-finite layer x value. Geometry validation
        // must reject this even when the outer integrity check succeeds.
        corrupt=valid;size_t xOffset=16+4+document.vm.size()+4+8;
        for(const auto* field:{&document.layers[0].image,&document.layers[0].kind,&document.layers[0].color})xOffset+=4+field->size();
        put32(corrupt,xOffset,0);put32(corrupt,xOffset+4,0x7ff00000u);repairChecksum(corrupt);writeBytes(broken,corrupt);
        rejects([&]{mahoyo::save::read(broken);},"nonfinite decoded number rejected with valid CRC");
        corrupt=valid;put32(corrupt,corrupt.size()-document.hostExtra.size()-8,2);repairChecksum(corrupt);writeBytes(broken,corrupt);rejects([&]{mahoyo::save::read(broken);},"invalid decoded boolean rejected");
#ifdef _WIN32
        {
        HANDLE lock=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(lock!=INVALID_HANDLE_VALUE,"open previous save with replacement denied");
        auto replacement=document;replacement.text="replacement while locked";
        rejects([&]{mahoyo::save::writeAtomic(path,replacement);},"OS replacement failure reported");
        CloseHandle(lock);check(bytes(path)==valid,"OS replacement failure preserves previous save");
        }
#endif
        auto replacement=document;replacement.text="replacement accepted";replacement.textLine=232;
        mahoyo::save::writeAtomic(path,replacement);equal(mahoyo::save::read(path),replacement);
        for(const auto& item:fs::directory_iterator(root))check(item.path().extension()!=".tmp"&&item.path().filename().string().find(".tmp.")==std::string::npos,"temporary file cleaned after success/failure");
        // Remove only exact synthetic filenames made by this test, never a
        // recursive directory or application save directory.
        fs::remove(path);fs::remove(broken);fs::remove(large);fs::remove(root);
        std::cout<<"Save container roundtrip, bounds, integrity and atomic replacement checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<" (fixtures retained at "<<root.string()<<")\n";return 1;}
}
