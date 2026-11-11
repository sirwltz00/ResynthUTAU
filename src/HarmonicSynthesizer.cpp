#include "resynth/HarmonicSynthesizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace resynth::dsp {
namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kEnvelopeFloor = 1.0e-8;

struct FramePartial {
    double amplitude{0.0};
    double phase{0.0};
    bool present{false};
};

double wrapPhase(const double phase) {
    return std::remainder(phase, kTwoPi);
}

double frameF0(const AnalysisFrame& frame) {
    return frame.voiced && frame.f0_hz > 0.0 ? frame.f0_hz : 0.0;
}

double interpolateF0(const AnalysisResult& analysis, const double sample_position,
                     std::size_t& segment) {
    const auto& frames = analysis.frames;
    if (frames.size() == 1U || sample_position <= static_cast<double>(frames.front().center_sample)) {
        return frameF0(frames.front());
    }
    while (segment + 1U < frames.size() - 1U &&
           sample_position >= static_cast<double>(frames[segment + 1U].center_sample)) {
        ++segment;
    }
    const auto& left = frames[segment];
    const auto& right = frames[std::min(segment + 1U, frames.size() - 1U)];
    const double span = static_cast<double>(right.center_sample - left.center_sample);
    if (span <= 0.0) return frameF0(left);
    const double t = std::clamp((sample_position - static_cast<double>(left.center_sample)) / span,
                                0.0, 1.0);
    return frameF0(left) + (frameF0(right) - frameF0(left)) * t;
}

double integrateF0Samples(const AnalysisResult& analysis, const std::size_t end_frame) {
    const auto& frames = analysis.frames;
    if (frames.size() < 2U || end_frame == 0U) return 0.0;
    double cycles_hz_samples = 0.0;
    for (std::size_t i = 0U; i < end_frame && i + 1U < frames.size(); ++i) {
        const double delta = static_cast<double>(frames[i + 1U].center_sample - frames[i].center_sample);
        cycles_hz_samples += 0.5 * (frameF0(frames[i]) + frameF0(frames[i + 1U])) * delta;
    }
    return cycles_hz_samples;
}

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


} // namespace

std::vector<float> synthesizeHarmonicComponent(const AnalysisResult& analysis,
                                               const HarmonicSynthesisOptions& options) {
    if (analysis.sample_rate == 0U || analysis.input_frames == 0U || analysis.frames.empty() ||
        analysis.config.hop_size == 0U) {
        throw std::invalid_argument("Harmonic synthesis requires a complete non-empty analysis result");
    }
    if (!(options.pitch_ratio > 0.0) || !std::isfinite(options.pitch_ratio) ||
        !(options.time_stretch_ratio >= 0.25 && options.time_stretch_ratio <= 4.0) ||
        !std::isfinite(options.time_stretch_ratio) ||
        !(options.gain >= 0.0) || !std::isfinite(options.gain) ||
        !(options.phase_observation_strength >= 0.0 && options.phase_observation_strength <= 1.0) ||
        !(options.maximum_envelope_compensation >= 1.0) ||
        !std::isfinite(options.maximum_envelope_compensation)) {
        throw std::invalid_argument("Invalid harmonic synthesis options");
    }

    std::size_t maximum_order = 0U;
    for (const auto& frame : analysis.frames) {
        if (!std::isfinite(frame.f0_hz) || !std::isfinite(frame.rms)) {
            throw std::invalid_argument("Analysis contains non-finite frame values");
        }
        for (const auto& partial : frame.harmonics) {
            if (!std::isfinite(partial.amplitude) || !std::isfinite(partial.phase_radians) ||
                partial.amplitude < 0.0) {
                throw std::invalid_argument("Analysis contains invalid harmonic partials");
            }
            maximum_order = std::max(maximum_order, static_cast<std::size_t>(partial.order));
        }
    }

    const double requested_output_frames = static_cast<double>(analysis.input_frames) *
                                           options.time_stretch_ratio;
    if (!std::isfinite(requested_output_frames) || requested_output_frames < 1.0 ||
        requested_output_frames > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::invalid_argument("Requested harmonic output duration is outside supported range");
    }
    const std::size_t output_frames = static_cast<std::size_t>(std::llround(requested_output_frames));
    std::vector<float> output(output_frames, 0.0F);
    if (maximum_order == 0U || options.gain == 0.0) return output;

    const std::size_t frame_count = analysis.frames.size();
    std::vector<std::vector<FramePartial>> partials(
        frame_count, std::vector<FramePartial>(maximum_order + 1U));
    std::vector<std::vector<double>> phase_corrections(
        frame_count > 1U ? frame_count - 1U : 0U,
        std::vector<double>(maximum_order + 1U, 0.0));

    for (std::size_t fi = 0U; fi < frame_count; ++fi) {
        const auto& frame = analysis.frames[fi];
        if (!frame.voiced || !(frame.f0_hz > 0.0)) continue;
        for (const auto& partial : frame.harmonics) {
            const std::size_t order = static_cast<std::size_t>(partial.order);
            if (order == 0U || order > maximum_order) continue;
            double amplitude = partial.amplitude;
            if (options.preserve_spectral_envelope && options.pitch_ratio != 1.0 &&
                frame.spectral_envelope_bands.size() >= 2U) {
                const double source_frequency = static_cast<double>(order) * frame.f0_hz;
                const double target_frequency = source_frequency * options.pitch_ratio;
                if (target_frequency < static_cast<double>(analysis.sample_rate) * 0.5) {
                    const double source_envelope = profileAtFrequency(
                        frame.spectral_envelope_bands, analysis.sample_rate, source_frequency);
                    const double target_envelope = profileAtFrequency(
                        frame.spectral_envelope_bands, analysis.sample_rate, target_frequency);
                    if (source_envelope > kEnvelopeFloor && target_envelope > 0.0) {
                        const double ratio = std::clamp(target_envelope / source_envelope,
                            1.0 / options.maximum_envelope_compensation,
                            options.maximum_envelope_compensation);
                        amplitude *= ratio;
                    }
                }
            }
            partials[fi][order] = {amplitude, partial.phase_radians, true};
        }
    }

    // Preserve measured phase perturbations while integrating a continuous
    // oscillator. The correction is bounded to the shortest wrapped phase error
    // in each analysis interval, rather than resetting phase at frame boundaries.
    for (std::size_t fi = 0U; fi + 1U < frame_count; ++fi) {
        const double interval = static_cast<double>(analysis.frames[fi + 1U].center_sample -
                                                    analysis.frames[fi].center_sample);
        if (!(interval > 0.0)) continue;
        const double source_advance_per_hz = interval / static_cast<double>(analysis.sample_rate);
        const double average_f0 = 0.5 * (frameF0(analysis.frames[fi]) +
                                         frameF0(analysis.frames[fi + 1U]));
        for (std::size_t order = 1U; order <= maximum_order; ++order) {
            const auto& a = partials[fi][order];
            const auto& b = partials[fi + 1U][order];
            if (!a.present || !b.present) continue;
            const double observed_delta = wrapPhase(b.phase - a.phase);
            const double expected_source_delta = kTwoPi * static_cast<double>(order) *
                                                 average_f0 * source_advance_per_hz;
            phase_corrections[fi][order] = wrapPhase(observed_delta - expected_source_delta) *
                                           options.phase_observation_strength;
        }
    }

    std::vector<double> phases(maximum_order + 1U, 0.0);
    for (std::size_t order = 1U; order <= maximum_order; ++order) {
        std::size_t reference_frame = frame_count;
        for (std::size_t fi = 0U; fi < frame_count; ++fi) {
            if (partials[fi][order].present) {
                reference_frame = fi;
                break;
            }
        }
        if (reference_frame == frame_count) continue;
        double correction_before_reference = 0.0;
        for (std::size_t fi = 0U; fi < reference_frame; ++fi) {
            correction_before_reference += phase_corrections[fi][order];
        }
        const double source_integral = integrateF0Samples(analysis, reference_frame);
        const double target_advance = kTwoPi * static_cast<double>(order) *
            options.pitch_ratio * options.time_stretch_ratio * source_integral /
            static_cast<double>(analysis.sample_rate);
        phases[order] = partials[reference_frame][order].phase - target_advance -
                        correction_before_reference;
    }

    const double nyquist = static_cast<double>(analysis.sample_rate) * 0.5;
    std::size_t segment = 0U;
    for (std::size_t sample = 0U; sample < output_frames; ++sample) {
        const double position = static_cast<double>(sample) / options.time_stretch_ratio;
        while (segment + 1U < frame_count - 1U &&
               position >= static_cast<double>(analysis.frames[segment + 1U].center_sample)) {
            ++segment;
        }
        const auto& left = analysis.frames[segment];
        const auto& right = analysis.frames[std::min(segment + 1U, frame_count - 1U)];
        const double interval = static_cast<double>(right.center_sample - left.center_sample);
        const double t = interval > 0.0
            ? std::clamp((position - static_cast<double>(left.center_sample)) / interval, 0.0, 1.0)
            : 0.0;
        const double f0 = frameF0(left) + (frameF0(right) - frameF0(left)) * t;
        const double f0_mid = interpolateF0(analysis,
            position + 0.5 / options.time_stretch_ratio, segment);
        const double phase_correction_interval = interval > 0.0
            ? interval * options.time_stretch_ratio : 1.0;
        double value = 0.0;

        for (std::size_t order = 1U; order <= maximum_order; ++order) {
            const double frequency = f0 * options.pitch_ratio * static_cast<double>(order);
            const double left_amplitude = partials[segment][order].present
                ? partials[segment][order].amplitude : 0.0;
            const std::size_t right_index = std::min(segment + 1U, frame_count - 1U);
            const double right_amplitude = partials[right_index][order].present
                ? partials[right_index][order].amplitude : 0.0;
            const double amplitude = left_amplitude + (right_amplitude - left_amplitude) * t;
            if (frequency > 0.0 && frequency < nyquist && amplitude > 0.0) {
                value += amplitude * std::cos(phases[order]);
            }
            // Advance every oscillator even while its partial is temporarily
            // absent or above Nyquist. Pausing a silent oscillator would create
            // a phase jump when that harmonic becomes audible again.
            const double correction = segment < phase_corrections.size()
                ? phase_corrections[segment][order] / phase_correction_interval : 0.0;
            const double phase_step = kTwoPi * static_cast<double>(order) *
                f0_mid * options.pitch_ratio / static_cast<double>(analysis.sample_rate);
            phases[order] += phase_step + correction;
        }
        const double scaled = value * options.gain;
        output[sample] = static_cast<float>(std::isfinite(scaled) ? scaled : 0.0);
    }
    return output;
}

} // namespace resynth::dsp
