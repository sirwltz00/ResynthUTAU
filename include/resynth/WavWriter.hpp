#pragma once

#include "resynth/AudioBuffer.hpp"
#include <filesystem>

namespace resynth {

void writeWav16(const std::filesystem::path& path, const AudioBuffer& audio);

} // namespace resynth
