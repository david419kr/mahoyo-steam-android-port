#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace mahoyo::shared {
struct State {
    uint64_t generation=0;
    std::vector<int32_t> variables;
    std::vector<uint8_t> flags;
};

// Missing file returns nullopt. Existing invalid, corrupt or unreadable data
// throws. Generation/slot precedence is the caller's policy, not this codec's.
std::optional<State> read(const std::filesystem::path& path);
// Validates before filesystem mutation; uses the CRC-checked SaveStore atomic
// container with only hostExtra populated. Failure preserves the old file.
void write(const std::filesystem::path& path,const State& state);
} // namespace mahoyo::shared
