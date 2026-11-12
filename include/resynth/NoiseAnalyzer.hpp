#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace resynth::dsp {

struct AperiodicityEstimate {
    double harmonic_energy{0.0};
    double aperiodic_energy{0.0};
    double aperiodicity_ratio{1.0};
    std::vector<float> aperiodic_magnitude;
};

// Spectral-domain harmonic mask/residual estimate. This is an analysis feature,
// not yet an inverse-STFT waveform decomposition.
[[nodiscard]] AperiodicityEstimate estimateAperiodicity(
    const std::vector<std::complex<double>>& spectrum,
    std::uint32_t sample_rate,
    double f0_hz,
    double window_sum);

} // namespace resynth::dsp
