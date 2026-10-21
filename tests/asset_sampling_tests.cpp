#include "asset_codecs.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
using namespace mahoyo::assets;
namespace {
void put(std::vector<uint8_t>& b,uint32_t value,int bytes){for(int i=0;i<bytes;++i)b.push_back(uint8_t(value>>(i*8)));}
Bytes view(const std::vector<uint8_t>& b){return {b.data(),b.size()};}
std::vector<uint8_t> fixture(uint32_t nx,uint32_t ny,int seed){
 std::vector<std::vector<uint8_t>> members(1);auto& header=members[0];for(auto n:{nx*16,ny*16,16u,16u,nx,ny,1u})put(header,n,2);put(header,1,1);put(header,1,1);
 for(int i=0;i<256;++i){put(header,uint8_t(i*17),1);put(header,uint8_t(i*31),1);put(header,uint8_t(i*61),1);put(header,i%3==0?0:i%3==1?64:128,1);}
 for(uint32_t tile=0;tile<nx*ny;++tile){std::vector<uint8_t> coded{'M','Z','X','0'};put(coded,256,4);for(int block=0;block<2;++block){coded.push_back(255);for(int i=0;i<128;++i)coded.push_back(uint8_t(seed+tile*23+block*128+i));}members.push_back(std::move(coded));}
 std::vector<uint8_t> out{'m','r','g','d','0','0'},body;put(out,uint32_t(members.size()),2);
 for(auto& member:members){uint32_t offset=uint32_t(body.size()),byte=offset%2048;put(out,offset/2048,2);put(out,byte,2);put(out,(byte+uint32_t(member.size())+2047)/2048,2);put(out,uint32_t(member.size()),2);body.insert(body.end(),member.begin(),member.end());}
 out.insert(out.end(),body.begin(),body.end());return out;
}
Image reference(Bytes primary,Bytes patch,Rect crop,uint32_t step){
 auto base=decodeImage(primary,crop);if(patch.size){auto info=imageInfo(patch);Rect p{crop.left,crop.top,std::min(crop.right,info.width),std::min(crop.bottom,info.height)};if(p.left<p.right&&p.top<p.bottom){auto overlay=decodeImage(patch,p);for(uint32_t y=0;y<overlay.height;++y)for(uint32_t x=0;x<overlay.width;++x){size_t src=(size_t(y)*overlay.width+x)*4,dst=(size_t(y)*base.width+x)*4;if(overlay.rgba[src+3])std::memcpy(base.rgba.data()+dst,overlay.rgba.data()+src,4);}}}
 uint32_t w=uint32_t((uint64_t(base.width)+step-1)/step),h=uint32_t((uint64_t(base.height)+step-1)/step);Image result{w,h,std::vector<uint8_t>(size_t(w)*h*4)};
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){uint32_t l=x*step,t=y*step,r=uint32_t(std::min<uint64_t>(uint64_t(l)+step,base.width)),b=uint32_t(std::min<uint64_t>(uint64_t(t)+step,base.height));uint64_t sums[4]{},straight[3]{},count=uint64_t(r-l)*(b-t);
  for(uint32_t sy=t;sy<b;++sy)for(uint32_t sx=l;sx<r;++sx){auto* p=base.rgba.data()+(size_t(sy)*base.width+sx)*4;sums[3]+=p[3];for(int c=0;c<3;++c){sums[c]+=uint64_t(p[c])*p[3];straight[c]+=p[c];}}
  auto* p=result.rgba.data()+(size_t(y)*w+x)*4;for(int c=0;c<3;++c)p[c]=uint8_t(sums[3]?(sums[c]+sums[3]/2)/sums[3]:(straight[c]+count/2)/count);p[3]=uint8_t((sums[3]+count/2)/count);
 }return result;
}
std::vector<uint8_t> cbgFixture(){
 const char magic[]="CompressedBG_MT";std::vector<uint8_t> out(magic,magic+16);put(out,2,4);put(out,301,4);put(out,1,4);put(out,8,4);out.resize(48);
 for(uint32_t i=0;i<301;++i)put(out,48+301*4+i*261,4);
 for(int i=0;i<301;++i){put(out,2,4);for(int symbol=0;symbol<256;++symbol)put(out,symbol==0||symbol==2?1:0,1);put(out,2,1);}return out;
}
}
int main(){try{auto primary=fixture(4,40,0),patch=fixture(2,20,91);auto size=imageInfo(view(primary));int checks=0;
 for(Bytes overlay:{Bytes{nullptr,0},view(patch)})for(Rect crop:{Rect{0,0,size.width,size.height},Rect{3,5,size.width-2,size.height-3},Rect{35,333,49,520}})for(uint32_t step:{1u,2u,3u,8u,127u,65535u,std::numeric_limits<uint32_t>::max()}){
  auto expected=reference(view(primary),overlay,crop,step),actual=decodeComposed(view(primary),overlay,crop,step);
  if(actual.width!=expected.width||actual.height!=expected.height||actual.rgba!=expected.rgba)throw Error("Strip sampler changed pixels/alpha");++checks;
 }
 bool rejected=false;try{decodeComposed(view(primary),{}, {},0);}catch(const Error&){rejected=true;}if(!rejected)throw Error("Zero sample accepted");++checks;
 rejected=false;try{decodeComposed(view(primary),{}, {0,0,size.width+1,size.height},2);}catch(const Error&){rejected=true;}if(!rejected)throw Error("Invalid crop accepted");++checks;
 auto whole=decodeComposed(view(primary),{});if(whole.rgba!=decodeImage(view(primary)).rgba)throw Error("Default crop changed");++checks;
 auto cbg=cbgFixture();for(uint32_t step:{1u,2u,3u,127u,65535u}){auto actual=decodeComposed(view(cbg),{}, {},step),expected=reference(view(cbg),{}, {0,0,2,301},step);if(actual.width!=expected.width||actual.height!=expected.height||actual.rgba!=expected.rgba)throw Error("CBG strip sampling changed pixels");++checks;}
 std::cout<<"bounded composition sampling: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
