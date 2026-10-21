#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace mahoyo::packed {
struct Range {uint64_t offset=0,length=0;};
inline bool fits(uint64_t offset,uint64_t length,uint64_t total){return offset<=total&&length<=total-offset;}
inline bool safe(const std::string& path){
    if(path.empty()||path.front()=='/')return false;
    for(unsigned char c:path)if(c<32||c=='\\'||c==':')return false;
    size_t start=0;while(start<=path.size()){auto end=path.find('/',start);auto part=path.substr(start,end==std::string::npos?end:end-start);if(part.empty()||part=="."||part=="..")return false;if(end==std::string::npos)break;start=end+1;}
    if(path=="assets.json")return true;
    auto slash=path.find('/');if(slash==std::string::npos)return false;auto top=path.substr(0,slash);return top=="audio"||top=="fonts"||top=="images"||top=="meta"||top=="scripts"||top=="text"||top=="video";
}
inline uint64_t decimal(const std::string& text){
    uint64_t value=0;if(text.empty()||(text.size()>1&&text[0]=='0'))throw std::runtime_error("Invalid OBB integer");
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc()||parsed.ptr!=text.data()+text.size()||value>uint64_t(std::numeric_limits<int64_t>::max()))throw std::runtime_error("OBB integer overflow");return value;
}
class Assets {
    std::filesystem::path archive;
    std::unordered_map<std::string,Range> entries;
public:
    bool active()const{return !archive.empty();}
    void open(const std::filesystem::path& root){
        const auto index=root/"meta/obb-index.tsv",source=root/"meta/obb-source.txt";
        if(!std::filesystem::exists(index)&&!std::filesystem::exists(source))return;
        if(!std::filesystem::is_regular_file(index)||!std::filesystem::is_regular_file(source)||std::filesystem::file_size(index)>16*1024*1024||std::filesystem::file_size(source)>4096)throw std::runtime_error("Incomplete OBB metadata");
        std::ifstream pointer(source);std::string name,extra;if(!std::getline(pointer,name)||name.empty()||std::getline(pointer,extra))throw std::runtime_error("Invalid OBB source");
        archive=std::filesystem::u8path(name);if(!archive.is_absolute()||!std::filesystem::is_regular_file(archive))throw std::runtime_error("OBB file missing");
        std::ifstream input(index);parse(input,std::filesystem::file_size(archive));
    }
    void parse(std::istream& input,uint64_t total){
        std::string line;if(!std::getline(input,line)||line!="WOHOBB1")throw std::runtime_error("Unknown OBB index");
        uint64_t end=0;size_t budget=0;
        while(std::getline(input,line)){
            budget+=line.size()+1;if(budget>16*1024*1024||entries.size()>=100000)throw std::runtime_error("OBB index budget");
            std::array<std::string,4> row;size_t at=0;
            for(size_t i=0;i<4;i++){auto sep=line.find('\t',at);if((i<3)==(sep==std::string::npos))throw std::runtime_error("Invalid OBB row");row[i]=line.substr(at,sep==std::string::npos?sep:sep-at);at=sep+1;}
            if(!safe(row[0])||row[3].size()!=8||!std::all_of(row[3].begin(),row[3].end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))throw std::runtime_error("Invalid OBB path/CRC");
            Range range{decimal(row[1]),decimal(row[2])};if(!fits(range.offset,range.length,total)||range.offset<end||!entries.emplace(row[0],range).second)throw std::runtime_error("OBB range/overlap");end=range.offset+range.length;
        }
        if(!entries.count("assets.json"))throw std::runtime_error("OBB manifest missing");
    }
    bool contains(const std::string& name)const{return entries.count(name)!=0;}
    size_t header(const std::string& name,unsigned char* bytes,size_t capacity)const{
        auto found=entries.find(name);if(found==entries.end())return 0;
        size_t count=static_cast<size_t>(std::min(uint64_t(capacity),found->second.length));
        std::ifstream file(archive,std::ios::binary);if(!file)return false;
        file.seekg(static_cast<std::streamoff>(found->second.offset));file.read(reinterpret_cast<char*>(bytes),static_cast<std::streamsize>(count));return file.gcount()==static_cast<std::streamsize>(count)?count:0;
    }
    bool prefix(const std::string& name,unsigned char* bytes,size_t count)const{return header(name,bytes,count)==count;}
};
}
