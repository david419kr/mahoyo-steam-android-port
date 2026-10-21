#include "asset_codecs.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <cstring>
using namespace mahoyo::assets;
int main(int argc,char**argv){try{if(argc<4)return 2;std::ifstream in(argv[2],std::ios::binary);std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),{});Bytes b{raw.data(),raw.size()};std::ofstream out(argv[3],std::ios::binary);std::string mode=argv[1];if(mode=="image"){auto image=decodeImage(b,argc==8?Rect{uint32_t(std::stoul(argv[4])),uint32_t(std::stoul(argv[5])),uint32_t(std::stoul(argv[6])),uint32_t(std::stoul(argv[7]))}:Rect{});uint32_t w=image.width,h=image.height;out.write(reinterpret_cast<char*>(&w),4);out.write(reinterpret_cast<char*>(&h),4);out.write(reinterpret_cast<char*>(image.rgba.data()),std::streamsize(image.rgba.size()));}else{auto decoded=mode=="lenzu"?decodeLenzu(b):decodeMzx(b,mode=="mzx-invert");out.write(reinterpret_cast<char*>(decoded.data()),std::streamsize(decoded.size()));}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
