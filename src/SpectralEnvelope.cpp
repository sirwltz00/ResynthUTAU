#include "resynth/SpectralEnvelope.hpp"
#include "resynth/FFT.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace resynth::dsp {

std::vector<float> estimateSpectralEnvelope(
    const std::vector<std::complex<double>>& spectrum,
    const std::uint32_t sample_rate,
    const double max_cepstral_quefrency_ms,
    const double window_sum) {
    const std::size_t n = spectrum.size();
    if (!isPowerOfTwo(n) || n < 64U) {
        throw std::invalid_argument("Spectral-envelope FFT size must be a power of two >= 64");
    }
    if (sample_rate == 0U || !(max_cepstral_quefrency_ms > 0.0) || !(window_sum > 0.0)) {
        throw std::invalid_argument("Invalid spectral-envelope configuration");
    }

    std::vector<std::complex<double>> cepstrum(n, {0.0, 0.0});
    for (std::size_t i = 0; i < n; ++i) {
        const double magnitude = std::max(std::abs(spectrum[i]), 1.0e-12);
        cepstrum[i] = {std::log(magnitude), 0.0};
    }
    fft(cepstrum, true);

    const auto requested_cutoff = static_cast<std::size_t>(std::llround(
        max_cepstral_quefrency_ms * static_cast<double>(sample_rate) / 1000.0));
    const std::size_t cutoff = std::clamp(requested_cutoff, std::size_t{2U}, n / 2U - 1U);
    for (std::size_t i = cutoff + 1U; i < n - cutoff; ++i) cepstrum[i] = {0.0, 0.0};

    fft(cepstrum);
    const std::size_t half = n / 2U + 1U;
    std::vector<float> envelope(half, 0.0F);
    const double normalization = 2.0 / window_sum;
    for (std::size_t i = 0; i < half; ++i) {
        const double value = std::exp(std::clamp(cepstrum[i].real(), -60.0, 40.0)) * normalization;
        envelope[i] = static_cast<float>(std::isfinite(value) ? value : 0.0);
    }
    if (!envelope.empty()) envelope.front() *= 0.5F;
    if (envelope.size() > 1U) envelope.back() *= 0.5F;
    return envelope;
}

} // namespace resynth::dsp
