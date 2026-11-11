#pragma once

#include "resynth/AudioBuffer.hpp"
#include "resynth/HarmonicAnalyzer.hpp"
#include "resynth/PitchAnalyzer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace resynth::dsp {

struct AnalysisConfig {
    std::size_t hop_size{256U};
    std::size_t pitch_window_size{4096U};
    std::size_t minimum_spectral_window{1024U};
    std::size_t maximum_spectral_window{4096U};
    double target_cycles_per_window{8.0};
    double min_f0_hz{65.0};
    double max_f0_hz{1000.0};
    double yin_threshold{0.18};
    double minimum_periodicity{0.65};
    double minimum_rms{0.0015};
    double cepstral_lifter_ms{1.5};
    std::size_t maximum_harmonics{128U};
    std::size_t profile_bands{256U};
};

struct AnalysisFrame {
    std::size_t center_sample{0U};
    std::size_t window_size{0U};
    double rms{0.0};
    double f0_hz{0.0};
    double periodicity{0.0};
    double yin_cmnd{1.0};
    bool voiced{false};
    double harmonic_energy{0.0};
    double aperiodic_energy{0.0};
    double aperiodicity_ratio{1.0};
    double spectral_centroid_hz{0.0};
    double spectral_flatness{0.0};
    std::vector<HarmonicPartial> harmonics;
    // Log-frequency profiles, ascending from low to high frequency.
    std::vector<float> spectral_envelope_bands;
    std::vector<float> aperiodic_magnitude_bands;
};

struct AnalysisSummary {
    std::size_t total_frames{0U};
    std::size_t voiced_frames{0U};
    std::size_t unvoiced_frames{0U};
    double average_f0_hz{0.0};
    double minimum_f0_hz{0.0};
    double maximum_f0_hz{0.0};
    double average_aperiodicity{0.0};
    double average_spectral_centroid_hz{0.0};
};

struct AnalysisResult {
    std::uint32_t sample_rate{0U};
    std::size_t input_frames{0U};
    AnalysisConfig config;
    AnalysisSummary summary;
    std::vector<AnalysisFrame> frames;
};

[[nodiscard]] AnalysisResult analyzeAudio(const AudioBuffer& audio,
                                          const AnalysisConfig& config = {});
[[nodiscard]] std::string analysisToJson(const AnalysisResult& analysis,
                                         bool include_frames = true);

} // namespace resynth::dsp
