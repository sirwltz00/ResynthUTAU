#include "resynth/PhoneticAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resynth::dsp {
namespace {

bool isTransientRise(const AnalysisFrame& previous, const AnalysisFrame& current) {
    const double denominator = std::max(previous.rms, 0.0008);
    const double rise = current.rms / denominator;
    const double centroid_rise = current.spectral_centroid_hz - previous.spectral_centroid_hz;
    // Require both a sharp energy attack and a broad/high-frequency spectrum.
    // This deliberately errs on the side of preserving possible consonants.
    return rise >= 2.2 && current.spectral_flatness >= 0.12 &&
           (current.spectral_centroid_hz >= 1400.0 || centroid_rise >= 900.0);
}

std::size_t frameBoundary(const AnalysisResult& analysis, const std::size_t right_frame) {
    if (right_frame == 0U) return 0U;
    const std::size_t left_center = analysis.frames[right_frame - 1U].center_sample;
    const std::size_t right_center = analysis.frames[right_frame].center_sample;
    return left_center + (right_center - left_center) / 2U;
}

} // namespace

PhoneticAnalysis classifyPhonetics(const AnalysisResult& analysis) {
    PhoneticAnalysis result;
    const std::size_t count = analysis.frames.size();
    result.frame_kinds.resize(count, PhoneticKind::Silence);
    result.voiced_weights.resize(count, 0.0);
    if (count == 0U || analysis.input_frames == 0U) return result;

    for (std::size_t i = 0U; i < count; ++i) {
        const auto& frame = analysis.frames[i];
        if (!(frame.rms >= analysis.config.minimum_rms)) {
            result.frame_kinds[i] = PhoneticKind::Silence;
            continue;
        }
        if (i > 0U && isTransientRise(analysis.frames[i - 1U], frame)) {
            result.frame_kinds[i] = PhoneticKind::Transient;
            continue;
        }
        const bool locally_aperiodic = frame.aperiodicity_ratio >= 0.45 ||
                                       frame.spectral_flatness >= 0.30;
        result.frame_kinds[i] = frame.voiced && frame.f0_hz > 0.0 && !locally_aperiodic
            ? PhoneticKind::Voiced : PhoneticKind::Unvoiced;
    }

    std::vector<double> raw_weights(count, 0.0);
    for (std::size_t i = 0U; i < count; ++i) {
        if (result.frame_kinds[i] != PhoneticKind::Voiced) continue;
        const double confidence = analysis.frames[i].periodicity;
        raw_weights[i] = std::clamp((confidence - 0.35) / 0.55, 0.0, 1.0);
    }

    // A short triangular smoothing window avoids abrupt synthesis gates at
    // consonant/vowel boundaries. Transient frames are then hard-protected.
    for (std::size_t i = 0U; i < count; ++i) {
        double weighted_sum = 0.0;
        double weight_sum = 0.0;
        for (int delta = -2; delta <= 2; ++delta) {
            const auto signed_index = static_cast<std::ptrdiff_t>(i) + delta;
            if (signed_index < 0 || static_cast<std::size_t>(signed_index) >= count) continue;
            const double weight = static_cast<double>(3 - std::abs(delta));
            weighted_sum += raw_weights[static_cast<std::size_t>(signed_index)] * weight;
            weight_sum += weight;
        }
        result.voiced_weights[i] = weight_sum > 0.0 ? weighted_sum / weight_sum : 0.0;
        if (result.frame_kinds[i] == PhoneticKind::Transient ||
            result.frame_kinds[i] == PhoneticKind::Unvoiced ||
            result.frame_kinds[i] == PhoneticKind::Silence) {
            result.voiced_weights[i] = 0.0;
        }
    }

    std::size_t region_begin = 0U;
    for (std::size_t i = 1U; i <= count; ++i) {
        if (i < count && result.frame_kinds[i] == result.frame_kinds[region_begin]) continue;
        const std::size_t begin = region_begin == 0U ? 0U : frameBoundary(analysis, region_begin);
        const std::size_t end = i == count ? analysis.input_frames : frameBoundary(analysis, i);
        if (end > begin) result.regions.push_back({result.frame_kinds[region_begin], begin, end});
        region_begin = i;
    }
    return result;
}

} // namespace resynth::dsp
