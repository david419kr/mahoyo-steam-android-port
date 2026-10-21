#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <vector>
namespace mahoyo::effects {
uint32_t maskProgress(uint64_t elapsed,uint64_t duration,int strength);
enum Field {X,Y,Alpha,ScaleX,ScaleY,Rotation,WindowX,WindowY,WindowWidth,WindowHeight,WindowColor,WindowSoft,
            OriginX,OriginY,BlendMode,ColorMode,ColorR,ColorG,ColorB,VariantX,VariantY,Brightness,Contrast,AuxResourceAndScale,Priority};
struct Frame {uint32_t time=0;std::array<int32_t,6> values{};int curve=0,mode=0;int globalIndex=-1;std::array<int32_t,6> window{{-32767,-32767,-32767,-32767,-32767,-32767}};
    std::array<int32_t,13> properties{{-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767}};};
struct Clip {std::vector<Frame> frames;uint32_t loopStart=0,duration=0;uint64_t descriptor=0;bool windowMetadata=false,propertiesMetadata=false;
    //Derived from the pinned A43F0 ID map and first RAW curve, never captured keys.
    bool timeRemap=false;int timeCurve=0;int nativeId=0;};
struct Properties {
    //Native integer P, separate from unscaled draw offset O and259 target offsets.
    //C0/Z0 canvas pivot=+P*3/intrinsicSize; extent uses the separate U*axis scales.
    int32_t originX=0,originY=0,blendMode=0,colorMode=0;
    uint32_t rgb=0;
    int32_t variantX=0,variantY=0,brightness=0,contrast=0,priority=0,decimalMode=0;
};
struct FrameOverride {int32_t x=0,y=0,alpha=0;std::array<int32_t,4> window{};};
using FrameOverrides=std::map<int,FrameOverride>;
std::map<int,Clip> load(const std::filesystem::path& path);
//Optional sidecar; if present it must cover every existing frame exactly once.
void loadWindows(std::map<int,Clip>& clips,const std::filesystem::path& path);
//17columns: effect,localFrame,globalFrame,descriptorBits,raw C10A0 fields6,7,17..27.
//Present sidecars cover every frame and validate atomically; private8372/7007 noise keys are bounded.
void loadProperties(std::map<int,Clip>& clips,const std::filesystem::path& path);
//Native curves0..17,24/25(circular),27(exponential),31/32(elastic),35/36(back),39..41(bounce),70(clock).
//Input progress is clamped0..1; elastic/back output overshoot is retained.
// Unsupported curve IDs throw rather than silently falling back to linear.
double ease(double progress,int curve);
//Original084D90 arithmetic for precise segment cases0/1/2/3/6/7/10/11/24/25/27/32/35/36/39/40/41.
float interpolateNativeCurve(int curve,float elapsed,float start,float delta,float duration);
//Native084D90 case0x46 with explicit elapsed/duration SSE float boundaries.
float progress70(float elapsed,float duration);
//Pinned original A43F0 full-map proof, actual IDs1..8372. False means either
//a supported special-time clip or an out-of-range ID; this is not an allowlist.
bool hasIdentityTimeRemap(int id);
struct PreparedTime {uint64_t phase=0;int32_t mapped=0;float nativeTime=0;bool done=false,segmentLinear=false;};
//Normalize loop/end once, then global084D90 and CVTTSS2SI. No post-remap clamp.
//done carries the native nonloop callback's sticky completion when retained.
PreparedTime prepareTime(const Clip& clip,uint64_t elapsed,bool done=false);
float warpTime(int curve,float phase,float duration);
bool isSelected(const Clip& clip,Field field);
double sample(const Clip& clip,uint64_t elapsed,Field field,double initial,const FrameOverrides* overrides=nullptr);
double samplePrepared(const Clip& clip,const PreparedTime& time,Field field,double initial,const FrameOverrides* overrides=nullptr);
//The caller supplies native fallback state: live graphics for142460, owner D0
//after individual endpoints for a143BD0 clone. Unselected fields are preserved.
//Blend flag and259 offset/divisor are untouched by this native effect producer.
Properties sampleProperties(const Clip& clip,uint64_t elapsed,const Properties& initial);
Properties samplePropertiesPrepared(const Clip& clip,const PreparedTime& time,const Properties& initial);
}
