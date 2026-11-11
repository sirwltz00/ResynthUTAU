#pragma once

#include "resynth/AnalysisEngine.hpp"
#include "resynth/AudioBuffer.hpp"
#include "resynth/ParameterParser.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace resynth {

struct ProcessResult {
    AudioBuffer audio;
    std::vector<std::string> warnings;
    std::size_t voiced_regions{0U};
    std::size_t unvoiced_regions{0U};
    std::size_t transient_regions{0U};
    double estimated_source_f0_hz{0.0};
    double pitch_ratio{1.0};
};

// Phase 1 waveform-domain renderer retained for regression comparisons.
[[nodiscard]] ProcessResult renderPhase1(const AudioBuffer& input,
                                         const ResamplerParameters& parameters);

// Phase 4 timing/phonetic renderer. Fixed consonant material and transient or
// unvoiced regions use waveform-domain WSOLA; voiced regions use time-mapped
// harmonic-plus-aperiodic resynthesis at the requested target note.
[[nodiscard]] ProcessResult renderPhase4(const AudioBuffer& input,
                                         const ResamplerParameters& parameters);

struct ResynthesisOptions {
    double pitch_ratio{1.0};
    double time_stretch_ratio{1.0};
    double harmonic_gain{1.0};
    double aperiodic_gain{1.0};
    double phase_observation_strength{1.0};
    bool preserve_spectral_envelope{true};
    double maximum_envelope_compensation{4.0};
    std::uint64_t noise_seed{0x5253594E544855ULL};
    std::size_t noise_fft_size{2048U};
    std::uint16_t output_channels{1U};
    double peak_limit{0.98};
    double edge_fade_ms{2.0};
};

struct ResynthesisMetrics {
    double harmonic_rms{0.0};
    double aperiodic_rms{0.0};
    double mixed_rms{0.0};
    double peak_before_limiter{0.0};
    double applied_gain{1.0};
    bool peak_limited{false};
};

struct ResynthesisResult {
    AudioBuffer audio;
    ResynthesisMetrics metrics;
    std::vector<std::string> warnings;
};

// Offline resynthesis from a Phase 2 AnalysisResult. Time stretching preserves
// the analyzed F0 while evolving harmonic amplitudes across the requested duration.
[[nodiscard]] ResynthesisResult resynthesize(
    const dsp::AnalysisResult& analysis,
    const ResynthesisOptions& options = {});

} // namespace resynth
