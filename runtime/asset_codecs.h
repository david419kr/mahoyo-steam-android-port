#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace mahoyo::assets {
struct Error:std::runtime_error{using std::runtime_error::runtime_error;};
struct Bytes {const uint8_t* data;size_t size;Bytes slice(size_t offset,size_t length)const;};
struct Rect {uint32_t left=0,top=0,right=0,bottom=0;};
struct ImageInfo {uint32_t width,height;};
struct Image {uint32_t width,height;std::vector<uint8_t> rgba;};
struct Entry {std::string name;uint64_t offset,length;};
std::vector<Entry> hfaIndex(Bytes index,uint64_t fileSize);
ImageInfo imageInfo(Bytes data);
Image decodeImage(Bytes data,Rect crop={});
Image decodeComposed(Bytes primary,Bytes patch,Rect crop={},uint32_t sample=1);
std::vector<uint8_t> decodeMzx(Bytes data,bool invert=false);
std::vector<uint8_t> decodeLenzu(Bytes data);
}
