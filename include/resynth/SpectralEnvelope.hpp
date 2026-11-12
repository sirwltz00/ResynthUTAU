#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace resynth::dsp {

// Cepstral spectral-envelope estimate from a full complex FFT spectrum.
// The returned values contain the positive-frequency half (including Nyquist).
[[nodiscard]] std::vector<float> estimateSpectralEnvelope(
    const std::vector<std::complex<double>>& spectrum,
    std::uint32_t sample_rate,
    double max_cepstral_quefrency_ms,
    double window_sum);

} // namespace resynth::dsp
