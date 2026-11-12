#include "resynth/PitchAnalyzer.hpp"
#include "resynth/FFT.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace resynth::dsp {
namespace {

double monoAt(const AudioBuffer& audio, const std::size_t frame) {
    if (frame >= audio.frames()) return 0.0;
    double sum = 0.0;
    for (std::size_t channel = 0; channel < audio.channels; ++channel) {
        sum += static_cast<double>(audio.at(frame, channel));
    }
    return sum / static_cast<double>(audio.channels);
}

} // namespace

PitchEstimate estimatePitch(const AudioBuffer& audio, const std::size_t center_frame,
                            const PitchConfig& config) {
    if (!audio.valid() || audio.frames() == 0U) {
        throw std::invalid_argument("Pitch analysis requires a non-empty valid audio buffer");
    }
    if (!isPowerOfTwo(config.window_size) || config.window_size < 64U) {
        throw std::invalid_argument("Pitch analysis window must be a power of two >= 64");
    }
    if (!(config.min_f0_hz > 0.0) || !(config.max_f0_hz > config.min_f0_hz) ||
        config.max_f0_hz >= static_cast<double>(audio.sample_rate) * 0.5 ||
        !(config.yin_threshold > 0.0 && config.yin_threshold < 1.0) ||
        !(config.minimum_periodicity >= 0.0 && config.minimum_periodicity <= 1.0) ||
        !(config.minimum_rms >= 0.0)) {
        throw std::invalid_argument("Invalid F0 range for this sample rate");
    }

    const std::size_t size = config.window_size;
    const std::int64_t first = static_cast<std::int64_t>(center_frame) -
                               static_cast<std::int64_t>(size / 2U);
    std::vector<double> frame(size, 0.0);
    double mean = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        const std::int64_t source_index = first + static_cast<std::int64_t>(i);
        if (source_index >= 0) mean += monoAt(audio, static_cast<std::size_t>(source_index));
    }
    mean /= static_cast<double>(size);

    double window_sum_sq = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi *
                                                  static_cast<double>(i) /
                                                  static_cast<double>(size - 1U));
        const std::int64_t source_index = first + static_cast<std::int64_t>(i);
        const double sample = source_index >= 0
            ? monoAt(audio, static_cast<std::size_t>(source_index)) - mean : 0.0;
        frame[i] = sample * hann;
        window_sum_sq += frame[i] * frame[i];
    }

    PitchEstimate result;
    result.rms = std::sqrt(window_sum_sq / static_cast<double>(size));
    if (result.rms < config.minimum_rms) return result;

    const std::size_t fft_size = nextPowerOfTwo(size * 2U);
    std::vector<std::complex<double>> spectrum(fft_size, {0.0, 0.0});
    for (std::size_t i = 0; i < size; ++i) spectrum[i] = {frame[i], 0.0};
    fft(spectrum);
    for (auto& bin : spectrum) bin = {std::norm(bin), 0.0};
    fft(spectrum, true);

    std::vector<double> prefix_energy(size + 1U, 0.0);
    for (std::size_t i = 0; i < size; ++i) {
        prefix_energy[i + 1U] = prefix_energy[i] + frame[i] * frame[i];
    }

    const auto min_lag = std::max<std::size_t>(2U, static_cast<std::size_t>(
        std::floor(static_cast<double>(audio.sample_rate) / config.max_f0_hz)));
    const auto max_lag = std::min<std::size_t>(size / 2U, static_cast<std::size_t>(
        std::ceil(static_cast<double>(audio.sample_rate) / config.min_f0_hz)));
    if (max_lag <= min_lag + 2U) return result;

    std::vector<double> difference(max_lag + 1U, 0.0);
    std::vector<double> cmnd(max_lag + 1U, 1.0);
    double cumulative = 0.0;
    for (std::size_t lag = 1U; lag <= max_lag; ++lag) {
        const double left_energy = prefix_energy[size - lag];
        const double right_energy = prefix_energy[size] - prefix_energy[lag];
        const double correlation = spectrum[lag].real();
        const double value = std::max(0.0, left_energy + right_energy - 2.0 * correlation);
        difference[lag] = value;
        cumulative += value;
        if (cumulative > 1.0e-18) {
            cmnd[lag] = value * static_cast<double>(lag) / cumulative;
        }
    }

    std::size_t selected = 0U;
    for (std::size_t lag = min_lag; lag <= max_lag; ++lag) {
        if (cmnd[lag] < config.yin_threshold) {
            selected = lag;
            while (selected + 1U <= max_lag && cmnd[selected + 1U] < cmnd[selected]) ++selected;
            break;
        }
    }
    if (selected == 0U) {
        selected = min_lag;
        for (std::size_t lag = min_lag + 1U; lag <= max_lag; ++lag) {
            if (cmnd[lag] < cmnd[selected]) selected = lag;
        }
        if (cmnd[selected] > 0.35) return result;
    }

    // Parabolic interpolation around the selected CMND trough.
    double refined_lag = static_cast<double>(selected);
    if (selected > min_lag && selected < max_lag) {
        const double left = cmnd[selected - 1U];
        const double middle = cmnd[selected];
        const double right = cmnd[selected + 1U];
        const double denominator = left - 2.0 * middle + right;
        if (std::abs(denominator) > 1.0e-12) {
            const double shift = 0.5 * (left - right) / denominator;
            refined_lag += std::clamp(shift, -0.5, 0.5);
        }
    }

    result.yin_cmnd = cmnd[selected];
    result.periodicity = std::clamp(1.0 - result.yin_cmnd, 0.0, 1.0);
    result.f0_hz = static_cast<double>(audio.sample_rate) / refined_lag;
    result.voiced = result.f0_hz >= config.min_f0_hz &&
                    result.f0_hz <= config.max_f0_hz &&
                    result.periodicity >= config.minimum_periodicity;
    if (!result.voiced) result.f0_hz = 0.0;
    return result;
}

} // namespace resynth::dsp
