#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace mahoyo::clock {
//Native380's resource-specific clock layout. P/O are internal native units;
//PNG storage dimensions are deliberately absent from this producer.
struct Part {const char* image;float x,y,uniform,rotation;int px,py,alphaFixed16,priority,colorMode,variant;};
inline float f(float value){volatile float rounded=value;return rounded;}
inline float add(float a,float b){return f(a+b);}
inline float mul(float a,float b){return f(a*b);}
inline float div(float a,float b){return f(a/b);}
inline int32_t trunc(float value){return !std::isfinite(value)||double(value)<INT32_MIN||double(value)>=2147483648.?INT32_MIN:int32_t(value);}
inline int32_t reduce(int32_t value,int32_t half){return int32_t(uint32_t(value)-uint32_t(value/half)*uint32_t(half*2));}
inline float angle(int32_t hundredths){auto value=uint16_t(trunc(div(mul(float(hundredths),32768),18000)));int signedValue=value<32768?int(value):int(value)-65536;return -mul(mul(float(signedValue),180),1.f/32768);}
inline std::array<float,2> hands(int32_t hour,int32_t minute){
    int32_t h=int32_t((uint32_t(reduce(hour,6))*60u+uint32_t(minute))*50u);
    int32_t m=int32_t(uint32_t(reduce(minute,30))*600u);
    return {angle(h),angle(m)};
}
inline std::array<Part,5> parts(int32_t hour,int32_t minute,int32_t x,int32_t y,int32_t permille,int color,int faceW,int faceH){
    float px=div(mul(float(int32_t(uint32_t(x)+820u)),640),480);
    float py=div(mul(float(int32_t(uint32_t(y)+635u)),480),360);
    float u=div(float(permille),1000);
    float dx=div(mul(mul(mul(mul(u,9),480),1.f/1024),640),480);
    float dy=div(mul(div(mul(mul(u,13),272),576),480),360);
    auto a=hands(hour,minute);
    auto point=[](float n){return float(trunc(n));};
    return {{{"img1242.mzp",point(px),point(py),u,0,faceW/2,faceH/2,65536,100,color,0},
             {"img1244.mzp",point(add(px,1)),point(add(py,1)),u,a[0],43,391,65536,103,color,0},
             {"img1244.mzp",point(add(add(px,1),dx)),point(add(add(py,1),dy)),u,a[0],43,391,16448,101,1,2},
             {"img1246.mzp",point(add(px,2)),point(add(py,3)),u,a[1],38,514,65536,104,color,0},
             {"img1246.mzp",point(add(add(px,2),dx)),point(add(add(py,3),dy)),u,a[1],38,514,16448,102,1,2}}};
}
}
