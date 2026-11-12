#pragma once

#include "resynth/AnalysisEngine.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace resynth::dsp {

struct HarmonicSynthesisOptions {
    double pitch_ratio{1.0};
    double time_stretch_ratio{1.0}; // Output duration / analyzed source duration.
    double gain{1.0};
    double phase_observation_strength{1.0};
    bool preserve_spectral_envelope{true};
    double maximum_envelope_compensation{4.0};
    // Positive values move the spectral envelope upward; negative values lower it.
    double formant_shift_semitones{0.0};
    // 100 is neutral. Lower values soften upper partials; higher values
    // progressively emphasize them without changing F0.
    double voice_voltage_percent{100.0};
    // Optional per-analysis-frame cents automation. It is interpolated between
    // frames while phase is integrated, leaving measured source phase residuals
    // independent of expressive pitch changes.
    std::vector<double> pitch_curve_cents;
};

// Reconstruct the periodic component from tracked partial amplitudes and local
// phase observations. Oscillator phase is integrated sample-by-sample; frame
// phase observations contribute bounded inter-frame phase corrections.
[[nodiscard]] std::vector<float> synthesizeHarmonicComponent(
    const AnalysisResult& analysis,
    const HarmonicSynthesisOptions& options = {});

} // namespace resynth::dsp
