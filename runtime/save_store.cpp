#include "save_store.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace mahoyo::save {
namespace {
constexpr size_t MaxFile=64*1024*1024,MaxVm=32*1024*1024,MaxString=1024*1024;
constexpr size_t HeaderSize=16,MaxLayers=128,MaxAudio=128,MaxHistory=512;
constexpr char Signature[]="WOHSAVE2";
constexpr std::array<uint32_t,256> crcTable(){
    std::array<uint32_t,256> result{};
    for(uint32_t i=0;i<256;++i){auto value=i;for(unsigned k=0;k<8;++k)value=(value>>1)^((value&1)?0xedb88320u:0);result[i]=value;}
    return result;
}
constexpr auto CrcTable=crcTable();
uint32_t crc32(const uint8_t* data,size_t size){uint32_t crc=0xffffffffu;for(size_t i=0;i<size;++i)crc=CrcTable[(crc^data[i])&255]^(crc>>8);return crc^0xffffffffu;}
void put32(std::vector<uint8_t>& bytes,size_t at,uint32_t value){for(unsigned n=0;n<4;++n)bytes.at(at+n)=uint8_t(value>>(n*8));}
uint32_t get32(const std::vector<uint8_t>& bytes,size_t at){if(at>bytes.size()||bytes.size()-at<4)throw Error("Truncated save dword");return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;}
struct Writer {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(HeaderSize);
    void room(size_t size){if(size>MaxFile-bytes.size())throw Error("Save container exceeds 64 MiB");}
    void u32(uint32_t value){room(4);const auto at=bytes.size();bytes.resize(at+4);put32(bytes,at,value);}
    void boolean(bool value){u32(value?1:0);}
    void number(double value){
        static_assert(sizeof(double)==8&&std::numeric_limits<double>::is_iec559,"Save requires IEEE754 binary64");
        if(!std::isfinite(value))throw Error("Save contains a non-finite number");
        uint64_t bits;std::memcpy(&bits,&value,8);u32(uint32_t(bits));u32(uint32_t(bits>>32));
    }
    void blob(const uint8_t* data,size_t size,size_t max){if(size>max)throw Error("Save field exceeds its size limit");room(4);u32(uint32_t(size));room(size);if(size)bytes.insert(bytes.end(),data,data+size);}
    void text(const std::string& value){blob(reinterpret_cast<const uint8_t*>(value.data()),value.size(),MaxString);}
};
struct Reader {
    const std::vector<uint8_t>& bytes;size_t at=HeaderSize;
    uint32_t u32(){auto value=get32(bytes,at);at+=4;return value;}
    bool boolean(){auto value=u32();if(value>1)throw Error("Invalid save boolean");return value!=0;}
    double number(){uint64_t bits=u32();bits|=uint64_t(u32())<<32;double value;std::memcpy(&value,&bits,8);if(!std::isfinite(value))throw Error("Save contains a non-finite number");return value;}
    std::vector<uint8_t> blob(size_t max){const auto size=u32();if(size>max||at>bytes.size()||size>bytes.size()-at)throw Error("Invalid save field size");std::vector<uint8_t> value(bytes.begin()+at,bytes.begin()+at+size);at+=size;return value;}
    std::string text(){auto value=blob(MaxString);return {value.begin(),value.end()};}
    uint32_t count(size_t maximum){auto value=u32();if(value>maximum)throw Error("Save collection exceeds its size limit");return value;}
};
std::vector<uint8_t> encode(const Document& document){
    if(document.layers.size()>MaxLayers||document.audio.size()>MaxAudio||document.history.size()>MaxHistory)throw Error("Save collection exceeds its size limit");
    Writer w;w.blob(document.vm.data(),document.vm.size(),MaxVm);
    w.u32(uint32_t(document.layers.size()));for(const auto& layer:document.layers){
        w.u32(uint32_t(layer.id));w.u32(uint32_t(layer.z));w.text(layer.image);w.text(layer.kind);w.text(layer.color);
        w.number(layer.x);w.number(layer.y);w.number(layer.w);w.number(layer.h);w.number(layer.alpha);
        for(auto value:layer.crop)w.u32(uint32_t(value));w.boolean(layer.hasCrop);
    }
    w.u32(uint32_t(document.audio.size()));for(const auto& audio:document.audio){w.text(audio.name);w.text(audio.channel);w.boolean(audio.loop);w.number(audio.volume);}
    w.u32(uint32_t(document.history.size()));for(const auto& item:document.history)w.text(item);
    w.text(document.text);w.u32(document.textLine);w.boolean(document.hidden);
    w.blob(document.hostExtra.data(),document.hostExtra.size(),MaxString);
    std::copy(Signature,Signature+8,w.bytes.begin());put32(w.bytes,8,uint32_t(w.bytes.size()-HeaderSize));
    put32(w.bytes,12,crc32(w.bytes.data()+HeaderSize,w.bytes.size()-HeaderSize));return w.bytes;
}
Document decode(const std::vector<uint8_t>& bytes){
    if(bytes.size()<HeaderSize||bytes.size()>MaxFile||std::memcmp(bytes.data(),Signature,8))throw Error("Invalid save container signature or size");
    if(get32(bytes,8)!=bytes.size()-HeaderSize)throw Error("Save payload length mismatch");
    if(get32(bytes,12)!=crc32(bytes.data()+HeaderSize,bytes.size()-HeaderSize))throw Error("Save checksum mismatch");
    Reader r{bytes};Document result;result.vm=r.blob(MaxVm);
    auto count=r.count(MaxLayers);result.layers.reserve(count);for(uint32_t i=0;i<count;++i){Layer layer;
        layer.id=int32_t(r.u32());layer.z=int32_t(r.u32());layer.image=r.text();layer.kind=r.text();layer.color=r.text();
        layer.x=r.number();layer.y=r.number();layer.w=r.number();layer.h=r.number();layer.alpha=r.number();
        for(auto& value:layer.crop)value=int32_t(r.u32());layer.hasCrop=r.boolean();result.layers.push_back(std::move(layer));
    }
    count=r.count(MaxAudio);result.audio.reserve(count);for(uint32_t i=0;i<count;++i){Audio audio;audio.name=r.text();audio.channel=r.text();audio.loop=r.boolean();audio.volume=r.number();result.audio.push_back(std::move(audio));}
    count=r.count(MaxHistory);result.history.reserve(count);for(uint32_t i=0;i<count;++i)result.history.push_back(r.text());
    result.text=r.text();result.textLine=r.u32();result.hidden=r.boolean();
    result.hostExtra=r.blob(MaxString);
    if(r.at!=bytes.size())throw Error("Unexpected save trailing data");return result;
}
std::filesystem::path temporaryPath(const std::filesystem::path& destination){
    static std::atomic<uint64_t> serial{0};
    auto name=destination.filename();
    name+=std::filesystem::path(".tmp."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(serial.fetch_add(1)));
    return destination.parent_path()/name;
}
void removeOwnTemporary(const std::filesystem::path& temporary) noexcept {std::error_code ignored;std::filesystem::remove(temporary,ignored);}
}

void writeAtomic(const std::filesystem::path& requested,const Document& document){
    // Encode and validate everything before any filesystem mutation.
    const auto bytes=encode(document);
    if(requested.empty()||requested.filename().empty()||requested.filename()=="."||requested.filename()=="..")throw Error("Invalid save filename");
    const auto destination=std::filesystem::absolute(requested);
    std::filesystem::create_directories(destination.parent_path());
    auto temporary=temporaryPath(destination);
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<16;++attempt){
        file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
        if(file!=INVALID_HANDLE_VALUE)break;
        if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)throw Error("Cannot create temporary save file");
        temporary=temporaryPath(destination);
    }
    if(file==INVALID_HANDLE_VALUE)throw Error("Cannot allocate a unique temporary save file");
    try{
        size_t at=0;while(at<bytes.size()){const auto part=DWORD(std::min<size_t>(bytes.size()-at,1024*1024));DWORD written=0;
            if(!WriteFile(file,bytes.data()+at,part,&written,nullptr)||written==0)throw Error("Cannot write temporary save file");at+=written;}
        if(!FlushFileBuffers(file))throw Error("Cannot flush temporary save file");
        if(!CloseHandle(file)){file=INVALID_HANDLE_VALUE;throw Error("Cannot close temporary save file");}file=INVALID_HANDLE_VALUE;
        if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw Error("Cannot replace save file (Windows error "+std::to_string(GetLastError())+")");
    }catch(...){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);removeOwnTemporary(temporary);throw;}
#else
    int file=-1;
    for(unsigned attempt=0;attempt<16;++attempt){
        file=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(file>=0)break;
        if(errno!=EEXIST)throw Error("Cannot create temporary save file");temporary=temporaryPath(destination);
    }
    if(file<0)throw Error("Cannot allocate a unique temporary save file");
    try{
        size_t at=0;while(at<bytes.size()){const auto written=::write(file,bytes.data()+at,bytes.size()-at);
            if(written<0&&errno==EINTR)continue;if(written<=0)throw Error("Cannot write temporary save file");at+=size_t(written);}
        if(::fsync(file)!=0)throw Error("Cannot flush temporary save file");
        if(::close(file)!=0){file=-1;throw Error("Cannot close temporary save file");}file=-1;
        if(::rename(temporary.c_str(),destination.c_str())!=0)throw Error("Cannot replace save file (errno "+std::to_string(errno)+")");
    }catch(...){if(file>=0)::close(file);removeOwnTemporary(temporary);throw;}
#endif
}
Document read(const std::filesystem::path& path){
    const auto size=std::filesystem::file_size(path);
    if(size<HeaderSize||size>MaxFile)throw Error("Invalid save container file size");
    std::ifstream file(path,std::ios::binary);if(!file)throw Error("Cannot open save file");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
    if(file.gcount()!=std::streamsize(bytes.size()))throw Error("Truncated save file");
    char extra;if(file.get(extra))throw Error("Save file grew while reading");
    return decode(bytes);
}
} // namespace mahoyo::save
