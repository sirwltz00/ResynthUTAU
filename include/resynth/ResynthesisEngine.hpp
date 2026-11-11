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
};

// Phase 1 waveform-domain duration renderer retained for the conventional
// UTAU invocation until timing/phonetic integration is implemented.
[[nodiscard]] ProcessResult renderPhase1(const AudioBuffer& input,
                                         const ResamplerParameters& parameters);

struct ResynthesisOptions {
    double pitch_ratio{1.0};
    double harmonic_gain{1.0};
    double aperiodic_gain{1.0};
    double phase_observation_strength{0.85};
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

// Offline Phase 3 resynthesis from a Phase 2 AnalysisResult. Output duration
// equals analysis.input_frames. This is a standalone synthesis API; the UTAU
// renderer remains on Phase 1 timing until Phase 4 integration.
[[nodiscard]] ResynthesisResult resynthesize(
    const dsp::AnalysisResult& analysis,
    const ResynthesisOptions& options = {});

} // namespace resynth
