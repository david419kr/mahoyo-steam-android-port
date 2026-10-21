#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mahoyo::archive {
struct Entry {
    int story=0,group=0,chapter=0,readFlag=0,local24=0,local25=0,local26=0;
    std::vector<int> required;
    std::string label,preview,target;
};
struct Catalog {
    std::vector<Entry> entries;
    std::map<int,std::string> headers;
    const Entry* find(int story) const {for(const auto& e:entries)if(e.story==story)return &e;return nullptr;}
    static bool eligible(const Entry& e,const std::vector<uint8_t>& flags) {
        return std::all_of(e.required.begin(),e.required.end(),[&](int f){return size_t(f)>=flags.size()||flags[size_t(f)]!=0;});
    }
    static bool read(const Entry& e,const std::vector<uint8_t>& flags) {return size_t(e.readFlag)>=flags.size()||flags[size_t(e.readFlag)]!=0;}
    static Catalog load(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Archive metadata is missing");return parse(file);
    }
    static Catalog parse(std::istream& file) {
        std::string line,tag,hash,extra;int count=0,groups=0;
        if(!std::getline(file,line))throw std::runtime_error("Empty Archive metadata");
        std::istringstream first(line);
        if(!(first>>tag>>hash>>count>>groups)||first>>extra||tag!="WOHARCHIVE1"||hash!="1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b"||count!=73||groups!=20)throw std::runtime_error("Invalid Archive metadata header");
        Catalog c;std::set<int> ids,chapters;
        auto name=[](const std::string& s){return !s.empty()&&s.size()<128&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-.")==std::string::npos&&s!="."&&s!="..";};
        while(std::getline(file,line)) {
            if(line.empty())continue;if(line.size()>4096)throw std::runtime_error("Archive row exceeds limit");
            std::istringstream row(line);row>>tag;
            if(tag=="G") {int id;std::string header;if(!(row>>id>>std::quoted(header))||row>>extra||id<0||id>=20||!name(header)||!c.headers.emplace(id,header).second)throw std::runtime_error("Invalid Archive group");}
            else if(tag=="E") {
                Entry e;std::string gates;
                if(!(row>>e.story>>e.group>>e.chapter>>e.readFlag>>gates>>std::quoted(e.label)>>std::quoted(e.preview)>>std::quoted(e.target)>>e.local24>>e.local25>>e.local26)||row>>extra||e.story<1||e.story>73||e.group<0||e.group>=20||e.chapter<0||e.readFlag<0||e.readFlag>=2048||e.label.empty()||e.label.size()>1024||!name(e.preview)||!name(e.target)||e.local26!=e.story||!ids.insert(e.story).second||!chapters.insert(e.chapter).second)throw std::runtime_error("Invalid Archive entry");
                if(e.preview.size()<4||e.preview.substr(e.preview.size()-4)!=".mzp"||e.target.size()<4||e.target.substr(e.target.size()-4)!=".chs")throw std::runtime_error("Invalid Archive resource extension");
                for(unsigned char ch:e.label)if(ch<32)throw std::runtime_error("Control character in Archive label");
                std::set<int> seen;size_t begin=0;
                do{auto end=gates.find(',',begin);auto token=gates.substr(begin,end-begin);if(token.empty()||token.size()>4||token.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid Archive gate");int flag=std::stoi(token);if(flag>=2048||!seen.insert(flag).second)throw std::runtime_error("Invalid Archive gate");e.required.push_back(flag);if(end==std::string::npos)break;begin=end+1;}while(true);
                if(e.required.size()>16)throw std::runtime_error("Too many Archive gates");c.entries.push_back(std::move(e));
            }else throw std::runtime_error("Unknown Archive metadata row");
        }
        if(c.entries.size()!=73||c.headers.size()!=20)throw std::runtime_error("Incomplete Archive metadata");
        std::sort(c.entries.begin(),c.entries.end(),[](const Entry& a,const Entry& b){return a.story<b.story;});return c;
    }
};
} // namespace mahoyo::archive
