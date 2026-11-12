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
    const std::size_t base = frame * static_cast<std::size_t>(audio.channels);
    for (std::size_t channel = 0; channel < audio.channels; ++channel) {
        sum += static_cast<double>(audio.samples[base + channel]);
    }
    return sum / static_cast<double>(audio.channels);
}

} // namespace

void PitchWorkspace::prepare(const std::size_t window_size) {
    if (prepared_window_size == window_size) return;
    if (!isPowerOfTwo(window_size) || window_size < 64U || window_size > 1U << 20U) {
        throw std::invalid_argument("Pitch analysis window must be a power of two between 64 and 1048576");
    }
    prepared_window_size = window_size;
    prepared_fft_size = nextPowerOfTwo(window_size * 2U);
    frame.resize(window_size);
    hann.resize(window_size);
    spectrum.resize(prepared_fft_size);
    prefix_energy.resize(window_size + 1U);

    double phase_scale = 2.0 * std::numbers::pi / static_cast<double>(window_size - 1U);
    for (std::size_t i = 0U; i < window_size; ++i) {
        hann[i] = 0.5 - 0.5 * std::cos(phase_scale * static_cast<double>(i));
    }
}

PitchEstimate estimatePitch(const AudioBuffer& audio, const std::size_t center_frame,
                            const PitchConfig& config, PitchWorkspace& workspace) {
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

    workspace.prepare(config.window_size);
    const std::size_t size = config.window_size;
    const std::int64_t first = static_cast<std::int64_t>(center_frame) -
                               static_cast<std::int64_t>(size / 2U);

    // Read once, measure the DC component, then window in place. This avoids
    // two interleaved-channel traversals and reuses the same frame storage.
    double mean = 0.0;
    for (std::size_t i = 0U; i < size; ++i) {
        const std::int64_t source_index = first + static_cast<std::int64_t>(i);
        const double sample = source_index >= 0
            ? monoAt(audio, static_cast<std::size_t>(source_index)) : 0.0;
        workspace.frame[i] = sample;
        mean += sample;
    }
    mean /= static_cast<double>(size);

    double window_sum_sq = 0.0;
    for (std::size_t i = 0U; i < size; ++i) {
        const double sample = (workspace.frame[i] - mean) * workspace.hann[i];
        workspace.frame[i] = sample;
        window_sum_sq += sample * sample;
    }

    PitchEstimate result;
    result.rms = std::sqrt(window_sum_sq / static_cast<double>(size));
    if (result.rms < config.minimum_rms) return result;

    auto& spectrum = workspace.spectrum;
    std::fill(spectrum.begin(), spectrum.end(), std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0U; i < size; ++i) spectrum[i] = {workspace.frame[i], 0.0};
    fft(spectrum);
    for (auto& bin : spectrum) bin = {std::norm(bin), 0.0};
    fft(spectrum, true);

    auto& prefix_energy = workspace.prefix_energy;
    prefix_energy[0] = 0.0;
    for (std::size_t i = 0U; i < size; ++i) {
        prefix_energy[i + 1U] = prefix_energy[i] + workspace.frame[i] * workspace.frame[i];
    }

    const auto min_lag = std::max<std::size_t>(2U, static_cast<std::size_t>(
        std::floor(static_cast<double>(audio.sample_rate) / config.max_f0_hz)));
    const auto max_lag = std::min<std::size_t>(size / 2U, static_cast<std::size_t>(
        std::ceil(static_cast<double>(audio.sample_rate) / config.min_f0_hz)));
    if (max_lag <= min_lag + 2U) return result;

    auto& difference = workspace.difference;
    auto& cmnd = workspace.cmnd;
    difference.resize(max_lag + 1U);
    cmnd.assign(max_lag + 1U, 1.0);
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

PitchEstimate estimatePitch(const AudioBuffer& audio, const std::size_t center_frame,
                            const PitchConfig& config) {
    PitchWorkspace workspace;
    return estimatePitch(audio, center_frame, config, workspace);
}

} // namespace resynth::dsp
