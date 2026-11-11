#pragma once

#include "resynth/AudioBuffer.hpp"
#include <cstddef>
#include <cstdint>

namespace resynth::dsp {

struct PitchEstimate {
    double f0_hz{0.0};
    double periodicity{0.0};
    double rms{0.0};
    double yin_cmnd{1.0};
    bool voiced{false};
};

struct PitchConfig {
    std::size_t window_size{4096};
    double min_f0_hz{65.0};
    double max_f0_hz{1000.0};
    double yin_threshold{0.18};
    double minimum_periodicity{0.65};
    double minimum_rms{0.0015};
};

// Estimate F0 at a frame center using the YIN cumulative-mean normalized
// difference function. The autocorrelation is computed with the FFT.
[[nodiscard]] PitchEstimate estimatePitch(const AudioBuffer& audio,
                                          std::size_t center_frame,
                                          const PitchConfig& config = {});

} // namespace resynth::dsp
