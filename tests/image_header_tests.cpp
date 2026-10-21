#include "image_header.h"
#include <stdexcept>
#include <vector>
#include <iostream>
int main(){
    auto check=[](bool ok){if(!ok)throw std::runtime_error("Image header contract failed");};
    std::vector<unsigned char> p(32);std::memcpy(p.data(),"\x89PNG\r\n\x1a\n",8);std::memcpy(p.data()+12,"IHDR",4);p[18]=7;p[19]=128;p[22]=4;p[23]=56;
    check(mahoyo::image_header::size(p.data(),24)==std::array<int,2>{1920,1080});check(mahoyo::image_header::size(p.data(),23)==std::array<int,2>{});
    std::fill(p.begin(),p.end(),0);std::memcpy(p.data(),"RIFF",4);std::memcpy(p.data()+8,"WEBPVP8X",8);p[24]=127;p[25]=7;p[27]=55;p[28]=4;
    check(mahoyo::image_header::size(p.data(),30)==std::array<int,2>{1920,1080});check(mahoyo::image_header::size(p.data(),29)==std::array<int,2>{});
    std::memcpy(p.data()+12,"VP8L",4);p[20]=0x2f;uint32_t bits=(1532-1)|((1526-1)<<14);for(int i=0;i<4;i++)p[21+i]=(bits>>(8*i))&255;
    check(mahoyo::image_header::size(p.data(),25)==std::array<int,2>{1532,1526});p[20]=0;check(mahoyo::image_header::size(p.data(),25)==std::array<int,2>{});
    std::memcpy(p.data()+12,"VP8 ",4);p[23]=0x9d;p[24]=1;p[25]=0x2a;p[26]=128;p[27]=7;p[28]=56;p[29]=4;
    check(mahoyo::image_header::size(p.data(),30)==std::array<int,2>{1920,1080});p[28]=p[29]=0;check(mahoyo::image_header::size(p.data(),30)==std::array<int,2>{});
    std::istringstream input("WOH_IMAGE_DIMENSIONS1\nimg0121.mzp\t2552\t2034\n");auto sizes=mahoyo::image_header::dimensions(input);check(sizes.at("img0121.mzp")==std::array<int,2>{2552,2034});
    for(const char* bad:{"bad\n","WOH_IMAGE_DIMENSIONS1\n../bad\t2\t3\n","WOH_IMAGE_DIMENSIONS1\na.mzp\t0\t3\n","WOH_IMAGE_DIMENSIONS1\na.mzp\t2\t65536\n","WOH_IMAGE_DIMENSIONS1\na.mzp\t2\t3\na.mzp\t2\t3\n"}){bool rejected=false;try{std::istringstream stream(bad);mahoyo::image_header::dimensions(stream);}catch(const std::exception&){rejected=true;}check(rejected);}
    std::cout<<"PASS: PNG, extended/lossless/lossy WebP dimensions and truncated headers\n";
}
