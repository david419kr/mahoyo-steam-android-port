#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mahoyo::extra {
inline constexpr const char* NativeHash="1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b";
inline bool fileName(const std::string& name){return !name.empty()&&name.size()<128&&name!="."&&name!=".."&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string::npos;}
inline std::ifstream open(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Extra metadata missing: "+path.filename().string());return file;}
inline std::string header(std::istream& file,const std::string& tag,int count,int second=-1){std::string line,actual,hash,extra;int n=0,m=0;if(!std::getline(file,line))throw std::runtime_error("Missing Extra header");std::istringstream row(line);if(!(row>>actual>>hash>>n)||(second>=0&&!(row>>m))||row>>extra||actual!=tag||hash!=NativeHash||n!=count||(second>=0&&m!=second))throw std::runtime_error("Invalid Extra header");return line;}
inline bool flag(size_t f,const std::vector<uint8_t>& flags){return f<flags.size()&&flags[f]!=0;}
inline bool complete(const std::vector<uint8_t>& flags){for(size_t f:{128,65,73,72,206,129,130,131,132,133,134})if(!flag(f,flags))return false;return true;}
struct Variant{int index=0;std::string name;bool seen=false;std::vector<int> required;};
struct Group{int id=0;std::vector<Variant> variants;};
struct Gallery {
    std::vector<Group> groups;
    const Group* find(int id)const{for(const auto& g:groups)if(g.id==id)return &g;return nullptr;}
    static bool unlocked(const Variant& v,const std::vector<uint8_t>& flags,const std::set<std::string>& seen){return complete(flags)||(v.seen?seen.count(v.name)!=0:std::all_of(v.required.begin(),v.required.end(),[&](int f){return flag(size_t(f),flags);}));}
    static Gallery parse(std::istream& file){header(file,"WOHGALLERY1",401,1314);Gallery result;std::string line,tag,extra;std::map<int,size_t> expected;std::set<std::string> names;
        while(std::getline(file,line)){if(line.empty())continue;if(line.size()>1024)throw std::runtime_error("Gallery row exceeds limit");std::istringstream row(line);row>>tag;
            if(tag=="G"){int id,count;if(!(row>>id>>count)||row>>extra||id<0||id>=420||count<1||count>37||!expected.emplace(id,size_t(count)).second)throw std::runtime_error("Invalid gallery group");result.groups.push_back(Group{id,{}});}
            else if(tag=="V"){int id;Variant v;std::string gates;if(!(row>>id>>v.index>>std::quoted(v.name)>>gates)||row>>extra||result.groups.empty()||result.groups.back().id!=id||v.index!=int(result.groups.back().variants.size())||v.index>=37||v.name.size()!=11||v.name.substr(0,3)!="img"||v.name.substr(7)!=".mzp"||v.name.substr(3,4).find_first_not_of("0123456789")!=std::string::npos||!names.insert(v.name).second)throw std::runtime_error("Invalid gallery variant");
                v.seen=gates=="-";if(!v.seen){size_t begin=0;std::set<int> unique;do{auto end=gates.find(',',begin);auto token=gates.substr(begin,end-begin);if(token.empty()||token.size()>4||token.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid gallery flag");int f=std::stoi(token);if(f>=2048||!unique.insert(f).second)throw std::runtime_error("Invalid gallery flag");v.required.push_back(f);if(end==std::string::npos)break;begin=end+1;}while(true);if(v.required.size()>6)throw std::runtime_error("Gallery condition exceeds native size");}
                result.groups.back().variants.push_back(std::move(v));}
            else throw std::runtime_error("Unknown gallery row");}
        if(result.groups.size()!=401||names.size()!=1314)throw std::runtime_error("Incomplete gallery metadata");for(const auto& g:result.groups)if(g.variants.size()!=expected.at(g.id))throw std::runtime_error("Incomplete gallery group");return result;
    }
    static Gallery load(const std::filesystem::path& path){auto file=open(path);return parse(file);}
};
struct Track{int row=0,id=0,heardFlag=0;std::string name,atlas;std::array<int,4> crop{};};
struct Music {
    std::vector<Track> tracks;
    static bool atlasName(const std::string& name){
        for(int page=1;page<=3;++page)for(const char* locale:{"ja","en","zc","zt"})
            if(name=="mu_text"+std::to_string(page)+"_select_"+locale+".cbg")return true;
        return false;
    }
    const Track* find(int row)const{return row>=0&&size_t(row)<tracks.size()?&tracks[size_t(row)]:nullptr;}
    static bool unlocked(const Track& t,const std::vector<uint8_t>& flags){return flag(size_t(t.heardFlag),flags)||complete(flags);}
    int adjacent(int row,int direction,const std::vector<uint8_t>& flags)const{if(row<0||size_t(row)>=tracks.size())row=direction>0?int(tracks.size())-1:0;for(size_t n=0;n<tracks.size();++n){row=(row+direction+int(tracks.size()))%int(tracks.size());if(unlocked(tracks[size_t(row)],flags))return row;}return -1;}
    static Music parse(std::istream& file){header(file,"WOHMUSIC1",56);Music result;std::string line,tag,extra;std::set<int> ids;
        while(std::getline(file,line)){if(line.empty())continue;if(line.size()>1024)throw std::runtime_error("Music row exceeds limit");std::istringstream row(line);Track t;
            if(!(row>>tag>>t.row>>t.id>>t.heardFlag>>std::quoted(t.name)>>std::quoted(t.atlas)>>t.crop[0]>>t.crop[1]>>t.crop[2]>>t.crop[3])||row>>extra||tag!="M"||t.row!=int(result.tracks.size())||t.id<1||t.id>65||t.heardFlag!=256+t.id||!ids.insert(t.id).second||!fileName(t.name)||t.name.size()<4||t.name.substr(t.name.size()-3)!=".hw"||!atlasName(t.atlas)||(t.crop[0]!=0&&t.crop[0]!=528)||t.crop[1]<0||t.crop[1]>936||t.crop[1]%72||t.crop[2]!=528||t.crop[3]!=72)throw std::runtime_error("Invalid music row");result.tracks.push_back(std::move(t));}
        if(result.tracks.size()!=56)throw std::runtime_error("Incomplete music metadata");return result;
    }
    static Music load(const std::filesystem::path& path){auto file=open(path);return parse(file);}
};
inline std::map<int,std::string> checkpoints(const std::filesystem::path& path){auto file=open(path);header(file,"WOHCHECKPOINT1",13);std::map<int,std::string> result;std::set<std::string> names;std::string line,tag,extra,name;
    while(std::getline(file,line)){if(line.empty())continue;std::istringstream row(line);int id;if(!(row>>tag>>id>>std::quoted(name))||row>>extra||tag!="C"||id<1||id>13||!fileName(name)||name.size()<12||name.substr(0,7)!="teatime"||name.substr(name.size()-4)!=".chs"||!result.emplace(id,name).second||!names.insert(name).second)throw std::runtime_error("Invalid checkpoint return map");}
    if(result.size()!=13)throw std::runtime_error("Incomplete checkpoint return map");return result;
}
} // namespace mahoyo::extra
