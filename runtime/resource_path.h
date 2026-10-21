#pragma once
#include <string>
namespace mahoyo::resource {
inline bool safe(const std::string& path){
    if(path.empty()||path.front()=='/')return false;
    for(unsigned char c:path)if(c<32||c=='\\'||c==':')return false;
    size_t start=0;while(start<=path.size()){auto end=path.find('/',start);auto part=path.substr(start,end==std::string::npos?end:end-start);if(part.empty()||part=="."||part=="..")return false;if(end==std::string::npos)break;start=end+1;}
    if(path=="assets.json")return true;
    auto slash=path.find('/');if(slash==std::string::npos)return false;auto top=path.substr(0,slash);return top=="audio"||top=="fonts"||top=="images"||top=="meta"||top=="scripts"||top=="text"||top=="video";
}
}
