#pragma once

#include "resynth/AnalysisEngine.hpp"

#include <cstddef>
#include <vector>

namespace resynth::dsp {

enum class PhoneticKind {
    Silence,
    Voiced,
    Unvoiced,
    Transient
};

struct PhoneticRegion {
    PhoneticKind kind{PhoneticKind::Silence};
    std::size_t begin_sample{0U};
    std::size_t end_sample{0U};
};

struct PhoneticAnalysis {
    std::vector<PhoneticKind> frame_kinds;
    // Smoothed 0..1 resynthesis confidence. Transient and unvoiced frames are
    // held at zero so their original waveform can remain in the output.
    std::vector<double> voiced_weights;
    std::vector<PhoneticRegion> regions;
};

// Classify analysis frames into voiced, unvoiced, silence, and short transient
// regions. This is a conservative rendering guide, not a phoneme recognizer.
[[nodiscard]] PhoneticAnalysis classifyPhonetics(const AnalysisResult& analysis);

} // namespace resynth::dsp
