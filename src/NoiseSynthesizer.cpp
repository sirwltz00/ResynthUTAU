#include "resynth/NoiseSynthesizer.hpp"

#include "resynth/FFT.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>

namespace resynth::dsp {
namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;

double profileAtFrequency(const std::vector<float>& profile,
                          const std::uint32_t sample_rate,
                          const double frequency_hz) {
    if (profile.empty() || sample_rate == 0U) return 0.0;
    const double nyquist = static_cast<double>(sample_rate) * 0.5;
    const double minimum_frequency = std::min(30.0, nyquist / 8.0);
    const double frequency = std::clamp(frequency_hz, minimum_frequency, nyquist);
    const double min_log = std::log(minimum_frequency);
    const double max_log = std::log(nyquist);
    const double position = (std::log(frequency) - min_log) /
                            (max_log - min_log) * static_cast<double>(profile.size() - 1U);
    const double bounded = std::clamp(position, 0.0, static_cast<double>(profile.size() - 1U));
    const std::size_t left = static_cast<std::size_t>(bounded);
    const std::size_t right = std::min(left + 1U, profile.size() - 1U);
    const double fraction = bounded - static_cast<double>(left);
    return static_cast<double>(profile[left]) +
           (static_cast<double>(profile[right]) - static_cast<double>(profile[left])) * fraction;
}

std::uint64_t nextRandom(std::mt19937_64& generator) {
    return generator();
}

double unitFromRandom(const std::uint64_t value) {
    // Use the upper 53 bits to construct a reproducible [0, 1) double.
    return static_cast<double>(value >> 11U) * (1.0 / 9007199254740992.0);
}

} // namespace

std::vector<float> synthesizeAperiodicComponent(const AnalysisResult& analysis,
                                                 const NoiseSynthesisOptions& options) {
    if (analysis.sample_rate == 0U || analysis.input_frames == 0U || analysis.frames.empty() ||
        analysis.config.hop_size == 0U) {
        throw std::invalid_argument("Aperiodic synthesis requires a complete non-empty analysis result");
    }
    if (!(options.gain >= 0.0) || !std::isfinite(options.gain) ||
        !(options.time_stretch_ratio >= 0.25 && options.time_stretch_ratio <= 4.0) ||
        !std::isfinite(options.time_stretch_ratio) ||
        !isPowerOfTwo(options.fft_size) || options.fft_size < 256U ||
        options.fft_size < analysis.config.hop_size * 2U) {
        throw std::invalid_argument("Invalid aperiodic synthesis options");
    }

    const double requested_output_frames = static_cast<double>(analysis.input_frames) *
                                           options.time_stretch_ratio;
    if (!std::isfinite(requested_output_frames) || requested_output_frames < 1.0 ||
        requested_output_frames > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::invalid_argument("Requested aperiodic output duration is outside supported range");
    }
    const std::size_t output_frames = static_cast<std::size_t>(std::llround(requested_output_frames));
    const double requested_fft = static_cast<double>(options.fft_size) * options.time_stretch_ratio;
    const std::size_t fft_size = std::clamp<std::size_t>(
        nextPowerOfTwo(static_cast<std::size_t>(std::llround(requested_fft))), 256U, 8192U);
    const std::size_t half = fft_size / 2U;
    const double bin_hz = static_cast<double>(analysis.sample_rate) / static_cast<double>(fft_size);
    const std::size_t frame_count = analysis.frames.size();
    std::vector<double> accumulation(output_frames, 0.0);
    std::vector<double> power_weight(output_frames, 0.0);
    std::vector<double> shape(half > 1U ? half - 1U : 0U, 0.0);
    std::vector<double> window(fft_size, 0.0);
    for (std::size_t i = 0U; i < fft_size; ++i) {
        window[i] = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) /
                                         static_cast<double>(fft_size - 1U));
    }

    std::mt19937_64 generator(options.random_seed);
    for (std::size_t frame_index = 0U; frame_index < frame_count; ++frame_index) {
        const auto& frame = analysis.frames[frame_index];
        if (frame.aperiodic_magnitude_bands.empty() ||
            !(frame.rms > 0.0) || !(frame.aperiodicity_ratio > 0.0) || options.gain == 0.0) {
            continue;
        }
        if (!std::isfinite(frame.rms) || !std::isfinite(frame.aperiodicity_ratio)) {
            throw std::invalid_argument("Analysis contains invalid aperiodic energy measurements");
        }
        const double target_rms = frame.rms *
            std::sqrt(std::clamp(frame.aperiodicity_ratio, 0.0, 1.0)) * options.gain;
        if (!(target_rms > 1.0e-7)) continue;

        double shape_power = 0.0;
        double maximum_shape = 0.0;
        for (std::size_t bin = 1U; bin < half; ++bin) {
            const double value = std::max(0.0, profileAtFrequency(
                frame.aperiodic_magnitude_bands, analysis.sample_rate,
                static_cast<double>(bin) * bin_hz));
            shape[bin - 1U] = value;
            shape_power += value * value;
            maximum_shape = std::max(maximum_shape, value);
        }
        if (!(shape_power > 1.0e-20) || maximum_shape <= 1.0e-10) continue;
        const double shape_normalization = std::sqrt(
            static_cast<double>(shape.size()) / shape_power);

        std::vector<std::complex<double>> spectrum(fft_size, {0.0, 0.0});
        for (std::size_t bin = 1U; bin < half; ++bin) {
            const double magnitude = shape[bin - 1U] * shape_normalization;
            if (!(magnitude > 0.0)) continue;
            const double phase = kTwoPi * unitFromRandom(nextRandom(generator));
            const std::complex<double> value(magnitude * std::cos(phase),
                                             magnitude * std::sin(phase));
            spectrum[bin] = value;
            spectrum[fft_size - bin] = std::conj(value);
        }
        // The DC and Nyquist bins are zero to prevent a random offset or an
        // endpoint component that cannot have a conjugate partner.
        spectrum[0] = {0.0, 0.0};
        spectrum[half] = {0.0, 0.0};
        fft(spectrum, true);

        double block_power = 0.0;
        for (const auto& value : spectrum) block_power += value.real() * value.real();
        const double block_rms = std::sqrt(block_power / static_cast<double>(fft_size));
        if (!(block_rms > 1.0e-12) || !std::isfinite(block_rms)) continue;
        const double scale = target_rms / block_rms;

        const std::size_t output_center = static_cast<std::size_t>(std::llround(
            static_cast<double>(frame.center_sample) * options.time_stretch_ratio));
        const std::int64_t first_sample = static_cast<std::int64_t>(output_center) -
                                          static_cast<std::int64_t>(fft_size / 2U);
        for (std::size_t i = 0U; i < fft_size; ++i) {
            const std::int64_t output_index = first_sample + static_cast<std::int64_t>(i);
            if (output_index < 0 || static_cast<std::uint64_t>(output_index) >= output_frames) continue;
            const std::size_t destination = static_cast<std::size_t>(output_index);
            const double window_value = window[i];
            accumulation[destination] += spectrum[i].real() * scale * window_value;
            power_weight[destination] += window_value * window_value;
        }
    }

    std::vector<float> output(output_frames, 0.0F);
    for (std::size_t i = 0U; i < output.size(); ++i) {
        if (power_weight[i] <= 1.0e-12) continue;
        const double value = accumulation[i] / std::sqrt(power_weight[i]);
        output[i] = static_cast<float>(std::isfinite(value) ? value : 0.0);
    }
    return output;
}

} // namespace resynth::dsp
