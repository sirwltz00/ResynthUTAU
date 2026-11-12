#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace resynth::dsp {

struct HarmonicPartial {
    std::uint32_t order{0};
    double frequency_hz{0.0};
    double amplitude{0.0};
    double phase_radians{0.0};
};

[[nodiscard]] std::vector<HarmonicPartial> extractHarmonics(
    const std::vector<std::complex<double>>& spectrum,
    std::uint32_t sample_rate,
    double f0_hz,
    double window_sum,
    std::size_t maximum_harmonics = 128U);

} // namespace resynth::dsp
