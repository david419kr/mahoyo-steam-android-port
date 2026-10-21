#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mahoyo::image_header {
// Resource names remain logical PNG paths; the payload may be exact lossless WebP.
inline std::array<int,2> size(const unsigned char* p,size_t n){
    auto le24=[&](size_t at){return uint32_t(p[at])|(uint32_t(p[at+1])<<8)|(uint32_t(p[at+2])<<16);};
    auto be32=[&](size_t at){return (uint32_t(p[at])<<24)|(uint32_t(p[at+1])<<16)|(uint32_t(p[at+2])<<8)|p[at+3];};
    uint32_t w=0,h=0;
    if(n>=24&&!std::memcmp(p,"\x89PNG\r\n\x1a\n",8)&&!std::memcmp(p+12,"IHDR",4)){w=be32(16);h=be32(20);}
    else if(n>=25&&!std::memcmp(p,"RIFF",4)&&!std::memcmp(p+8,"WEBP",4)){
        if(n>=30&&!std::memcmp(p+12,"VP8X",4)){w=le24(24)+1;h=le24(27)+1;}
        else if(!std::memcmp(p+12,"VP8L",4)&&p[20]==0x2f){w=1+p[21]+((p[22]&63)<<8);h=1+(p[22]>>6)+(uint32_t(p[23])<<2)+((p[24]&15)<<10);}
        else if(n>=30&&!std::memcmp(p+12,"VP8 ",4)&&p[23]==0x9d&&p[24]==1&&p[25]==0x2a){w=(p[26]|(uint32_t(p[27])<<8))&0x3fff;h=(p[28]|(uint32_t(p[29])<<8))&0x3fff;}
    }
    if(!w||!h||w>65535||h>65535)return {};
    return {int(w),int(h)};
}
inline std::map<std::string,std::array<int,2>> dimensions(std::istream& input){
    std::map<std::string,std::array<int,2>> result;std::string line;
    if(!std::getline(input,line)||line!="WOH_IMAGE_DIMENSIONS1")throw std::runtime_error("Invalid prepared image dimensions header");
    size_t bytes=line.size();
    while(std::getline(input,line)){
        bytes+=line.size()+1;if(bytes>4*1024*1024||result.size()>=100000)throw std::runtime_error("Prepared image dimensions budget");
        std::istringstream row(line);std::string name,extra;int w,h;
        if(!(row>>name>>w>>h)||row>>extra||name.empty()||name=="."||name==".."||name.find_first_of("/\\:")!=std::string::npos||w<=0||h<=0||w>65535||h>65535||!result.emplace(name,std::array<int,2>{w,h}).second)throw std::runtime_error("Invalid prepared image dimensions row");
    }
    if(result.empty())throw std::runtime_error("Prepared image dimensions empty");return result;
}
}
