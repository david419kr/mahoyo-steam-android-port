#pragma once
#include "packed_assets.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mahoyo::language {
inline bool supported(const std::string& edition,const std::string& language){
    return language=="ja"||language=="zh-Hans"||language=="zh-Hant"||(edition=="korean"?language=="ko":language=="en");
}
inline int slot(const std::string& language){if(language=="ja")return 0;if(language=="en"||language=="ko")return 1;if(language=="zh-Hans")return 2;if(language=="zh-Hant")return 3;throw std::runtime_error("Unsupported language");}
inline std::string suffix(const std::string& language){return std::array<std::string,4>{"ja","en","zc","zt"}.at(slot(language));}
inline bool logicalName(const std::string& name){return !name.empty()&&name!="."&&name!=".."&&std::all_of(name.begin(),name.end(),[](unsigned char c){return c>32&&c!='/'&&c!='\\'&&c!=':';});}
inline bool ends(const std::string& value,const std::string& tail){return value.size()>=tail.size()&&value.compare(value.size()-tail.size(),tail.size(),tail)==0;}
inline std::string defaultPath(const std::string& name){
    if(!logicalName(name))throw std::runtime_error("Invalid logical language asset");
    if(ends(name,".mzp")||ends(name,".cbg"))return "images/"+name+".png";
    if(ends(name,".hw"))return "audio/"+name+".ogg";
    if(ends(name,".chs"))return "scripts/"+name;
    if(ends(name,".ctd"))return "text/"+name;
    if(ends(name,".ccit"))return "fonts/"+name+".bin";
    if(ends(name,".mp4"))return "video/"+name;
    return {};
}
struct Record {std::string path;std::array<int,2> size{};};
class Assets {
    std::string edition_="vanilla",language_="ja";
    std::map<std::string,Record> korean;
    mutable std::map<std::string,std::string> names;
public:
    const std::string& edition()const{return edition_;}
    const std::string& current()const{return language_;}
    void open(const std::filesystem::path& root,const std::string& expected="",const std::string& selected=""){
        edition_="vanilla";korean.clear();names.clear();
        const auto marker=root/"meta/game-edition.txt";
        if(std::filesystem::exists(marker)){
            if(std::filesystem::file_size(marker)>256)throw std::runtime_error("Edition marker budget");
            std::ifstream file(marker);std::string magic,extra;
            if(!std::getline(file,magic)||magic!="WOHEDITION1"||!std::getline(file,edition_)||std::getline(file,extra))throw std::runtime_error("Invalid edition marker");
        }
        if((edition_!="vanilla"&&edition_!="korean")||(!expected.empty()&&expected!=edition_))throw std::runtime_error("Game/OBB edition mismatch");
        const auto index=root/"meta/language-assets.tsv";
        if(std::filesystem::exists(index)){
            if(std::filesystem::file_size(index)>4*1024*1024)throw std::runtime_error("Language asset index budget");
            std::ifstream file(index);std::string line;
            if(!std::getline(file,line)||line!="WOHLANGASSETS1")throw std::runtime_error("Invalid language asset index");
            while(std::getline(file,line)){
                std::istringstream row(line);std::string locale,name,path,extra;int w,h;
                if(!(row>>locale>>name>>path>>w>>h)||row>>extra||locale!="ko"||!logicalName(name)||!mahoyo::packed::safe(path)||w<0||h<0||w>65535||h>65535||(w==0)!=(h==0))throw std::runtime_error("Invalid language asset row");
                auto wanted=defaultPath(name);auto slash=wanted.find('/');if(slash==std::string::npos)throw std::runtime_error("Unknown language asset type");wanted.insert(slash+1,"ko/");
                const bool composite=ends(name,".mzp")&&w>0&&path=="images/ko/"+name+".composite.png";
                if((path!=wanted&&!composite)||!korean.emplace(name,Record{path,{w,h}}).second)throw std::runtime_error("Duplicate/unsafe language asset");
            }
        }
        if(edition_=="korean"&&!korean.count("script_text_en.ctd"))throw std::runtime_error("Korean text resources missing");
        select(selected.empty()?(edition_=="korean"?"ko":"ja"):selected);
    }
    void select(const std::string& language){if(!supported(edition_,language))throw std::runtime_error("Language not available in this edition");language_=language;names.clear();}
    const Record* koreanRecord(const std::string& name)const{auto i=korean.find(name);return i!=korean.end()?&i->second:nullptr;}
    const Record* overlay(const std::string& name)const{return language_=="ko"?koreanRecord(name):nullptr;}
    std::string localize(const std::string& logical,const std::function<bool(const std::string&)>& exists)const{
        auto cached=names.find(logical);if(cached!=names.end())return cached->second;
        std::string resolved=logical;
        for(size_t i=0;(i=logical.find('_',i))!=std::string::npos;++i){
            if(i+3>=logical.size()||(logical[i+3]!='.'&&logical[i+3]!='_'))continue;
            auto old=logical.substr(i+1,2);if(old!="ja"&&old!="en"&&old!="zc"&&old!="zt")continue;
            auto candidate=logical.substr(0,i+1)+suffix(language_)+logical.substr(i+3);
            if(overlay(candidate)||exists(candidate))resolved=candidate;
            break;
        }
        names.emplace(logical,resolved);return resolved;
    }
    std::string path(const std::string& name)const{if(auto record=overlay(name))return record->path;return defaultPath(name);}
    std::string tablePath()const{return path("script_text_"+suffix(language_)+".ctd");}
};
}
