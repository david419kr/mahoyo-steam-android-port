#include "text.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mahoyo::text {
namespace {
bool digit(char value){return value>='0'&&value<='9';}
struct StyleControl {size_t end;int italic;};
StyleControl inlineStyle(const std::string& input,size_t at){
    if(input.compare(at,2,"@i")==0)return {at+2,1};
    if(input.compare(at,2,"@r")==0)return {at+2,0};
    if(input.compare(at,2,"@z")==0)return {at+2,-1};
    if(at+2<input.size()&&((input.compare(at,2,"@a")==0&&(input[at+2]=='1'||input[at+2]=='2'))||(input.compare(at,2,"@c")==0&&digit(input[at+2]))))return {at+3,-1};
    if(input[at]!='<')return {at,-1};
    auto close=input.find('>',at);if(close==std::string::npos)return {at,-1};
    std::string tag=input.substr(at+1,close-at-1);
    if(tag.find('|')!=std::string::npos)return {at,-1}; // Raw ruby is compiled before body controls.
    for(char& ch:tag)if(ch>='A'&&ch<='Z')ch=char(ch-'A'+'a');
    // Native markup dispatch matches these names by prefix after lowercasing.
    if(tag.compare(0,1,"i")==0)return {close+1,1};
    if(tag.compare(0,2,"/i")==0||tag.compare(0,2,"fr")==0)return {close+1,0};
    return {at,-1};
}
}
std::string normalizeCtdLine(std::string input){
    std::replace(input.begin(),input.end(),'^','\n');return input;
}
Resolution resolveReferences(const std::string& input,const std::vector<std::string>& table){
    Resolution result;
    size_t at=0,literal=0;
    while(at<input.size()){
        if(input[at]!='$'||input.size()-at<7){++at;continue;}
        int id=0;bool valid=true;
        for(size_t offset=1;offset<=6;++offset){const char value=input[at+offset];if(!digit(value)){valid=false;break;}id=id*10+(value-'0');}
        if(!valid||(input.size()-at>7&&digit(input[at+7]))){++at;continue;}
        if(size_t(id)>=table.size())throw std::out_of_range("CTD reference "+input.substr(at,7)+" (ID "+std::to_string(id)+") exceeds table size "+std::to_string(table.size()));
        result.text.append(input,literal,at-literal);
        result.text.append(table[size_t(id)]);
        result.lastId=id;
        at+=7;literal=at;
    }
    result.text.append(input,literal,input.size()-literal);
    return result;
}
std::string stripDisplayControls(const std::string& input){
    std::string result;result.reserve(input.size());
    const std::string dummy="＆ダミー＆";
    size_t at=input.compare(0,dummy.size(),dummy)==0?dummy.size():0;
    while(at<input.size()){
        if(input.compare(at,2,"@@")==0){result.append(input,at,2);at+=2;}
        else if(input.compare(at,2,"@x")==0)at+=2;
        else result+=input[at++];
    }
    return result;
}
std::string compactDialogueSpacing(const std::string& input,const std::string& preceding){
    const auto punctuation=[](const std::string& value,size_t at){return value.compare(at,3,"、")==0||value.compare(at,3,"。")==0;};
    bool after=preceding.size()>=3&&punctuation(preceding,preceding.size()-3);
    std::string result;result.reserve(input.size());
    for(size_t at=0;at<input.size();){
        if(after&&input.compare(at,3,"　")==0){at+=3;continue;}
        const auto style=inlineStyle(input,at);
        if(style.end!=at){result.append(input,at,style.end-at);at=style.end;continue;}
        auto close=input[at]=='<'?input.find('>',at):std::string::npos;
        auto split=close!=std::string::npos?input.find('|',at):std::string::npos;
        if(split!=std::string::npos&&split<close){result.append(input,at,close-at+1);at=close+1;after=false;continue;}
        after=punctuation(input,at);size_t count=after?3:1;result.append(input,at,count);at+=count;
    }
    return result;
}
size_t visibleLength(const std::string& input){
    auto scalars=[&](size_t from,size_t to){size_t n=0;for(size_t i=from;i<to;++i){if(i+1<to&&input.compare(i,2,"@o")==0){++i;continue;}if(input[i]!='\n'&&(static_cast<unsigned char>(input[i])&0xc0)!=0x80)++n;}return n;};
    size_t count=0,at=0;
    while(at<input.size()){
        if(input.compare(at,2,"@o")==0){at+=2;continue;}
        const auto style=inlineStyle(input,at);if(style.end!=at){at=style.end;continue;}
        auto close=input[at]=='<'?input.find('>',at):std::string::npos;
        auto split=close!=std::string::npos?input.find('|',at):std::string::npos;
        if(split!=std::string::npos&&split<close){count+=scalars(at+1,split);at=close+1;}
        else{if(input[at]!='\n'&&(static_cast<unsigned char>(input[at])&0xc0)!=0x80)++count;++at;}
    }
    return count;
}
bool endingItalic(const std::string& input,bool initialItalic){
    bool italic=initialItalic;size_t at=0;
    while(at<input.size()){
        const auto style=inlineStyle(input,at);
        if(style.end!=at){if(style.italic>=0)italic=style.italic!=0;at=style.end;continue;}
        const auto close=input[at]=='<'?input.find('>',at):std::string::npos;
        const auto split=close!=std::string::npos?input.find('|',at):std::string::npos;
        if(split!=std::string::npos&&split<close)at=close+1;
        else ++at;
    }
    return italic;
}
void Reveal::reset(const std::string& text,uint64_t now,bool complete){total_=visibleLength(text);shown_=complete||speed_==0?double(total_):0;last_=now;}
void Reveal::append(const std::string& text,uint64_t now){tick(now);total_=visibleLength(text);shown_=speed_==0?double(total_):std::min(shown_,double(total_));}
void Reveal::remap(const std::string& text,uint64_t now){const double fraction=total_?shown_/double(total_):1.;total_=visibleLength(text);shown_=std::min(1.,fraction)*double(total_);last_=now;}
void Reveal::tick(uint64_t now){if(now<last_)return;shown_=speed_==0?double(total_):std::min(double(total_),shown_+double(now-last_)*speed_/1000.);last_=now;}
void Reveal::speed(int value,uint64_t now){if(value<0||value>100)throw std::out_of_range("Text speed must be 0..100 characters per second");tick(now);speed_=value;if(value==0)finish();}
bool Reveal::finish(){bool was=active();shown_=double(total_);return was;}
size_t Reveal::visible() const{return std::min(total_,size_t(std::floor(shown_+1e-9)));}
} // namespace mahoyo::text
