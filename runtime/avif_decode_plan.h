#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace mahoyo::avif_plan {
constexpr uint64_t sourcePixels=64ULL*1024*1024,fullPixels=192ULL*1024*1024/4,cropPixels=8'000'000;
struct Plan {uint32_t width,height,left,top,right,bottom;};
inline void bounds(uint32_t w,uint32_t h){if(!w||!h||w>16384||h>16384||uint64_t(w)*h>sourcePixels)throw std::runtime_error("AVIF source image budget");}
inline Plan make(uint32_t w,uint32_t h,int sample,bool crop,int l,int t,int r,int b){
    bounds(w,h);if(sample<1)throw std::runtime_error("Invalid AVIF sample");
    auto ceil=[&](uint32_t v){return uint32_t((uint64_t(v)+uint32_t(sample)-1)/uint32_t(sample));};
    Plan p{ceil(w),ceil(h),0,0,ceil(w),ceil(h)};
    if(crop){if(l<0||t<0||r<=l||b<=t||uint32_t(r)>w||uint32_t(b)>h)throw std::runtime_error("AVIF crop outside source");p.left=uint32_t(l)/sample;p.top=uint32_t(t)/sample;p.right=ceil(uint32_t(r));p.bottom=ceil(uint32_t(b));}
    if(uint64_t(p.right-p.left)*(p.bottom-p.top)>(crop?cropPixels:fullPixels))throw std::runtime_error("AVIF bitmap pixel budget");
    return p;
}
}
