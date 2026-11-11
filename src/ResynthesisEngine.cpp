#include "resynth/ResynthesisEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
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

} // namespace resynth
