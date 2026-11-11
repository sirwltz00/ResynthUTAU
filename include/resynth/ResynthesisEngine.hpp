#pragma once

#include "resynth/AudioBuffer.hpp"
#include "resynth/ParameterParser.hpp"
#include <vector>

namespace resynth {

struct ProcessResult {
    AudioBuffer audio;
    std::vector<std::string> warnings;
};

// Phase 1 waveform-domain duration renderer. Harmonic/aperiodic analysis and
// pitch reconstruction are deliberately reserved for later development phases.
[[nodiscard]] ProcessResult renderPhase1(const AudioBuffer& input,
                                         const ResamplerParameters& parameters);

} // namespace resynth
