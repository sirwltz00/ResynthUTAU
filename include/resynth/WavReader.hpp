#pragma once

#include "resynth/AudioBuffer.hpp"
#include <filesystem>
#include <string>

namespace resynth {

struct WavReadResult {
    AudioBuffer audio;
    std::uint16_t source_format{0};
    std::uint16_t source_bits_per_sample{0};
};

[[nodiscard]] WavReadResult readWav(const std::filesystem::path& path);

} // namespace resynth
