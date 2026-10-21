#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace mahoyo::save {
class Error : public std::runtime_error { using std::runtime_error::runtime_error; };
struct Layer {
    int32_t id=0,z=0;
    std::string image,kind,color;
    double x=0,y=0,w=0,h=0,alpha=1;
    std::array<int32_t,4> crop{};
    bool hasCrop=false;
};
struct Audio {
    std::string name,channel;
    bool loop=false;
    double volume=1;
};
struct Document {
    std::vector<uint8_t> vm;
    std::vector<Layer> layers;
    std::vector<Audio> audio;
    std::vector<std::string> history;
    std::string text;
    uint32_t textLine=0;
    bool hidden=false;
    std::vector<uint8_t> hostExtra;
};

// Versioned, CRC32-checked portable container. The destination is replaced only
// after the new complete file has been written and flushed in the same folder.
// Failure never removes the existing destination or any other save slot.
void writeAtomic(const std::filesystem::path& path,const Document& document);
Document read(const std::filesystem::path& path);
} // namespace mahoyo::save
