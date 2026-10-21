#include "packed_assets.h"
#include <iostream>
#include <sstream>
#include <functional>
using mahoyo::packed::Assets;
int main(){
    auto check=[](bool value){if(!value)throw std::runtime_error("Packed asset contract failed");};
    check(mahoyo::packed::fits(8589934592ULL,24,9000000000ULL));
    check(!mahoyo::packed::fits(UINT64_MAX,2,UINT64_MAX));
    check(!mahoyo::packed::fits(9,2,10));
    Assets good;std::istringstream input("WOHOBB1\nassets.json\t64\t2\t12345678\nimages/a.png\t8589934592\t24\t00000000\n");good.parse(input,9000000000ULL);check(good.contains("images/a.png"));
    for(const char* bad:{"WOHOBB2\n", "WOHOBB1\n../a\t1\t1\t00000000\n", "WOHOBB1\nassets.json\t-1\t2\t00000000\n", "WOHOBB1\nassets.json\t0\t12\t00000000\n", "WOHOBB1\nassets.json\t1\t2\t00000000\nassets.json\t4\t2\t00000000\n", "WOHOBB1\nassets.json\t1\t4\t00000000\nimages/a.png\t2\t1\t00000000\n", "WOHOBB1\nassets.json\t9223372036854775808\t1\t00000000\n"}){
        bool rejected=false;try{Assets value;std::istringstream stream(bad);value.parse(stream,10);}catch(const std::exception&){rejected=true;}check(rejected);
    }
    std::cout<<"PASS: OBB 64-bit offsets, overflow, bounds, traversal, duplicates and overlap\n";
}
