#pragma once

#include "resynth/ParameterParser.hpp"
#include <filesystem>

namespace resynth {

struct RenderReport {
    std::size_t input_frames{0};
    std::size_t output_frames{0};
    std::uint32_t sample_rate{0};
    std::vector<std::string> warnings;
};

[[nodiscard]] RenderReport renderUtauInvocation(const ResamplerParameters& parameters);

} // namespace resynth
