#include "resynth/HarmonicAnalyzer.hpp"
#include "resynth/FFT.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace resynth::dsp {

std::vector<HarmonicPartial> extractHarmonics(
    const std::vector<std::complex<double>>& spectrum,
    const std::uint32_t sample_rate,
    const double f0_hz,
    const double window_sum,
    const std::size_t maximum_harmonics) {
    const std::size_t n = spectrum.size();
    if (!isPowerOfTwo(n) || n < 64U || sample_rate == 0U || !(window_sum > 0.0)) {
        throw std::invalid_argument("Invalid harmonic-analysis spectrum or sample rate");
    }
    if (!(f0_hz > 0.0) || maximum_harmonics == 0U) return {};

    const double bin_hz = static_cast<double>(sample_rate) / static_cast<double>(n);
    const double nyquist = static_cast<double>(sample_rate) * 0.5;
    const std::size_t half = n / 2U;
    const std::size_t count = std::min(maximum_harmonics,
        static_cast<std::size_t>(std::floor(nyquist / f0_hz)));
    std::vector<HarmonicPartial> partials;
    partials.reserve(count);

    for (std::size_t order = 1U; order <= count; ++order) {
        const double expected_hz = static_cast<double>(order) * f0_hz;
        const double expected_bin = expected_hz / bin_hz;
        const std::size_t nearest = static_cast<std::size_t>(std::llround(expected_bin));
        const std::size_t radius = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::ceil(0.18 * f0_hz / bin_hz)), 1U, 4U);
        const std::size_t low = nearest > radius ? nearest - radius : 1U;
        const std::size_t high = std::min(half, nearest + radius);
        std::size_t peak_bin = low;
        double peak_magnitude = 0.0;
        for (std::size_t bin = low; bin <= high; ++bin) {
            const double magnitude = std::abs(spectrum[bin]);
            if (magnitude > peak_magnitude) {
                peak_magnitude = magnitude;
                peak_bin = bin;
            }
        }
        if (peak_magnitude <= 1.0e-12) continue;

        double peak_offset = 0.0;
        if (peak_bin > 0U && peak_bin < half) {
            const double left = std::log(std::max(std::abs(spectrum[peak_bin - 1U]), 1.0e-12));
            const double middle = std::log(std::max(std::abs(spectrum[peak_bin]), 1.0e-12));
            const double right = std::log(std::max(std::abs(spectrum[peak_bin + 1U]), 1.0e-12));
            const double denominator = left - 2.0 * middle + right;
            if (std::abs(denominator) > 1.0e-12) {
                peak_offset = std::clamp(0.5 * (left - right) / denominator, -0.5, 0.5);
                peak_magnitude = std::exp(middle - 0.25 * (left - right) * peak_offset);
            }
        }

        HarmonicPartial partial;
        partial.order = static_cast<std::uint32_t>(order);
        partial.frequency_hz = expected_hz;
        partial.amplitude = 2.0 * peak_magnitude / window_sum;
        partial.phase_radians = std::arg(spectrum[peak_bin]);
        partials.push_back(partial);
    }
    return partials;
}

} // namespace resynth::dsp
