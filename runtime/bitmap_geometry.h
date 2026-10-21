#pragma once
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

//Exact bitmap metadata and pure geometry transforms; Runtime owns WH integration.
namespace mahoyo::bitmap_geometry {
struct Size {int32_t width,height;bool operator==(const Size& b)const{return width==b.width&&height==b.height;}};
struct Recipe {Size patch,primary;};
using Recipes=std::map<std::string,Recipe>;
using SavedSizes=std::map<int,Size>;
struct Resize {Size before,after;};
inline Size checkedSize(double w,double h){
    if(!std::isfinite(w)||!std::isfinite(h)||w<1||h<1||w>INT32_MAX||h>INT32_MAX||w!=std::floor(w)||h!=std::floor(h))throw std::runtime_error("Invalid bitmap intrinsic dimensions");
    return {int32_t(w),int32_t(h)};
}
inline uint64_t decimal(const std::string& s,uint64_t maximum){
    if(s.empty()||s.size()>10||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid bitmap geometry integer");
    const auto value=std::stoull(s);if(value>maximum)throw std::runtime_error("Bitmap geometry integer out of range");return value;
}
inline bool fullKey(const std::string& key){
    return key.size()>4&&key.size()<=4096&&key.compare(key.size()-4,4,".mzp")==0&&key.find("..") == std::string::npos&&key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string::npos;
}
inline void lineEnd(std::istream& row){row>>std::ws;if(!row.eof())throw std::runtime_error("Trailing bitmap geometry fields");}
inline Recipes readRecipes(std::istream& input,const std::string& expectedPE,const std::string& expectedAuthority){
    std::string line,magic,pe,authority,count;
    if(!std::getline(input,line))throw std::runtime_error("Missing bitmap geometry metadata");
    std::istringstream head(line);if(!(head>>magic>>pe>>authority>>count)||magic!="WOHCOMPOSITEGEOMETRY1"||pe!=expectedPE||authority!=expectedAuthority)throw std::runtime_error("Bitmap geometry authority mismatch");lineEnd(head);
    const auto n=decimal(count,3818);Recipes result;std::string previous;
    for(uint64_t i=0;i<n;++i){
        if(!std::getline(input,line))throw std::runtime_error("Truncated bitmap geometry metadata");
        std::istringstream row(line);row>>std::ws;std::string key,w,h,nw,nh;if(row.peek()!='"'||!(row>>std::quoted(key)>>w>>h>>nw>>nh)||!fullKey(key)||(!previous.empty()&&key<=previous))throw std::runtime_error("Invalid/duplicate/unsorted bitmap geometry key");lineEnd(row);
        Recipe r{checkedSize(decimal(w,INT32_MAX),decimal(h,INT32_MAX)),checkedSize(decimal(nw,INT32_MAX),decimal(nh,INT32_MAX))};result.emplace(key,r);previous=std::move(key);
    }
    input>>std::ws;if(!input.eof())throw std::runtime_error("Trailing bitmap geometry metadata");return result;
}
//A missing table/key preserves the entire old path. In particular, no suffix
//stripping, single->base recipe inheritance or normalized-center guess occurs.
inline std::optional<Resize> plan(const Recipes* recipes,const std::string& exactKey,std::optional<Size> savedIntrinsic,Size currentIntrinsic,bool nativeLayoutPresent){
    if(!recipes)return std::nullopt;auto at=recipes->find(exactKey);if(at==recipes->end())return std::nullopt;
    if(!nativeLayoutPresent)throw std::runtime_error("Pair migration lacks exact native image layout");
    checkedSize(currentIntrinsic.width,currentIntrinsic.height);const auto& r=at->second;Size old=savedIntrinsic?*savedIntrinsic:r.patch;checkedSize(old.width,old.height);
    if(!(old==r.patch)&&!(old==r.primary))throw std::runtime_error("Unrecognized saved pair intrinsic dimensions");
    if(!(currentIntrinsic==r.patch)&&!(currentIntrinsic==r.primary))throw std::runtime_error("Pair metadata/current bitmap mismatch");
    if(old==currentIntrinsic)return std::nullopt;
    if(!(old==r.patch)||!(currentIntrinsic==r.primary))throw std::runtime_error("Unsupported pair bitmap downgrade");
    return Resize{old,currentIntrinsic};
}
//Only the four dimension-dependent values change. Caller keeps XY, rotation,
//drawOffset, uniform/axis scales, crop, Native origin/window and clocks intact.
inline void apply(const Resize& r,double& width,double& height,double& pivotX,double& pivotY){
    checkedSize(r.before.width,r.before.height);checkedSize(r.after.width,r.after.height);
    if(r.before==r.after)return;
    if(!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(pivotX)||!std::isfinite(pivotY))throw std::runtime_error("Nonfinite bitmap migration input");
    const double w=width*(double(r.after.width)/r.before.width),h=height*(double(r.after.height)/r.before.height),px=pivotX*(double(r.before.width)/r.after.width),py=pivotY*(double(r.before.height)/r.after.height);
    if(!std::isfinite(w)||!std::isfinite(h)||!std::isfinite(px)||!std::isfinite(py))throw std::runtime_error("Bitmap migration overflow");
    width=w;height=h;pivotX=px;pivotY=py;
}
inline SavedSizes readSavedSizes(std::istream& input,const std::set<int>& layerIds){
    if(layerIds.size()>64)throw std::runtime_error("Too many saved bitmap layers");for(int id:layerIds)if(id<0||id>=64)throw std::runtime_error("Invalid bitmap layer id");
    std::string line,tag,count;if(!std::getline(input>>std::ws,line))throw std::runtime_error("Missing BITMAPGEOMETRY1");std::istringstream head(line);
    if(!(head>>tag>>count)||tag!="BITMAPGEOMETRY1"||decimal(count,64)!=layerIds.size())throw std::runtime_error("Invalid saved bitmap layer count");lineEnd(head);
    SavedSizes result;
    for(size_t i=0;i<layerIds.size();++i){
        if(!std::getline(input,line))throw std::runtime_error("Truncated saved bitmap geometry");std::istringstream row(line);std::string id,w,h;
        if(!(row>>id>>w>>h))throw std::runtime_error("Invalid saved bitmap geometry row");lineEnd(row);const auto n=int(decimal(id,63));
        if(!layerIds.count(n)||!result.emplace(n,checkedSize(decimal(w,INT32_MAX),decimal(h,INT32_MAX))).second)throw std::runtime_error("Duplicate/unknown saved bitmap layer");
    }
    return result; //Root common reader owns the final EOF after all19 tags.
}
inline void writeSavedSizes(std::ostream& out,const SavedSizes& values){
    if(values.size()>64)throw std::runtime_error("Too many saved bitmap layers");for(const auto& p:values){if(p.first<0||p.first>=64)throw std::runtime_error("Invalid saved bitmap layer");checkedSize(p.second.width,p.second.height);}
    out<<"BITMAPGEOMETRY1 "<<values.size()<<'\n';for(const auto& p:values)out<<p.first<<' '<<p.second.width<<' '<<p.second.height<<'\n';
}
}
