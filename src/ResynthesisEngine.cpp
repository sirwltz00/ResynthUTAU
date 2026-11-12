#include "resynth/ResynthesisEngine.hpp"
#include "resynth/HarmonicSynthesizer.hpp"
#include "resynth/NoiseSynthesizer.hpp"
#include "resynth/PhoneticAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <numbers>
#include <vector>
#include <stdexcept>

namespace resynth {
namespace {

std::size_t msToFrames(const double ms, const std::uint32_t sample_rate) {
    const double frames = ms * static_cast<double>(sample_rate) / 1000.0;
    if (frames < 0.0 || frames > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("Timing value is outside supported range");
    }
    return static_cast<std::size_t>(std::llround(frames));
}

float monoCorrelation(const AudioBuffer& reference, const std::size_t ref_start,
                      const AudioBuffer& candidate, const std::size_t cand_start,
                      const std::size_t count) {
    double dot = 0.0;
    double ref_energy = 1.0e-12;
    double cand_energy = 1.0e-12;
    const std::size_t channels = reference.channels;
    for (std::size_t i = 0; i < count; ++i) {
        double a = 0.0;
        double b = 0.0;
        for (std::size_t ch = 0; ch < channels; ++ch) {
            a += reference.samples[(ref_start + i) * channels + ch];
            b += candidate.samples[(cand_start + i) * channels + ch];
        }
        a /= static_cast<double>(channels);
        b /= static_cast<double>(channels);
        dot += a * b;
        ref_energy += a * a;
        cand_energy += b * b;
    }
    return static_cast<float>(dot / std::sqrt(ref_energy * cand_energy));
}

AudioBuffer wsolaStretch(const AudioBuffer& source, const std::size_t target_frames) {
    if (!source.valid()) throw std::runtime_error("Invalid source buffer for WSOLA");
    AudioBuffer out;
    out.sample_rate = source.sample_rate;
    out.channels = source.channels;
    if (target_frames == 0) return out;
    if (source.frames() == 0) {
        out.samples.assign(target_frames * out.channels, 0.0F);
        return out;
    }
    if (source.frames() == target_frames) {
        out.samples = source.samples;
        return out;
    }
    if (source.frames() < 96 || target_frames < 96) {
        // Tiny regions do not contain enough context for a correlation search.
        // Linear interpolation is confined to this short-region fallback.
        out.samples.resize(target_frames * out.channels);
        for (std::size_t i = 0; i < target_frames; ++i) {
            const double pos = target_frames == 1 ? 0.0 :
                static_cast<double>(i) * static_cast<double>(source.frames() - 1) /
                static_cast<double>(target_frames - 1);
            const std::size_t left = static_cast<std::size_t>(pos);
            const std::size_t right = std::min(left + 1, source.frames() - 1);
            const float frac = static_cast<float>(pos - static_cast<double>(left));
            for (std::size_t ch = 0; ch < out.channels; ++ch) {
                const float a = source.samples[left * out.channels + ch];
                const float b = source.samples[right * out.channels + ch];
                out.samples[i * out.channels + ch] = a + (b - a) * frac;
            }
        }
        return out;
    }

    const std::size_t nominal_window = std::max<std::size_t>(
        256, static_cast<std::size_t>(source.sample_rate / 50U)); // about 20 ms
    const std::size_t limiting_length = std::min(source.frames(), target_frames);
    // Leave room for more than one analysis grain when the requested region is
    // shorter than the nominal window. Otherwise a single grain would merely
    // crop the source instead of performing a duration transformation.
    const std::size_t adaptive_window = std::max<std::size_t>(96, limiting_length / 2);
    const std::size_t window = std::min(nominal_window, adaptive_window);
    const std::size_t overlap = window / 2;
    const std::size_t hop = window - overlap;
    const std::size_t search = std::max<std::size_t>(8, source.sample_rate / 200U); // 5 ms

    out.samples.assign(target_frames * out.channels, 0.0F);
    const std::size_t last_output_start = target_frames > window ? target_frames - window : 0;
    const double expected_advance = last_output_start == 0 ? 0.0 :
        static_cast<double>(source.frames() - window) / static_cast<double>(last_output_start / hop + 1);

    std::size_t out_start = 0;
    std::size_t previous_source_start = 0;
    bool first = true;
    while (out_start < target_frames) {
        const std::size_t this_window = std::min(window, target_frames - out_start);
        std::size_t source_start = 0;
        if (first) {
            source_start = 0;
            first = false;
        } else {
            const double expected = static_cast<double>(previous_source_start) + expected_advance;
            const auto center = static_cast<std::size_t>(std::clamp(
                expected, 0.0, static_cast<double>(source.frames() - this_window)));
            const std::size_t low = center > search ? center - search : 0;
            const std::size_t high = std::min(center + search, source.frames() - this_window);
            const std::size_t compare = std::min(overlap, target_frames - out_start);
            float best = -std::numeric_limits<float>::infinity();
            source_start = center;
            for (std::size_t candidate = low; candidate <= high; candidate += 2) {
                const float score = monoCorrelation(out, out_start - overlap, source,
                                                    candidate, compare);
                if (score > best) {
                    best = score;
                    source_start = candidate;
                }
                if (candidate == high) break;
            }
        }

        for (std::size_t i = 0; i < this_window; ++i) {
            const float w = (i < overlap && out_start > 0)
                ? static_cast<float>(i + 1) / static_cast<float>(overlap + 1)
                : 1.0F;
            const std::size_t out_frame = out_start + i;
            for (std::size_t ch = 0; ch < out.channels; ++ch) {
                const float sample = source.samples[(source_start + i) * out.channels + ch];
                const std::size_t index = out_frame * out.channels + ch;
                out.samples[index] = out.samples[index] * (1.0F - w) + sample * w;
            }
        }
        previous_source_start = source_start;
        if (out_start + this_window >= target_frames) break;
        out_start += hop;
    }
    return out;
}

AudioBuffer slice(const AudioBuffer& source, const std::size_t begin, const std::size_t end) {
    AudioBuffer result;
    result.sample_rate = source.sample_rate;
    result.channels = source.channels;
    const std::size_t safe_begin = std::min(begin, source.frames());
    const std::size_t safe_end = std::clamp(end, safe_begin, source.frames());
    const auto first = source.samples.begin() + static_cast<std::ptrdiff_t>(safe_begin * source.channels);
    const auto last = source.samples.begin() + static_cast<std::ptrdiff_t>(safe_end * source.channels);
    result.samples.assign(first, last);
    return result;
}

void append(AudioBuffer& destination, const AudioBuffer& source) {
    if (destination.channels != source.channels || destination.sample_rate != source.sample_rate) {
        throw std::runtime_error("Cannot append buffers with different formats");
    }
    destination.samples.insert(destination.samples.end(), source.samples.begin(), source.samples.end());
}

} // namespace

ProcessResult renderPhase1(const AudioBuffer& input, const ResamplerParameters& p) {
    if (!input.valid() || input.frames() == 0) throw std::runtime_error("Input audio buffer is empty or invalid");
    ProcessResult result;
    const std::size_t offset = std::min(msToFrames(p.offset_ms, input.sample_rate), input.frames());
    const std::size_t end_trim = std::min(msToFrames(p.end_blank_ms, input.sample_rate), input.frames() - offset);
    const std::size_t source_end = input.frames() - end_trim;
    if (source_end <= offset) throw std::runtime_error("Offset and end blank remove the entire input sample");

    const AudioBuffer usable = slice(input, offset, source_end);
    const std::size_t fixed_source = std::min(msToFrames(p.fixed_length_ms, input.sample_rate), usable.frames());
    const double fixed_scale = std::pow(2.0, (100.0 - p.velocity) / 100.0);
    std::size_t fixed_target = static_cast<std::size_t>(std::llround(static_cast<double>(fixed_source) * fixed_scale));
    if (fixed_source == 0) fixed_target = 0;

    std::size_t target_frames = 0;
    if (p.length_required_ms) {
        target_frames = msToFrames(*p.length_required_ms, input.sample_rate);
    } else {
        target_frames = fixed_target + (usable.frames() - fixed_source);
    }
    if (target_frames == 0) throw std::runtime_error("Requested output duration is zero");
    if (fixed_target > target_frames) {
        result.warnings.emplace_back("Requested note duration is shorter than velocity-adjusted fixed consonant; the fixed region was truncated");
        fixed_target = target_frames;
    }

    const AudioBuffer fixed_part = slice(usable, 0, fixed_source);
    const AudioBuffer tail_part = slice(usable, fixed_source, usable.frames());
    AudioBuffer fixed_out = wsolaStretch(fixed_part, fixed_target);
    const AudioBuffer tail_out = wsolaStretch(tail_part, target_frames - fixed_target);

    result.audio.sample_rate = input.sample_rate;
    result.audio.channels = input.channels;
    result.audio.samples.reserve(target_frames * input.channels);
    append(result.audio, fixed_out);
    append(result.audio, tail_out);

    const float gain = static_cast<float>(p.volume_percent / 100.0);
    for (float& sample : result.audio.samples) {
        const float value = sample * gain;
        sample = std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0.0F;
    }

    result.warnings.emplace_back("Phase 1 does not yet perform pitch/formant resynthesis; target note was parsed but source pitch is preserved");
    if (!p.flags.empty()) result.warnings.emplace_back("Flags were parsed but are not active in Phase 1: " + p.flags);
    if (p.modulation_percent != 0.0) result.warnings.emplace_back("Modulation was parsed but is not implemented in Phase 1");
    if (!p.pitch_bend_tokens.empty()) result.warnings.emplace_back("Pitch-bend data was parsed but is not implemented in Phase 1");
    return result;
}


namespace {

double voicedWeightAt(const dsp::AnalysisResult& analysis,
                      const dsp::PhoneticAnalysis& phonetics,
                      const double source_position,
                      std::size_t& segment) {
    const auto& frames = analysis.frames;
    if (frames.empty()) return 0.0;
    if (frames.size() == 1U || source_position <= static_cast<double>(frames.front().center_sample)) {
        return phonetics.voiced_weights.front();
    }
    while (segment + 1U < frames.size() - 1U &&
           source_position >= static_cast<double>(frames[segment + 1U].center_sample)) {
        ++segment;
    }
    const auto& left = frames[segment];
    const auto& right = frames[std::min(segment + 1U, frames.size() - 1U)];
    const double span = static_cast<double>(right.center_sample - left.center_sample);
    if (!(span > 0.0)) return phonetics.voiced_weights[segment];
    const double t = std::clamp((source_position - static_cast<double>(left.center_sample)) / span,
                                0.0, 1.0);
    const double a = phonetics.voiced_weights[segment];
    const double b = phonetics.voiced_weights[std::min(segment + 1U, frames.size() - 1U)];
    return a + (b - a) * t;
}

double monoValueAt(const AudioBuffer& audio, const std::int64_t frame) {
    if (frame < 0 || static_cast<std::uint64_t>(frame) >= audio.frames()) return 0.0;
    double value = 0.0;
    for (std::size_t channel = 0U; channel < audio.channels; ++channel) {
        value += static_cast<double>(audio.at(static_cast<std::size_t>(frame), channel));
    }
    return value / static_cast<double>(audio.channels);
}

double interpolatedChannelAt(const AudioBuffer& audio, const double frame,
                             const std::size_t channel) {
    if (!std::isfinite(frame) || frame < 0.0 || frame >= static_cast<double>(audio.frames())) {
        return 0.0;
    }
    const auto left = static_cast<std::size_t>(frame);
    const auto right = std::min(left + 1U, audio.frames() - 1U);
    const double fraction = frame - static_cast<double>(left);
    const double a = static_cast<double>(audio.at(left, channel));
    const double b = static_cast<double>(audio.at(right, channel));
    return a + (b - a) * fraction;
}

double parabolicPeakOffset(const double before, const double peak, const double after) {
    const double denominator = before - 2.0 * peak + after;
    if (std::abs(denominator) < 1.0e-12) return 0.0;
    return std::clamp(0.5 * (before - after) / denominator, -0.5, 0.5);
}

double voicedF0At(const dsp::AnalysisResult& analysis, const double sample_position,
                  std::size_t& segment) {
    const auto& frames = analysis.frames;
    if (frames.empty()) return 0.0;
    while (segment + 1U < frames.size() - 1U &&
           sample_position >= static_cast<double>(frames[segment + 1U].center_sample)) {
        ++segment;
    }
    std::size_t left = segment;
    std::size_t right = std::min(segment + 1U, frames.size() - 1U);
    while (left > 0U && (!frames[left].voiced || !(frames[left].f0_hz > 0.0))) --left;
    while (right + 1U < frames.size() && (!frames[right].voiced || !(frames[right].f0_hz > 0.0))) ++right;
    const bool left_valid = frames[left].voiced && frames[left].f0_hz > 0.0;
    const bool right_valid = frames[right].voiced && frames[right].f0_hz > 0.0;
    if (left_valid && right_valid && right > left) {
        const double span = static_cast<double>(frames[right].center_sample - frames[left].center_sample);
        if (span > 0.0) {
            const double t = std::clamp((sample_position - static_cast<double>(frames[left].center_sample)) / span,
                                        0.0, 1.0);
            return frames[left].f0_hz + (frames[right].f0_hz - frames[left].f0_hz) * t;
        }
    }
    if (left_valid) return frames[left].f0_hz;
    if (right_valid) return frames[right].f0_hz;
    return 0.0;
}

std::vector<double> detectPitchMarks(const AudioBuffer& source,
                                          const dsp::AnalysisResult& analysis,
                                          const dsp::PhoneticRegion& region) {
    std::vector<double> marks;
    if (region.end_sample <= region.begin_sample + 64U) return marks;
    const std::size_t begin = std::min(region.begin_sample, source.frames() - 1U);
    const std::size_t end = std::min(region.end_sample, source.frames());
    std::size_t frame_segment = 0U;
    const double first_f0 = voicedF0At(analysis,
        0.5 * static_cast<double>(begin + end), frame_segment);
    if (!(first_f0 > 0.0)) return marks;
    const double nominal_period = static_cast<double>(source.sample_rate) / first_f0;

    // Select the dominant waveform polarity for this voiced region. Searching
    // one consistent polarity prevents octave errors from alternating positive
    // and negative extrema in asymmetric glottal pulses.
    double positive_energy = 0.0;
    double negative_energy = 0.0;
    const std::size_t probe_step = std::max<std::size_t>(1U, static_cast<std::size_t>(nominal_period / 8.0));
    for (std::size_t sample = begin; sample < end; sample += probe_step) {
        const double value = monoValueAt(source, static_cast<std::int64_t>(sample));
        if (value >= 0.0) positive_energy += value * value;
        else negative_energy += value * value;
    }
    const double polarity = positive_energy >= negative_energy ? 1.0 : -1.0;

    const std::size_t initial_search_end = std::min(end - 1U,
        begin + std::max<std::size_t>(8U, static_cast<std::size_t>(std::ceil(nominal_period))));
    std::size_t first_mark = begin;
    double best_value = -std::numeric_limits<double>::infinity();
    for (std::size_t sample = begin; sample <= initial_search_end; ++sample) {
        const double value = polarity * monoValueAt(source, static_cast<std::int64_t>(sample));
        if (value > best_value) {
            best_value = value;
            first_mark = sample;
        }
    }
    const double first_before = polarity * monoValueAt(source, static_cast<std::int64_t>(first_mark) - 1);
    const double first_peak = polarity * monoValueAt(source, static_cast<std::int64_t>(first_mark));
    const double first_after = polarity * monoValueAt(source, static_cast<std::int64_t>(first_mark) + 1);
    marks.push_back(static_cast<double>(first_mark) +
        parabolicPeakOffset(first_before, first_peak, first_after));

    const std::size_t max_marks = std::max<std::size_t>(8U,
        static_cast<std::size_t>(std::ceil(static_cast<double>(end - begin) * 1100.0 /
                                           static_cast<double>(source.sample_rate))) + 8U);
    while (marks.size() < max_marks) {
        const double previous = marks.back();
        const double f0 = voicedF0At(analysis, static_cast<double>(previous), frame_segment);
        if (!(f0 > 0.0)) break;
        const double period = static_cast<double>(source.sample_rate) / f0;
        const double predicted = static_cast<double>(previous) + period;
        if (predicted >= static_cast<double>(end - 1U)) break;
        const std::size_t radius = std::max<std::size_t>(3U,
            static_cast<std::size_t>(std::ceil(period * 0.28)));
        const std::size_t low = std::max(static_cast<std::size_t>(std::floor(previous)) + 1U,
            static_cast<std::size_t>(std::max(0.0, std::floor(predicted - static_cast<double>(radius)))));
        const std::size_t high = std::min(end - 1U,
            static_cast<std::size_t>(std::min(static_cast<double>(end - 1U), predicted + static_cast<double>(radius))));
        if (high <= low) break;

        std::size_t candidate = static_cast<std::size_t>(std::llround(predicted));
        double best = -std::numeric_limits<double>::infinity();
        bool found_local_peak = false;
        for (std::size_t sample = low; sample <= high; ++sample) {
            const double value = polarity * monoValueAt(source, static_cast<std::int64_t>(sample));
            const double before = polarity * monoValueAt(source, static_cast<std::int64_t>(sample) - 1);
            const double after = polarity * monoValueAt(source, static_cast<std::int64_t>(sample) + 1);
            if (value >= before && value > after && value > best) {
                best = value;
                candidate = sample;
                found_local_peak = true;
            }
        }
        if (!found_local_peak) {
            best = -std::numeric_limits<double>::infinity();
            for (std::size_t sample = low; sample <= high; ++sample) {
                const double value = polarity * monoValueAt(source, static_cast<std::int64_t>(sample));
                if (value > best) {
                    best = value;
                    candidate = sample;
                }
            }
        }
        if (static_cast<double>(candidate) <= previous ||
            (static_cast<double>(candidate) - previous) < period * 0.55) {
            candidate = static_cast<std::size_t>(std::clamp(std::llround(predicted),
                static_cast<long long>(low), static_cast<long long>(high)));
        }
        if (static_cast<double>(candidate) <= previous || candidate >= end) break;
        const double before = polarity * monoValueAt(source, static_cast<std::int64_t>(candidate) - 1);
        const double peak = polarity * monoValueAt(source, static_cast<std::int64_t>(candidate));
        const double after = polarity * monoValueAt(source, static_cast<std::int64_t>(candidate) + 1);
        marks.push_back(static_cast<double>(candidate) + parabolicPeakOffset(before, peak, after));
    }
    return marks;
}

struct PitchBendCurve;
double pitchBendAt(const PitchBendCurve& curve, double output_position);
bool hasNonzeroPitchBend(const PitchBendCurve& curve);

AudioBuffer naturalPitchShift(const AudioBuffer& source,
                              const AudioBuffer& waveform_fallback,
                              const std::size_t target_frames,
                              const double pitch_ratio,
                              const dsp::AnalysisResult& analysis,
                              const dsp::PhoneticAnalysis& phonetics,
                              const double time_stretch_ratio,
                              const PitchBendCurve* pitch_curve = nullptr,
                              const double pitch_curve_origin_frames = 0.0) {
    if (!(pitch_ratio > 0.0) || !std::isfinite(pitch_ratio) ||
        !(time_stretch_ratio > 0.0) || !std::isfinite(time_stretch_ratio)) {
        throw std::invalid_argument("Natural pitch shifting received an invalid pitch or time ratio");
    }
    if (!waveform_fallback.valid() || waveform_fallback.frames() != target_frames ||
        waveform_fallback.channels != source.channels ||
        waveform_fallback.sample_rate != source.sample_rate) {
        throw std::invalid_argument("Natural pitch shifting requires a matching waveform fallback");
    }
    AudioBuffer output = waveform_fallback; // unvoiced/transient fallback
    const bool has_pitch_automation = pitch_curve != nullptr && hasNonzeroPitchBend(*pitch_curve);
    if (std::abs(pitch_ratio - 1.0) < 1.0e-8 &&
        std::abs(time_stretch_ratio - 1.0) < 1.0e-8 && !has_pitch_automation) return output;

    std::vector<double> accumulation(output.samples.size(), 0.0);
    std::vector<double> normalization(output.frames(), 0.0);
    std::size_t frame_segment = 0U;
    std::size_t voiced_region_count = 0U;
    for (const auto& region : phonetics.regions) {
        if (region.kind != dsp::PhoneticKind::Voiced) continue;
        const auto source_marks = detectPitchMarks(source, analysis, region);
        if (source_marks.size() < 3U) continue;
        ++voiced_region_count;

        const double output_region_end = static_cast<double>(region.end_sample) * time_stretch_ratio;
        std::vector<double> synthesis_marks;
        std::vector<std::size_t> selected_source_marks;
        double target_mark = static_cast<double>(source_marks.front()) * time_stretch_ratio;
        std::size_t mark_guard = 0U;
        while (target_mark < output_region_end && mark_guard++ < target_frames + 1U) {
            const double source_position = target_mark / time_stretch_ratio;
            const double f0 = voicedF0At(analysis, source_position, frame_segment);
            if (!(f0 > 0.0)) break;
            const double source_period = static_cast<double>(source.sample_rate) / f0;
            const double curve_cents = pitch_curve != nullptr
                ? pitchBendAt(*pitch_curve, target_mark + pitch_curve_origin_frames)
                : 0.0;
            const double effective_pitch_ratio = pitch_ratio * std::pow(2.0, curve_cents / 1200.0);
            const double target_period = source_period / effective_pitch_ratio;
            if (!(target_period >= 2.0) || !std::isfinite(target_period)) break;

            auto upper = std::lower_bound(source_marks.begin(), source_marks.end(),
                std::clamp(source_position, 0.0, static_cast<double>(source.frames() - 1U)));
            std::size_t selected = upper == source_marks.end()
                ? source_marks.size() - 1U
                : static_cast<std::size_t>(upper - source_marks.begin());
            if (selected > 0U) {
                const std::size_t previous = selected - 1U;
                if (std::abs(source_marks[previous] - source_position) <=
                    std::abs(source_marks[selected] - source_position)) {
                    selected = previous;
                }
            }
            synthesis_marks.push_back(target_mark);
            selected_source_marks.push_back(selected);
            target_mark += target_period;
        }
        if (synthesis_marks.size() < 2U) continue;

        for (std::size_t mark_index = 0U; mark_index < synthesis_marks.size(); ++mark_index) {
            const double center_out = synthesis_marks[mark_index];
            const std::size_t source_mark_index = selected_source_marks[mark_index];
            const double center_in = source_marks[source_mark_index];
            const double current_target_period = (mark_index + 1U < synthesis_marks.size())
                ? synthesis_marks[mark_index + 1U] - synthesis_marks[mark_index]
                : synthesis_marks[mark_index] - synthesis_marks[mark_index - 1U];
            const double previous_target_period = mark_index > 0U
                ? synthesis_marks[mark_index] - synthesis_marks[mark_index - 1U]
                : current_target_period;
            const double next_target_period = mark_index + 1U < synthesis_marks.size()
                ? synthesis_marks[mark_index + 1U] - synthesis_marks[mark_index]
                : current_target_period;
            const double previous_source_period = source_mark_index > 0U
                ? source_marks[source_mark_index] - source_marks[source_mark_index - 1U]
                : (source_mark_index + 1U < source_marks.size()
                    ? source_marks[source_mark_index + 1U] - source_marks[source_mark_index]
                    : current_target_period / time_stretch_ratio);
            const double next_source_period = source_mark_index + 1U < source_marks.size()
                ? source_marks[source_mark_index + 1U] - source_marks[source_mark_index]
                : previous_source_period;
            const double left_radius = std::max(2.0, std::min(previous_source_period, previous_target_period));
            const double right_radius = std::max(2.0, std::min(next_source_period, next_target_period));
            const std::int64_t first_out = static_cast<std::int64_t>(std::ceil(center_out - left_radius));
            const std::int64_t last_out = static_cast<std::int64_t>(std::floor(center_out + right_radius));

            for (std::int64_t out_frame = first_out; out_frame <= last_out; ++out_frame) {
                if (out_frame < 0 || static_cast<std::uint64_t>(out_frame) >= target_frames) continue;
                const double offset = static_cast<double>(out_frame) - center_out;
                const double radius = offset < 0.0 ? left_radius : right_radius;
                const double window = 0.5 + 0.5 * std::cos(std::numbers::pi * offset / radius);
                if (!(window > 0.0)) continue;
                const double source_frame = center_in + offset;
                if (source_frame < 0.0 || source_frame >= static_cast<double>(source.frames())) continue;
                const std::size_t output_frame = static_cast<std::size_t>(out_frame);
                normalization[output_frame] += window;
                for (std::size_t channel = 0U; channel < source.channels; ++channel) {
                    const std::size_t out_index = output_frame * source.channels + channel;
                    accumulation[out_index] += interpolatedChannelAt(source, source_frame, channel) * window;
                }
            }
        }
    }

    if (voiced_region_count == 0U) return output;
    std::size_t confidence_segment = 0U;
    for (std::size_t frame = 0U; frame < target_frames; ++frame) {
        if (normalization[frame] <= 1.0e-8) continue;
        const double source_position = static_cast<double>(frame) / time_stretch_ratio;
        const double confidence = std::clamp(voicedWeightAt(
            analysis, phonetics, source_position, confidence_segment), 0.0, 1.0);
        const double transition = std::clamp((confidence - 0.08) / 0.70, 0.0, 1.0);
        const double wet = transition * transition * (3.0 - 2.0 * transition);
        // Both paths are waveform-derived and strongly correlated. A linear
        // complementary blend avoids the level bump and partial cancellation
        // associated with equal-power crossfades between correlated signals.
        const double dry_gain = 1.0 - wet;
        const double wet_gain = wet;
        for (std::size_t channel = 0U; channel < source.channels; ++channel) {
            const std::size_t index = frame * source.channels + channel;
            const double psola_sample = accumulation[index] / normalization[frame];
            const double mixed = static_cast<double>(output.samples[index]) * dry_gain +
                                  psola_sample * wet_gain;
            output.samples[index] = static_cast<float>(std::isfinite(mixed) ? mixed : 0.0);
        }
    }
    return output;
}

bool requiresHarmonicExpressionPath(const ResamplerParameters& parameters) {
    const auto& e = parameters.expression;
    return e.breathiness_percent != 0.0 || e.formant_shift_semitones != 0.0 ||
           e.gender_factor != 0.0 || e.voice_voltage_percent != 100.0 ||
           e.vibrato_depth_percent != 0.0 || e.harmonic_balance_percent != 100.0 ||
           e.noise_level_percent != 100.0 || e.noise_suppression_percent != 0.0 ||
           parameters.modulation_percent != 0.0 ||
           (!parameters.openutau_classic && !parameters.pitch_bend_tokens.empty()) ||
           std::abs(e.fine_pitch_cents) > 1.0e-9;
}

double medianVoicedF0(const dsp::AnalysisResult& analysis,
                      const dsp::PhoneticAnalysis& phonetics,
                      std::size_t& stable_voiced_frames) {
    std::vector<double> values;
    values.reserve(analysis.summary.voiced_frames);
    stable_voiced_frames = 0U;
    for (std::size_t i = 0U; i < analysis.frames.size(); ++i) {
        const auto& frame = analysis.frames[i];
        if (i < phonetics.frame_kinds.size() &&
            phonetics.frame_kinds[i] == dsp::PhoneticKind::Voiced &&
            frame.voiced && frame.f0_hz > 0.0 && std::isfinite(frame.f0_hz)) {
            values.push_back(frame.f0_hz);
            ++stable_voiced_frames;
        }
    }
    if (values.empty()) return 0.0;
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2U);
    std::nth_element(values.begin(), middle, values.end());
    if (values.size() % 2U == 1U) return *middle;
    const auto lower = std::max_element(values.begin(), middle);
    return 0.5 * (*lower + *middle);
}

void applyJoinRamp(AudioBuffer& fixed, AudioBuffer& tail) {
    if (fixed.frames() == 0U || tail.frames() == 0U ||
        fixed.channels != tail.channels || fixed.sample_rate != tail.sample_rate) return;
    const std::size_t ramp_frames = std::min<std::size_t>(
        std::max<std::size_t>(1U, static_cast<std::size_t>(fixed.sample_rate / 2000U)),
        tail.frames()); // approximately 0.5 ms
    for (std::size_t i = 0U; i < ramp_frames; ++i) {
        const double t = static_cast<double>(i + 1U) / static_cast<double>(ramp_frames);
        for (std::size_t channel = 0U; channel < fixed.channels; ++channel) {
            const double anchor = fixed.at(fixed.frames() - 1U, channel);
            const double sample = tail.at(i, channel);
            tail.at(i, channel) = static_cast<float>(anchor * (1.0 - t) + sample * t);
        }
    }
}

struct PitchBendCurve {
    std::vector<double> cents;
    double point_spacing_frames{256.0};
};

PitchBendCurve makePitchBendCurve(const std::vector<std::string>& tokens,
                                  const std::uint32_t sample_rate,
                                  const double stretch_ratio,
                                  const bool points_on_output_timeline,
                                  std::vector<std::string>& warnings) {
    PitchBendCurve curve;
    curve.point_spacing_frames = 256.0 * stretch_ratio;
    std::size_t first_point = 0U;
    double tempo = 0.0;
    if (!tokens.empty() && !tokens.front().empty() &&
        (tokens.front().front() == 'Q' || tokens.front().front() == 'q')) {
        std::string tempo_text = tokens.front().substr(1U);
        first_point = 1U;
        if (tempo_text.empty() && tokens.size() > 1U) {
            tempo_text = tokens[1U];
            first_point = 2U;
        }
        if (!tempo_text.empty()) {
            try {
                std::size_t consumed = 0U;
                tempo = std::stod(tempo_text, &consumed);
                if (consumed != tempo_text.size() || !(tempo > 0.0) || !std::isfinite(tempo)) tempo = 0.0;
            } catch (...) { tempo = 0.0; }
        }
        if (tempo > 0.0) {
            // Legacy UTAU Q mode subdivides each quarter note into 96 points.
            curve.point_spacing_frames = static_cast<double>(sample_rate) * 60.0 /
                                         (tempo * 96.0);
            if (!points_on_output_timeline) curve.point_spacing_frames *= stretch_ratio;
        } else {
            warnings.emplace_back("Invalid Q<tempo> pitch-bend grid; the 256-sample legacy grid was used");
        }
    }

    for (std::size_t i = first_point; i < tokens.size(); ++i) {
        try {
            std::size_t consumed = 0U;
            const double value = std::stod(tokens[i], &consumed);
            if (consumed != tokens[i].size() || !std::isfinite(value) ||
                value < -2400.0 || value > 2400.0) {
                warnings.emplace_back("Ignored invalid pitch-bend point: " + tokens[i]);
                continue;
            }
            curve.cents.push_back(value);
        } catch (...) {
            warnings.emplace_back("Ignored invalid pitch-bend point: " + tokens[i]);
        }
    }
    return curve;
}

double pitchBendAt(const PitchBendCurve& curve, const double output_position) {
    if (curve.cents.empty() || !(curve.point_spacing_frames > 0.0)) return 0.0;
    const double point = std::clamp(output_position / curve.point_spacing_frames,
                                    0.0, static_cast<double>(curve.cents.size() - 1U));
    const std::size_t left = static_cast<std::size_t>(point);
    const std::size_t right = std::min(left + 1U, curve.cents.size() - 1U);
    const double fraction = point - static_cast<double>(left);
    return curve.cents[left] + (curve.cents[right] - curve.cents[left]) * fraction;
}

bool hasNonzeroPitchBend(const PitchBendCurve& curve) {
    return std::any_of(curve.cents.begin(), curve.cents.end(), [](const double value) {
        return std::abs(value) > 1.0e-9;
    });
}

double vibratoAt(const double output_position, const double total_output_frames,
                 const std::uint32_t sample_rate, const ExpressionControls& expression) {
    if (!(expression.vibrato_depth_percent > 0.0) || sample_rate == 0U) return 0.0;
    const double time = output_position / static_cast<double>(sample_rate);
    const double duration = total_output_frames / static_cast<double>(sample_rate);
    const double fade_in = std::clamp((time - 0.04) / 0.08, 0.0, 1.0);
    const double fade_out = std::clamp((duration - time) / 0.08, 0.0, 1.0);
    const double envelope = fade_in * fade_out;
    const double depth_cents = expression.vibrato_depth_percent * 0.5;
    return std::sin(2.0 * std::numbers::pi * expression.vibrato_rate_hz * time) *
           depth_cents * envelope;
}

void applyAttackSpeed(AudioBuffer& tail, const dsp::PhoneticAnalysis& phonetics,
                      const double stretch_ratio, const double attack_speed_percent) {
    if (tail.frames() == 0U || attack_speed_percent == 100.0) return;
    auto first_voiced = std::find_if(phonetics.regions.begin(), phonetics.regions.end(),
        [](const dsp::PhoneticRegion& region) {
            return region.kind == dsp::PhoneticKind::Voiced;
        });
    if (first_voiced == phonetics.regions.end()) return;
    const std::size_t start = std::min(tail.frames(), static_cast<std::size_t>(std::llround(
        static_cast<double>(first_voiced->begin_sample) * stretch_ratio)));
    if (attack_speed_percent < 100.0) {
        const double duration_ms = (100.0 - attack_speed_percent) * 1.2;
        const std::size_t ramp = std::min(tail.frames() - start,
            std::max<std::size_t>(1U, static_cast<std::size_t>(std::llround(
                duration_ms * static_cast<double>(tail.sample_rate) / 1000.0))));
        for (std::size_t i = 0U; i < ramp; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(1U, ramp - 1U));
            const double smooth = t * t * (3.0 - 2.0 * t);
            const float gain = static_cast<float>(0.15 + 0.85 * smooth);
            for (std::size_t ch = 0U; ch < tail.channels; ++ch) tail.at(start + i, ch) *= gain;
        }
    } else {
        const double boost = 0.12 * (attack_speed_percent - 100.0) / 100.0;
        const std::size_t ramp = std::min(tail.frames() - start,
            std::max<std::size_t>(1U, static_cast<std::size_t>(tail.sample_rate / 125U)));
        for (std::size_t i = 0U; i < ramp; ++i) {
            const double t = 1.0 - static_cast<double>(i) / static_cast<double>(ramp);
            const float gain = static_cast<float>(1.0 + boost * t * t);
            for (std::size_t ch = 0U; ch < tail.channels; ++ch) tail.at(start + i, ch) *= gain;
        }
    }
}

void applyDynamics(AudioBuffer& audio, const double dynamics_percent) {
    if (audio.frames() < 2U || dynamics_percent == 100.0) return;
    const double amount = (dynamics_percent - 100.0) / 100.0 * 0.20;
    const double attack = std::exp(-1.0 / (0.008 * static_cast<double>(audio.sample_rate)));
    const double release = std::exp(-1.0 / (0.075 * static_cast<double>(audio.sample_rate)));
    std::vector<double> gain(audio.frames(), 1.0);
    double envelope_power = 0.0;
    double mean_envelope = 0.0;
    for (std::size_t frame = 0U; frame < audio.frames(); ++frame) {
        double power = 0.0;
        for (std::size_t ch = 0U; ch < audio.channels; ++ch) {
            const double sample = audio.at(frame, ch);
            power += sample * sample;
        }
        power /= static_cast<double>(audio.channels);
        const double coefficient = power > envelope_power ? attack : release;
        envelope_power = coefficient * envelope_power + (1.0 - coefficient) * power;
        mean_envelope += std::sqrt(std::max(0.0, envelope_power));
        gain[frame] = std::sqrt(std::max(0.0, envelope_power));
    }
    mean_envelope /= static_cast<double>(audio.frames());
    if (!(mean_envelope > 1.0e-9)) return;
    double mean_gain = 0.0;
    for (double& value : gain) {
        value = std::clamp(std::pow(std::max(value, 1.0e-8) / mean_envelope, amount), 0.70, 1.40);
        mean_gain += value;
    }
    mean_gain /= static_cast<double>(gain.size());
    if (!(mean_gain > 0.0)) return;
    for (std::size_t frame = 0U; frame < audio.frames(); ++frame) {
        const float factor = static_cast<float>(gain[frame] / mean_gain);
        for (std::size_t ch = 0U; ch < audio.channels; ++ch) audio.at(frame, ch) *= factor;
    }
}

void applySpectralBrightness(AudioBuffer& audio, const double brightness_percent) {
    if (audio.frames() == 0U || brightness_percent == 100.0) return;
    const double high_gain = std::pow(2.0, (brightness_percent - 100.0) / 100.0);
    const double cutoff = 1700.0;
    const double alpha = std::exp(-2.0 * std::numbers::pi * cutoff /
                                  static_cast<double>(audio.sample_rate));
    std::vector<double> low(audio.channels, 0.0);
    for (std::size_t frame = 0U; frame < audio.frames(); ++frame) {
        for (std::size_t ch = 0U; ch < audio.channels; ++ch) {
            const double input = audio.at(frame, ch);
            low[ch] = (1.0 - alpha) * input + alpha * low[ch];
            const double output = low[ch] + (input - low[ch]) * high_gain;
            audio.at(frame, ch) = static_cast<float>(std::isfinite(output) ? output : 0.0);
        }
    }
}

void applyLowpass(AudioBuffer& audio, const double lowpass_percent) {
    if (audio.frames() == 0U || lowpass_percent <= 0.0) return;
    const double nyquist_guard = 0.45 * static_cast<double>(audio.sample_rate);
    const double cutoff = nyquist_guard * std::pow(0.12, lowpass_percent / 99.0);
    const double alpha = std::exp(-2.0 * std::numbers::pi * cutoff /
                                  static_cast<double>(audio.sample_rate));
    std::vector<double> state(audio.channels, 0.0);
    for (std::size_t frame = 0U; frame < audio.frames(); ++frame) {
        for (std::size_t ch = 0U; ch < audio.channels; ++ch) {
            const double input = audio.at(frame, ch);
            state[ch] = (1.0 - alpha) * input + alpha * state[ch];
            audio.at(frame, ch) = static_cast<float>(state[ch]);
        }
    }
}

void applyDecay(AudioBuffer& audio, const double decay_percent) {
    if (audio.frames() == 0U || decay_percent <= 0.0) return;
    const std::size_t fade_frames = std::min(audio.frames(), std::max<std::size_t>(1U,
        static_cast<std::size_t>(std::llround(
            (250.0 * decay_percent / 100.0) * static_cast<double>(audio.sample_rate) / 1000.0))));
    const std::size_t start = audio.frames() - fade_frames;
    for (std::size_t i = 0U; i < fade_frames; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(std::max<std::size_t>(1U, fade_frames - 1U));
        const double gain = 0.5 * (1.0 + std::cos(std::numbers::pi * t));
        for (std::size_t ch = 0U; ch < audio.channels; ++ch) audio.at(start + i, ch) *= static_cast<float>(gain);
    }
}

} // namespace

ProcessResult renderPhase5(const AudioBuffer& input, const ResamplerParameters& p) {
    if (!input.valid() || input.frames() == 0U) {
        throw std::runtime_error("Input audio buffer is empty or invalid");
    }
    ProcessResult result;
    const std::size_t offset = std::min(msToFrames(p.offset_ms, input.sample_rate), input.frames());
    std::size_t source_end = input.frames();
    if (p.openutau_classic) {
        if (p.openutau_cutoff_ms >= 0.0) {
            const std::size_t right_blank = std::min(
                msToFrames(p.openutau_cutoff_ms, input.sample_rate), input.frames());
            source_end = input.frames() - right_blank;
        } else {
            const double endpoint_ms = p.offset_ms + std::abs(p.openutau_cutoff_ms);
            source_end = std::min(msToFrames(endpoint_ms, input.sample_rate), input.frames());
        }
    } else {
        const std::size_t end_trim = std::min(msToFrames(p.end_blank_ms, input.sample_rate), input.frames() - offset);
        source_end = input.frames() - end_trim;
    }
    if (source_end <= offset) {
        throw std::runtime_error("Offset and end blank remove the entire input sample");
    }

    const AudioBuffer usable = slice(input, offset, source_end);
    const std::size_t fixed_source = std::min(msToFrames(p.fixed_length_ms, input.sample_rate),
                                              usable.frames());
    const ExpressionControls& expression = p.expression;
    const double fixed_scale = std::pow(2.0, (100.0 - p.velocity) / 100.0) *
                               expression.consonant_rate_percent / 100.0;
    std::size_t fixed_target = static_cast<std::size_t>(std::llround(
        static_cast<double>(fixed_source) * fixed_scale));

    std::size_t target_frames = p.length_required_ms
        ? msToFrames(*p.length_required_ms, input.sample_rate)
        : fixed_target + (usable.frames() - fixed_source);
    if (target_frames == 0U) throw std::runtime_error("Requested output duration is zero");
    if (fixed_target > target_frames) {
        result.warnings.emplace_back(
            "Requested note duration is shorter than the velocity-adjusted fixed region; the fixed region was truncated");
        fixed_target = target_frames;
    }

    const AudioBuffer fixed_source_audio = slice(usable, 0U, fixed_source);
    AudioBuffer fixed_output = wsolaStretch(fixed_source_audio, fixed_target);
    const AudioBuffer tail_source = slice(usable, fixed_source, usable.frames());
    const std::size_t tail_target_frames = target_frames - fixed_target;
    AudioBuffer tail_output;
    tail_output.sample_rate = input.sample_rate;
    tail_output.channels = input.channels;

    if (tail_target_frames > 0U && tail_source.frames() > 0U) {
        // WSOLA is the waveform-preserving path for consonants, unvoiced sounds,
        // and transients. The harmonic path is blended in only where the analysis
        // indicates stable voicing.
        AudioBuffer preserved = wsolaStretch(tail_source, tail_target_frames);
        dsp::AnalysisConfig analysis_config;
        analysis_config.minimum_rms = 0.0010;
        const dsp::AnalysisResult analysis = dsp::analyzeAudio(tail_source, analysis_config);
        const dsp::PhoneticAnalysis phonetics = dsp::classifyPhonetics(analysis);

        for (const auto& region : phonetics.regions) {
            if (region.kind == dsp::PhoneticKind::Voiced) ++result.voiced_regions;
            else if (region.kind == dsp::PhoneticKind::Transient) ++result.transient_regions;
            else if (region.kind == dsp::PhoneticKind::Unvoiced) ++result.unvoiced_regions;
        }

        std::size_t stable_voiced_frames = 0U;
        const double source_f0 = medianVoicedF0(analysis, phonetics, stable_voiced_frames);
        result.estimated_source_f0_hz = source_f0;
        if (source_f0 > 0.0 && stable_voiced_frames >= 3U) {
            const double pitch_ratio = p.target_pitch_is_note
                ? (440.0 * std::pow(2.0,
                    (static_cast<double>(p.target_midi) - 69.0) / 12.0)) / source_f0
                : p.pitch_percent / 100.0;
            result.pitch_ratio = pitch_ratio;
            const double stretch_ratio = static_cast<double>(tail_target_frames) /
                                         static_cast<double>(tail_source.frames());

            const double effective_formant_shift = std::clamp(
                expression.formant_shift_semitones - 0.06 * expression.gender_factor,
                -24.0, 24.0);

            if (!requiresHarmonicExpressionPath(p)) {
                // Natural mode is waveform-derived: stable voiced regions use
                // pitch-synchronous overlap-add (TD-PSOLA), while unvoiced,
                // transient, and consonant regions remain on the single WSOLA
                // fallback. No independently phased oscillator is added to the
                // original vowel waveform.
                const PitchBendCurve bend_curve = makePitchBendCurve(
                    p.pitch_bend_tokens, input.sample_rate, stretch_ratio,
                    p.pitch_points_on_output_timeline, result.warnings);
                const double curve_origin_frames = p.openutau_classic
                    ? static_cast<double>(fixed_target) : 0.0;
                tail_output = naturalPitchShift(tail_source, preserved, tail_target_frames,
                    pitch_ratio, analysis, phonetics, stretch_ratio,
                    p.openutau_classic ? &bend_curve : nullptr, curve_origin_frames);
                result.warnings.emplace_back(
                    "Natural source-waveform pitch shifting was used; voiced regions use pitch-synchronous overlap-add (TD-PSOLA), while consonants remain waveform-preserved");
            } else {
                // The legacy harmonic route remains available for expression
                // controls that require a time-varying oscillator contour.
                const PitchBendCurve bend_curve = makePitchBendCurve(
                    p.pitch_bend_tokens, input.sample_rate, stretch_ratio,
                    p.pitch_points_on_output_timeline, result.warnings);
                std::vector<double> pitch_curve_cents(analysis.frames.size(), 0.0);
                for (std::size_t frame_index = 0U; frame_index < analysis.frames.size(); ++frame_index) {
                    const auto& frame = analysis.frames[frame_index];
                    if (!frame.voiced || !(frame.f0_hz > 0.0)) continue;
                    const double output_position = static_cast<double>(frame.center_sample) * stretch_ratio +
                        (p.openutau_classic ? static_cast<double>(fixed_target) : 0.0);
                    const double source_deviation = 1200.0 * std::log2(frame.f0_hz / source_f0);
                    const double retained_contour = source_deviation * (p.modulation_percent / 100.0);
                    const double bend_cents = pitchBendAt(bend_curve, output_position);
                    const double vibrato_cents = vibratoAt(output_position,
                        static_cast<double>(tail_target_frames), input.sample_rate, expression);
                    pitch_curve_cents[frame_index] = retained_contour + expression.fine_pitch_cents +
                                                     bend_cents + vibrato_cents;
                }

                ResynthesisOptions synthesis_options;
                synthesis_options.pitch_ratio = pitch_ratio;
                synthesis_options.time_stretch_ratio = stretch_ratio;
                const double breath = expression.breathiness_percent / 100.0;
                const double noise_suppression = expression.noise_suppression_percent / 100.0;
                synthesis_options.harmonic_gain = (expression.harmonic_balance_percent / 100.0) *
                    (0.5 + expression.voice_voltage_percent / 200.0) * (1.0 - 0.12 * breath);
                synthesis_options.aperiodic_gain = 0.12 * (expression.noise_level_percent / 100.0) *
                    (1.0 + 3.0 * breath) * (1.0 - 0.90 * noise_suppression);
                synthesis_options.preserve_spectral_envelope = true;
                synthesis_options.edge_fade_ms = 0.0;
                synthesis_options.output_channels = 1U;
                synthesis_options.formant_shift_semitones = effective_formant_shift;
                synthesis_options.voice_voltage_percent = expression.voice_voltage_percent;
                synthesis_options.pitch_curve_cents = std::move(pitch_curve_cents);
                const ResynthesisResult synthesized = resynthesize(analysis, synthesis_options);

                if (synthesized.audio.frames() == tail_target_frames &&
                    synthesized.audio.channels == 1U) {
                    tail_output = std::move(preserved);
                    std::size_t segment = 0U;
                    for (std::size_t frame = 0U; frame < tail_target_frames; ++frame) {
                        const double source_position = static_cast<double>(frame) / stretch_ratio;
                        const double wet = std::clamp(voicedWeightAt(
                            analysis, phonetics, source_position, segment), 0.0, 1.0);
                        const double dry = 1.0 - wet;
                        const double resynth_sample = synthesized.audio.samples[frame];
                        for (std::size_t channel = 0U; channel < tail_output.channels; ++channel) {
                            const double original_sample = tail_output.at(frame, channel);
                            const double mixed = original_sample * dry + resynth_sample * wet;
                            tail_output.at(frame, channel) = static_cast<float>(
                                std::isfinite(mixed) ? mixed : 0.0);
                        }
                    }
                    result.warnings.emplace_back(
                        "Expression oscillator path was used without mixing duplicate dry harmonics into voiced material");
                } else {
                    tail_output = std::move(preserved);
                    result.warnings.emplace_back(
                        "Resynthesis duration did not match the timing target; waveform-preserving WSOLA was used for the tail");
                }
            }
            applyAttackSpeed(tail_output, phonetics, stretch_ratio,
                             expression.attack_speed_percent);
        } else {
            tail_output = std::move(preserved);
            result.warnings.emplace_back(
                "No stable voiced region was detected; the entire non-fixed tail used waveform-preserving time stretching");
        }
    } else if (tail_target_frames > 0U) {
        tail_output.samples.assign(tail_target_frames * tail_output.channels, 0.0F);
        result.warnings.emplace_back("The source has no tail after the fixed region; the requested remainder was filled with silence");
    }

    applyJoinRamp(fixed_output, tail_output);
    result.audio.sample_rate = input.sample_rate;
    result.audio.channels = input.channels;
    result.audio.samples.reserve(target_frames * input.channels);
    append(result.audio, fixed_output);
    append(result.audio, tail_output);
    if (result.audio.frames() != target_frames) {
        throw std::runtime_error("Internal timing error: Phase 5 output frame count does not match requested duration");
    }

    // Apply note-level controls after the fixed and resynthesized regions have
    // been joined, so consonants and vowels share a coherent expression curve.
    applyDynamics(result.audio, expression.dynamics_percent);
    applySpectralBrightness(result.audio, expression.spectral_brightness_percent);
    applyLowpass(result.audio, expression.lowpass_percent);
    applyDecay(result.audio, expression.decay_percent);

    const double gain = (p.volume_percent / 100.0) *
                        (expression.note_gain_percent / 100.0);
    double peak = 0.0;
    for (float& sample : result.audio.samples) {
        const double value = static_cast<double>(sample) * gain;
        sample = static_cast<float>(std::isfinite(value) ? value : 0.0);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    if (peak > 0.98) {
        const double limiter_gain = 0.98 / peak;
        for (float& sample : result.audio.samples) {
            sample = static_cast<float>(static_cast<double>(sample) * limiter_gain);
        }
        result.warnings.emplace_back("A final peak guard reduced the output to prevent clipping");
    }

    for (const auto& unknown : expression.unknown_flags) {
        result.warnings.emplace_back("Ignored unsupported expression flag token: " + unknown);
    }
    return result;
}

ProcessResult renderPhase4(const AudioBuffer& input, const ResamplerParameters& parameters) {
    // Keep the Phase 4 symbol for source compatibility; Phase 5 is a strict
    // superset of the timing/phonetic renderer.
    return renderPhase5(input, parameters);
}

ResynthesisResult resynthesize(const dsp::AnalysisResult& analysis,
                               const ResynthesisOptions& options) {
    if (analysis.sample_rate == 0U || analysis.input_frames == 0U || analysis.frames.empty()) {
        throw std::invalid_argument("Resynthesis requires a complete non-empty analysis result");
    }
    if (!(options.pitch_ratio > 0.0) || !std::isfinite(options.pitch_ratio) ||
        !(options.time_stretch_ratio >= 0.25 && options.time_stretch_ratio <= 4.0) ||
        !std::isfinite(options.time_stretch_ratio) ||
        !(options.harmonic_gain >= 0.0) || !std::isfinite(options.harmonic_gain) ||
        !(options.aperiodic_gain >= 0.0) || !std::isfinite(options.aperiodic_gain) ||
        !(options.peak_limit > 0.0 && options.peak_limit <= 1.0) ||
        !std::isfinite(options.peak_limit) || !(options.edge_fade_ms >= 0.0) ||
        !std::isfinite(options.edge_fade_ms) ||
        (options.output_channels != 1U && options.output_channels != 2U)) {
        throw std::invalid_argument("Invalid resynthesis options");
    }

    dsp::HarmonicSynthesisOptions harmonic_options;
    harmonic_options.pitch_ratio = options.pitch_ratio;
    harmonic_options.time_stretch_ratio = options.time_stretch_ratio;
    harmonic_options.gain = options.harmonic_gain;
    harmonic_options.phase_observation_strength = options.phase_observation_strength;
    harmonic_options.preserve_spectral_envelope = options.preserve_spectral_envelope;
    harmonic_options.maximum_envelope_compensation = options.maximum_envelope_compensation;
    harmonic_options.formant_shift_semitones = options.formant_shift_semitones;
    harmonic_options.voice_voltage_percent = options.voice_voltage_percent;
    harmonic_options.pitch_curve_cents = options.pitch_curve_cents;
    const std::vector<float> harmonic = dsp::synthesizeHarmonicComponent(analysis, harmonic_options);

    dsp::NoiseSynthesisOptions noise_options;
    noise_options.gain = options.aperiodic_gain;
    noise_options.time_stretch_ratio = options.time_stretch_ratio;
    noise_options.random_seed = options.noise_seed;
    noise_options.fft_size = options.noise_fft_size;
    const std::vector<float> aperiodic = dsp::synthesizeAperiodicComponent(analysis, noise_options);

    ResynthesisResult result;
    result.audio.sample_rate = analysis.sample_rate;
    result.audio.channels = options.output_channels;
    const std::size_t output_frames = harmonic.size();
    if (aperiodic.size() != output_frames) {
        throw std::runtime_error("Harmonic and aperiodic synthesis lengths do not match");
    }
    result.audio.samples.resize(output_frames * options.output_channels, 0.0F);

    const double fade_requested = options.edge_fade_ms *
        static_cast<double>(analysis.sample_rate) / 1000.0;
    const double maximum_fade = static_cast<double>(output_frames / 2U);
    const std::size_t fade_frames = static_cast<std::size_t>(std::llround(
        std::clamp(fade_requested, 0.0, maximum_fade)));
    double harmonic_power = 0.0;
    double aperiodic_power = 0.0;
    double mixed_power = 0.0;
    double peak = 0.0;

    for (std::size_t frame = 0U; frame < output_frames; ++frame) {
        double fade = 1.0;
        if (fade_frames > 0U) {
            if (frame < fade_frames) {
                const double phase = static_cast<double>(frame + 1U) /
                                     static_cast<double>(fade_frames);
                fade *= 0.5 - 0.5 * std::cos(std::numbers::pi * phase);
            }
            const std::size_t remaining = output_frames - 1U - frame;
            if (remaining < fade_frames) {
                const double phase = static_cast<double>(remaining + 1U) /
                                     static_cast<double>(fade_frames);
                fade *= 0.5 - 0.5 * std::cos(std::numbers::pi * phase);
            }
        }
        const double h = static_cast<double>(harmonic[frame]) * fade;
        const double n = static_cast<double>(aperiodic[frame]) * fade;
        const double mixed = h + n;
        harmonic_power += h * h;
        aperiodic_power += n * n;
        mixed_power += mixed * mixed;
        peak = std::max(peak, std::abs(mixed));
        for (std::size_t channel = 0U; channel < options.output_channels; ++channel) {
            result.audio.samples[frame * options.output_channels + channel] =
                static_cast<float>(std::isfinite(mixed) ? mixed : 0.0);
        }
    }

    const double frame_count = static_cast<double>(output_frames);
    result.metrics.harmonic_rms = std::sqrt(harmonic_power / frame_count);
    result.metrics.aperiodic_rms = std::sqrt(aperiodic_power / frame_count);
    result.metrics.mixed_rms = std::sqrt(mixed_power / frame_count);
    result.metrics.peak_before_limiter = peak;
    if (peak > options.peak_limit) {
        result.metrics.applied_gain = options.peak_limit / peak;
        result.metrics.peak_limited = true;
        for (float& sample : result.audio.samples) {
            sample = static_cast<float>(static_cast<double>(sample) * result.metrics.applied_gain);
        }
        result.warnings.emplace_back("A global peak guard reduced output gain to prevent clipping");
    }

    if (analysis.summary.voiced_frames == 0U) {
        result.warnings.emplace_back("No reliably voiced frames were detected; output contains only the aperiodic component");
    }
    if (options.pitch_ratio != 1.0 && options.preserve_spectral_envelope) {
        result.warnings.emplace_back("Spectral-envelope compensation is an interpolated cepstral approximation, not a dedicated formant tracker");
    }
    if (options.output_channels == 2U) {
        result.warnings.emplace_back("Analysis is mono-downmixed; stereo output duplicates the reconstructed mono signal");
    }
    return result;
}

} // namespace resynth
