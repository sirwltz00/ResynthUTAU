#pragma once

#include "resynth/AnalysisEngine.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace resynth::dsp {

struct NoiseSynthesisOptions {
    double gain{1.0};
    double time_stretch_ratio{1.0}; // Output duration / analyzed source duration.
    std::uint64_t random_seed{0x5253594E544855ULL};
    std::size_t fft_size{2048U};
};

// Synthesize a stochastic, spectrally shaped aperiodic component with
// deterministic seeded noise and window-power-normalized overlap-add.
[[nodiscard]] std::vector<float> synthesizeAperiodicComponent(
    const AnalysisResult& analysis,
    const NoiseSynthesisOptions& options = {});

} // namespace resynth::dsp
