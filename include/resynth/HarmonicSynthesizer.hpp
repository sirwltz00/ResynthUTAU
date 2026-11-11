#pragma once

#include "resynth/AnalysisEngine.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace resynth::dsp {

struct HarmonicSynthesisOptions {
    double pitch_ratio{1.0};
    double gain{1.0};
    double phase_observation_strength{1.0};
    bool preserve_spectral_envelope{true};
    double maximum_envelope_compensation{4.0};
};

// Reconstruct the periodic component from tracked partial amplitudes and local
// phase observations. Oscillator phase is integrated sample-by-sample; frame
// phase observations contribute bounded inter-frame phase corrections.
[[nodiscard]] std::vector<float> synthesizeHarmonicComponent(
    const AnalysisResult& analysis,
    const HarmonicSynthesisOptions& options = {});

} // namespace resynth::dsp
