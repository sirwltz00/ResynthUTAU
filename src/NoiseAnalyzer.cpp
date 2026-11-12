#include "resynth/NoiseAnalyzer.hpp"
#include "resynth/FFT.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numbers>

namespace resynth::dsp {

AperiodicityEstimate estimateAperiodicity(
    const std::vector<std::complex<double>>& spectrum,
    const std::uint32_t sample_rate,
    const double f0_hz,
    const double window_sum) {
    const std::size_t n = spectrum.size();
    if (!isPowerOfTwo(n) || n < 64U || sample_rate == 0U || !(window_sum > 0.0)) {
        throw std::invalid_argument("Invalid aperiodicity-analysis spectrum or sample rate");
    }

    const std::size_t half = n / 2U;
    const double bin_hz = static_cast<double>(sample_rate) / static_cast<double>(n);
    const double nyquist = static_cast<double>(sample_rate) * 0.5;
    AperiodicityEstimate result;
    result.aperiodic_magnitude.resize(half + 1U, 0.0F);

    for (std::size_t bin = 0U; bin <= half; ++bin) {
        const double endpoint_scale = (bin == 0U || bin == half) ? 1.0 : 2.0;
        const double magnitude = std::abs(spectrum[bin]) * endpoint_scale / window_sum;
        double harmonic_mask = 0.0;
        if (f0_hz > 0.0) {
            const double frequency = static_cast<double>(bin) * bin_hz;
            const double nearest_order = std::round(frequency / f0_hz);
            if (nearest_order >= 1.0 && nearest_order * f0_hz <= nyquist) {
                const double harmonic_hz = nearest_order * f0_hz;
                const double distance_bins = std::abs(frequency - harmonic_hz) / bin_hz;
                // The Hann main lobe extends to roughly two bins on either side.
                // Keep that lobe periodic, then use a cosine taper to avoid a hard mask edge.
                constexpr double full_mask_radius = 1.75;
                constexpr double zero_mask_radius = 3.0;
                if (distance_bins <= full_mask_radius) {
                    harmonic_mask = 1.0;
                } else if (distance_bins < zero_mask_radius) {
                    const double position = (distance_bins - full_mask_radius) /
                                            (zero_mask_radius - full_mask_radius);
                    harmonic_mask = 0.5 + 0.5 * std::cos(std::numbers::pi * position);
                }
            }
        }
        const double power = magnitude * magnitude;
        const double harmonic_power = power * harmonic_mask * harmonic_mask;
        const double residual_power = power * std::max(0.0, 1.0 - harmonic_mask * harmonic_mask);
        result.harmonic_energy += harmonic_power;
        result.aperiodic_energy += residual_power;
        result.aperiodic_magnitude[bin] = static_cast<float>(std::sqrt(residual_power));
    }

    const double total = result.harmonic_energy + result.aperiodic_energy;
    // Silence has no measurable periodic or aperiodic energy; report zero rather
    // than mislabeling an empty frame as broadband noise.
    result.aperiodicity_ratio = total > 1.0e-20
        ? std::clamp(result.aperiodic_energy / total, 0.0, 1.0) : 0.0;
    return result;
}

} // namespace resynth::dsp
