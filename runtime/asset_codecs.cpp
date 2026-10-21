#include "asset_codecs.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <queue>
#include <set>
#include <tuple>

namespace mahoyo::assets {
namespace {
constexpr uint64_t MaxBytes=256ULL*1024*1024,MaxPixels=64ULL*1024*1024;
void require(bool ok,const char* message){if(!ok)throw Error(message);}
void allocation(uint64_t size,uint64_t cap=MaxBytes){require(size<=cap,"Asset allocation budget exceeded");}
uint32_t le(Bytes b,size_t at,size_t n=4){auto p=b.slice(at,n).data;uint32_t v=0;for(size_t i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;}
bool prefix(Bytes b,const char* text,size_t n){return b.size>=n&&!std::memcmp(b.data,text,n);}
struct Reader {
 Bytes bytes;size_t pos=0;
 Bytes take(size_t n){auto b=bytes.slice(pos,n);pos+=n;return b;}
 uint32_t integer(size_t n=4){auto b=take(n);return le(b,0,n);}
 uint64_t varint(){uint64_t value=0;for(int s=0;s<35;s+=7){auto c=integer(1);value|=uint64_t(c&127)<<s;if(!(c&128))return value;}throw Error("Asset varint exceeds 35 bits");}
};
struct Bits {
 Reader& source;bool msb;uint8_t current=0;int remaining=0;
 uint32_t read(int count=1){uint32_t result=0;for(int shift=0;shift<count;++shift){if(!remaining){current=uint8_t(source.integer(1));remaining=8;}--remaining;if(msb)result=(result<<1)|((current>>remaining)&1);else{result|=uint32_t(current&1)<<shift;current>>=1;}}return result;}
};
struct Node {int left=-1,right=-1,symbol=-1;};
struct Tree {
 std::vector<Node> nodes;int root;
 Tree(const std::vector<std::pair<uint32_t,uint64_t>>& weights,bool inverted=false){
  using Item=std::tuple<uint64_t,size_t,int>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
  size_t serial=weights.size();
  for(size_t i=0;i<weights.size();++i)if(weights[i].second){int index=int(nodes.size());nodes.push_back({-1,-1,int(weights[i].first)});queue.emplace(weights[i].second,i,index);}
  require(!queue.empty(),"Empty Huffman alphabet");
  if(queue.size()==1){auto leaf=std::get<2>(queue.top());nodes.push_back(inverted?Node{-1,leaf,-1}:Node{leaf,-1,-1});root=int(nodes.size()-1);return;}
  while(queue.size()>1){auto a=queue.top();queue.pop();auto b=queue.top();queue.pop();int first=std::get<2>(a),second=std::get<2>(b),index=int(nodes.size());nodes.push_back(inverted?Node{second,first,-1}:Node{first,second,-1});queue.emplace(std::get<0>(a)+std::get<0>(b),serial++,index);}
  root=std::get<2>(queue.top());
 }
 uint32_t symbol(Bits& bits)const{int at=root;while(nodes[size_t(at)].symbol<0){auto& n=nodes[size_t(at)];at=bits.read()?n.right:n.left;require(at>=0,"Invalid Huffman branch");}return uint32_t(nodes[size_t(at)].symbol);}
};
void backcopy(std::vector<uint8_t>& out,size_t distance,size_t count){require(distance&&distance<=out.size(),"Back-reference before output");allocation(uint64_t(out.size())+count+128);auto start=out.size()-distance;for(size_t i=0;i<count;++i)out.push_back(out[start+i]);}
std::vector<Bytes> mzpMembers(Bytes data){
 Reader r{data};require(prefix(r.take(6),"mrgd00",6),"Not an MZP image");auto count=r.integer(2);auto table=r.take(size_t(count)*8);size_t base=r.pos;std::vector<Bytes> members;members.reserve(count);
 for(size_t i=0;i<count;++i){size_t at=i*8;uint64_t sector=le(table,at,2),byte=le(table,at+2,2),span=le(table,at+4,2),low=le(table,at+6,2);require(byte<2048,"Invalid MZP byte offset");uint64_t lower=span?uint64_t(std::max<int64_t>(0,(int64_t(span)-1)*2048-int64_t(byte)+1)):0,upper=span?span*2048-byte:0;uint64_t size=low+(lower>low?(lower-low+65535)/65536:0)*65536;require(size>=lower&&size<=upper,"Invalid MZP sector length");members.push_back(data.slice(base+sector*2048+byte,size));}
 return members;
}
struct Mzp {
 std::vector<Bytes> members;uint32_t width,height,tw,th,nx,ny,kind,depth,border;
 explicit Mzp(Bytes data):members(mzpMembers(data)){
  require(!members.empty()&&members[0].size>=16,"Missing MZP descriptor");auto b=members[0];uint32_t w=le(b,0,2),h=le(b,2,2);tw=le(b,4,2);th=le(b,6,2);nx=le(b,8,2);ny=le(b,10,2);kind=le(b,12,2);depth=le(b,14,1);border=le(b,15,1);
  require(w&&h&&tw&&th&&nx&&ny&&2*border<std::min(tw,th),"Invalid MZP geometry");require(uint64_t(nx)*ny+1==members.size(),"MZP tile count mismatch");require(uint64_t(2)*border*(nx-1)<w&&uint64_t(2)*border*(ny-1)<h,"MZP borders consume output");width=w-2*border*(nx-1);height=h-2*border*(ny-1);allocation(uint64_t(width)*height,MaxPixels);allocation(uint64_t(tw)*th*4);
  require((kind==1&&(depth==1||depth==0x11||depth==0x91))||(kind==0xb&&depth==0x14)||(kind==0xc&&depth==0x11),"Unsupported MZP mode");if(kind==1)require(b.size>=1040,"Truncated MZP palette");
 }
};
Rect cropRect(uint32_t w,uint32_t h,Rect p){if(!p.right&&!p.bottom)p={0,0,w,h};require(p.left<p.right&&p.top<p.bottom&&p.right<=w&&p.bottom<=h,"Asset crop outside source");return p;}
Image canvas(Rect p){uint32_t w=p.right-p.left,h=p.bottom-p.top;allocation(uint64_t(w)*h,MaxPixels);return {w,h,std::vector<uint8_t>(size_t(w)*h*4)};}
using Palette=std::array<std::array<uint8_t,4>,256>;
Palette palette(Bytes data,bool swizzle=false){require(data.size>=1024,"Truncated image palette");Palette result{};for(size_t i=0;i<256;++i){for(int c=0;c<3;++c)result[i][size_t(c)]=data.data[i*4+size_t(c)];auto a=data.data[i*4+3];result[i][3]=a>=128?255:uint8_t((a<<1)|(a>>6));}if(swizzle)for(size_t i=0;i<256;i+=32)for(size_t j=0;j<8;++j)std::swap(result[i+8+j],result[i+16+j]);return result;}
Image decodeMzp(Bytes data,Rect area){
 Mzp m(data);area=cropRect(m.width,m.height,area);auto out=canvas(area);Palette global{};if(m.kind==1)global=palette(m.members[0].slice(16,1024),m.depth!=1);
 for(size_t i=0;i<size_t(m.nx)*m.ny;++i){
  uint32_t x=uint32_t(i%m.nx),y=uint32_t(i/m.nx),sx=x?m.border:0,sy=y?m.border:0;uint64_t dx=uint64_t(x)*(m.tw-2*m.border)+sx,dy=uint64_t(y)*(m.th-2*m.border)+sy;require(dx<=m.width&&dy<=m.height,"MZP tile placement outside source");uint32_t cw=std::min<uint32_t>(m.tw-sx-(x<m.nx-1?m.border:0),m.width-uint32_t(dx)),ch=std::min<uint32_t>(m.th-sy-(y<m.ny-1?m.border:0),m.height-uint32_t(dy));
  uint32_t left=std::max<uint32_t>(uint32_t(dx),area.left),top=std::max<uint32_t>(uint32_t(dy),area.top),right=std::min<uint32_t>(uint32_t(dx)+cw,area.right),bottom=std::min<uint32_t>(uint32_t(dy)+ch,area.bottom);if(left>=right||top>=bottom)continue;
  auto raw=decodeMzx(m.members[i+1]);Bytes b{raw.data(),raw.size()};size_t pixels=size_t(m.tw)*m.th;Palette colors=global;size_t indices=0;
  if(m.kind==1)require(raw.size()==pixels,"Indexed tile length mismatch");
  else if(m.kind==0xc){require(raw.size()>=32&&le(b,0)==0x00504548&&le(b,20)==m.tw&&le(b,24)==m.th&&le(b,4)==32+pixels+1024&&raw.size()==32+pixels+1024,"Invalid HEP tile");colors=palette(b.slice(32+pixels,1024));indices=32;}
  else require(raw.size()==pixels*4,"Truecolor tile length mismatch");
  for(uint32_t py=top;py<bottom;++py)for(uint32_t px=left;px<right;++px){size_t source=size_t(sy+py-uint32_t(dy))*m.tw+sx+px-uint32_t(dx),dest=(size_t(py-area.top)*out.width+px-area.left)*4;auto* p=out.rgba.data()+dest;if(m.kind==1||m.kind==0xc)std::memcpy(p,colors[raw[indices+source]].data(),4);else{auto packed=le(b,source*2,2);auto fine=raw[pixels*2+source];p[0]=uint8_t(((packed&0xf800)>>8)|(fine>>5));p[1]=uint8_t(((packed&0x07e0)>>3)|((fine>>3)&3));p[2]=uint8_t(((packed&0x001f)<<3)|(fine&7));p[3]=raw[pixels*3+source];}}
 }
 return out;
}
Image decodeCbg(Bytes data,Rect area){
 Reader r{data};require(prefix(r.take(16),"CompressedBG_MT\0",16),"Not a CBG image");uint32_t width=r.integer(),height=r.integer(),stripe=r.integer(),bpp=r.integer();require(width&&height&&stripe&&(bpp==8||bpp==24||bpp==32),"Invalid CBG geometry");allocation(uint64_t(width)*height,MaxPixels);area=cropRect(width,height,area);auto out=canvas(area);r.take(16);uint32_t count=uint32_t((uint64_t(height)+stripe-1)/stripe),channels=bpp/8;std::vector<size_t> offsets;offsets.reserve(size_t(count)+1);for(uint32_t i=0;i<count;++i)offsets.push_back(r.integer());offsets.push_back(data.size);require(offsets[0]>=r.pos,"CBG stripe overlaps index");
 for(uint32_t i=0;i<count;++i){require(offsets[i]<=offsets[i+1]&&offsets[i+1]<=data.size,"CBG stripe outside source");uint32_t rows=std::min(stripe,height-i*stripe);if(uint64_t(i)*stripe+rows<=area.top||uint64_t(i)*stripe>=area.bottom)continue;
  Reader part{data.slice(offsets[i],offsets[i+1]-offsets[i])};auto coded=part.integer();uint64_t expected=uint64_t(rows)*width*channels;allocation(expected);require(coded<=expected*2+16,"CBG coded stripe budget");std::vector<std::pair<uint32_t,uint64_t>> weights;weights.reserve(256);for(uint32_t n=0;n<256;++n)weights.emplace_back(n,part.varint());Tree tree(weights);Bits bits{part,false};std::vector<uint8_t> runs;allocation(coded);runs.reserve(coded);for(uint32_t n=0;n<coded;++n)runs.push_back(uint8_t(tree.symbol(bits)));Reader run{{runs.data(),runs.size()}};std::vector<uint8_t> block;block.reserve(size_t(expected));bool zero=false;
  while(run.pos<runs.size()){auto n=run.varint();require(n<=expected-block.size(),"CBG run exceeds stripe");if(zero)block.insert(block.end(),size_t(n),0);else{auto literal=run.take(size_t(n));block.insert(block.end(),literal.data,literal.data+literal.size);}zero=!zero;}require(block.size()==expected,"CBG stripe length mismatch");
  for(uint32_t y=0;y<rows;++y)for(uint32_t x=0;x<width;++x)for(uint32_t c=0;c<channels;++c){if(!x&&!y)continue;auto at=(size_t(y)*width+x)*channels+c;uint32_t prediction;if(x&&y)prediction=(uint32_t(block[at-width*channels])+block[at-channels])/2;else prediction=block[(size_t(y?y-1:0)*width+(x?x-1:0))*channels+c];block[at]=uint8_t(block[at]+prediction);}
  uint32_t top=std::max(area.top,i*stripe),bottom=std::min(area.bottom,i*stripe+rows);for(uint32_t y=top;y<bottom;++y)for(uint32_t x=area.left;x<area.right;++x){auto at=(size_t(y-i*stripe)*width+x)*channels,dest=(size_t(y-area.top)*out.width+x-area.left)*4;if(channels==1){out.rgba[dest]=out.rgba[dest+1]=out.rgba[dest+2]=block[at];out.rgba[dest+3]=255;}else{out.rgba[dest]=block[at+2];out.rgba[dest+1]=block[at+1];out.rgba[dest+2]=block[at];out.rgba[dest+3]=channels==4?block[at+3]:255;}}
 }
 return out;
}
}
Bytes Bytes::slice(size_t offset,size_t length)const{if(offset>size||length>size-offset)throw Error("Truncated asset range");return {data+offset,length};}
std::vector<uint8_t> decodeMzx(Bytes data,bool invert){
 Reader r{data};require(prefix(r.take(4),"MZX0",4),"Not MZX0");auto length=r.integer();allocation(length);std::vector<uint8_t> out;out.reserve(size_t(length)+128);std::array<uint16_t,64> ring{};if(invert)ring.fill(0xffff);size_t ringPos=0;int reset=0;
 while(out.size()<length){auto flags=r.integer(1);uint32_t command=flags&3,argument=flags>>2,count=command==2?1:argument+1;if(reset<=0)reset=4096;if(command==0){uint16_t previous=reset==4096?(invert?0xffff:0):uint16_t(le({out.data(),out.size()},out.size()-2,2));for(uint32_t n=0;n<count;++n){out.push_back(uint8_t(previous));out.push_back(uint8_t(previous>>8));}}
  else if(command==1)backcopy(out,2*(r.integer(1)+1),count*2);
  else if(command==2){auto v=ring[argument];out.push_back(uint8_t(v));out.push_back(uint8_t(v>>8));}
  else{auto literal=r.take(count*2);for(uint32_t n=0;n<count;++n){auto v=uint16_t(le(literal,n*2,2));if(invert)v^=0xffff;out.push_back(uint8_t(v));out.push_back(uint8_t(v>>8));ring[ringPos]=v;ringPos=(ringPos+1)&63;}}reset-=int(count);
 }
 out.resize(length);return out;
}
std::vector<uint8_t> decodeLenzu(Bytes data){
 Reader r{data};std::array<uint8_t,32> magic{};std::memcpy(magic.data(),"LenZuCompressor",15);magic[16]='1';magic[20]='0';auto sig=r.take(32);require(!std::memcmp(sig.data,magic.data(),32),"Unsupported LenZu signature");uint32_t length=r.integer(),high=r.integer(),low=r.integer();r.integer();allocation(length);r.integer(1);auto rawBits=r.integer(1),minBits=r.integer(1),upper=r.integer(1),lowBits=r.integer(1),base=r.integer(1),alphabetBits=std::max(rawBits,minBits);require(rawBits>=3&&rawBits<16&&minBits>=3&&minBits<16&&lowBits<upper&&upper<16&&alphabetBits>=upper-lowBits&&base>=2&&base<9,"Unsupported LenZu options");uint32_t alphabet=1u<<alphabetBits,count=r.integer(1);if(!count)count=alphabet;require(count<=alphabet,"LenZu alphabet count exceeds range");std::vector<std::pair<uint32_t,uint64_t>> weights;std::set<uint32_t> symbols;
 if(uint64_t(alphabet)*4<uint64_t((alphabetBits+7)/8+4)*count)for(uint32_t n=0;n<alphabet;++n)weights.emplace_back(n,r.integer());
 else for(uint32_t n=0;n<count;++n){auto symbol=r.integer(1),weight=r.integer();require(symbol<alphabet&&symbols.insert(symbol).second,"Invalid LenZu symbol");weights.emplace_back(symbol,weight);}Tree tree(weights,true);Bits bits{r,true};std::vector<uint8_t> out;out.reserve(length);
 while(out.size()<length){bool reference=bits.read()!=0;uint32_t n=tree.symbol(bits);if(reference){n+=base;uint32_t distance=(tree.symbol(bits)<<lowBits)|bits.read(int(lowBits));distance+=base;require(n<=length-out.size(),"LenZu reference exceeds output");backcopy(out,distance,n);}else{++n;require(n<=length-out.size(),"LenZu literal exceeds output");while(n--)out.push_back(uint8_t(bits.read(8)));}}
 uint64_t crc=0;constexpr uint64_t multipliers[]{233,277,311,433};for(size_t i=0;i<out.size();++i)crc=(crc+out[i])*multipliers[i&3];require(crc==((uint64_t(high)<<32)|low),"LenZu checksum mismatch");return out;
}
ImageInfo imageInfo(Bytes data){if(prefix(data,"mrgd00",6)){Mzp m(data);return {m.width,m.height};}if(prefix(data,"CompressedBG_MT\0",16)){auto w=le(data,16),h=le(data,20),stripe=le(data,24),depth=le(data,28);require(w&&h&&stripe&&(depth==8||depth==24||depth==32),"Invalid CBG geometry");allocation(uint64_t(w)*h,MaxPixels);return {w,h};}throw Error("Unknown native image format");}
Image decodeImage(Bytes data,Rect crop){if(prefix(data,"mrgd00",6))return decodeMzp(data,crop);if(prefix(data,"CompressedBG_MT\0",16))return decodeCbg(data,crop);throw Error("Unknown native image format");}
Image decodeComposed(Bytes primary,Bytes patch,Rect crop,uint32_t sample){
 require(sample>0,"Invalid native image sample");auto size=imageInfo(primary);crop=cropRect(size.width,size.height,crop);
 ImageInfo patchSize{};if(patch.size)patchSize=imageInfo(patch);
 auto strip=[&](Rect area){
  auto image=decodeImage(primary,area);
  Rect p{area.left,area.top,std::min(area.right,patchSize.width),std::min(area.bottom,patchSize.height)};
  if(patch.size&&p.left<p.right&&p.top<p.bottom){auto overlay=decodeImage(patch,p);for(uint32_t y=0;y<overlay.height;++y)for(uint32_t x=0;x<overlay.width;++x){auto src=(size_t(y)*overlay.width+x)*4,dst=(size_t(y)*image.width+x)*4;if(overlay.rgba[src+3])std::memcpy(image.rgba.data()+dst,overlay.rgba.data()+src,4);}}
  return image;
 };
 if(sample==1)return strip(crop);
 uint32_t width=crop.right-crop.left,height=crop.bottom-crop.top,w=uint32_t((uint64_t(width)+sample-1)/sample),h=uint32_t((uint64_t(height)+sample-1)/sample);
 auto out=canvas({0,0,w,h});
 // Carry one output row across bounded source strips; never allocate a full atlas before sampling.
 using Sum=std::array<uint64_t,7>;allocation(uint64_t(w)*sizeof(Sum),16ULL*1024*1024);std::vector<Sum> sums(w);
 allocation(uint64_t(width)*4,16ULL*1024*1024);uint32_t rows=uint32_t(16ULL*1024*1024/(uint64_t(width)*4)),stride=128,first=128;
 if(prefix(primary,"mrgd00",6)){Mzp m(primary);stride=m.th-2*m.border;first=m.th-(m.ny>1?m.border:0);}else if(prefix(primary,"CompressedBG_MT\0",16)){stride=le(primary,24);first=stride;}
 for(uint32_t top=0;top<height;){uint64_t sourceTop=uint64_t(crop.top)+top,next=sourceTop<first?first:uint64_t(first)+((sourceTop-first)/stride+1)*stride;uint32_t bottom=top+uint32_t(std::min<uint64_t>({rows,height-top,next-sourceTop}));auto block=strip({crop.left,crop.top+top,crop.right,crop.top+bottom});
  for(uint32_t y=top;y<bottom;++y){
   for(uint32_t x=0;x<w;++x){auto& sum=sums[x];uint32_t left=uint32_t(uint64_t(x)*sample),right=uint32_t(std::min<uint64_t>(uint64_t(left)+sample,width));for(uint32_t sx=left;sx<right;++sx){auto* p=block.rgba.data()+(size_t(y-top)*width+sx)*4;sum[3]+=p[3];for(int c=0;c<3;++c){sum[size_t(c)]+=uint64_t(p[c])*p[3];sum[size_t(c)+4]+=p[c];}}}
   if((uint64_t(y)+1)%sample==0||y+1==height){uint32_t oy=y/sample;uint64_t countY=uint64_t(y)+1-uint64_t(oy)*sample;
    for(uint32_t x=0;x<w;++x){auto& sum=sums[x];uint64_t count=countY*std::min<uint64_t>(sample,uint64_t(width)-uint64_t(x)*sample);auto* p=out.rgba.data()+(size_t(oy)*w+x)*4;for(int c=0;c<3;++c)p[c]=uint8_t(sum[3]?(sum[size_t(c)]+sum[3]/2)/sum[3]:(sum[size_t(c)+4]+count/2)/count);p[3]=uint8_t((sum[3]+count/2)/count);sum.fill(0);}
   }
  }top=bottom;
 }return out;
}
std::vector<Entry> hfaIndex(Bytes index,uint64_t fileSize){
 require(prefix(index,"HUNEXGGEFA10",12),"Not HFA");auto count=le(index,12);uint64_t start=16+uint64_t(count)*128;require(start<=fileSize&&uint64_t(count)*128<=64ULL*1024*1024&&start<=index.size,"Invalid HFA index size");std::vector<Entry> out;out.reserve(count);std::vector<std::pair<uint64_t,uint64_t>> ranges;
 for(uint32_t i=0;i<count;++i){auto row=index.slice(16+size_t(i)*128,128);auto end=static_cast<const uint8_t*>(std::memchr(row.data,0,96));require(end&&end>row.data,"Invalid HFA resource name");std::string name(reinterpret_cast<const char*>(row.data),size_t(end-row.data));require(name.find_first_of("/\\:\r\n")==std::string::npos&&name!="."&&name!="..","Unsafe HFA resource name");uint64_t offset=start+le(row,96),size=le(row,100);require(offset<=fileSize&&size<=fileSize-offset,"HFA resource outside file");out.push_back({std::move(name),offset,size});if(size)ranges.emplace_back(offset,offset+size);}
 std::sort(ranges.begin(),ranges.end());uint64_t last=start;for(auto range:ranges){require(range.first>=last,"Overlapping HFA resources");last=range.second;}return out;
}
}
