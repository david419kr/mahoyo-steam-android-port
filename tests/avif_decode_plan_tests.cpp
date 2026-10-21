#include "avif_decode_plan.h"
#include <iostream>
#include <climits>
int main(){
    using namespace mahoyo::avif_plan;
    auto check=[](bool x){if(!x)throw std::runtime_error("AVIF decode plan failed");};
    auto p=make(3062,4320,1,false,0,0,0,0);check(p.width==3062&&p.height==4320);
    p=make(5102,8672,1,false,0,0,0,0);check(uint64_t(p.width)*p.height==44244544);
    p=make(3062,4320,4,true,101,201,501,601);check(p.width==766&&p.height==1080&&p.left==25&&p.top==50&&p.right==126&&p.bottom==151);
    p=make(3062,4320,INT_MAX,false,0,0,0,0);check(p.width==1&&p.height==1);
    for(int mode=0;mode<5;mode++){bool rejected=false;try{if(mode==0)make(20000,1,1,false,0,0,0,0);if(mode==1)make(16384,16384,1,false,0,0,0,0);if(mode==2)make(4096,4096,1,true,0,0,4096,4096);if(mode==3)make(300,400,1,true,0,0,301,20);if(mode==4)make(300,400,0,false,0,0,0,0);}catch(const std::exception&){rejected=true;}check(rejected);}
    std::cout<<"PASS: AVIF13/44MP, sample/crop bounds, overflow and pixel budgets\n";
}
