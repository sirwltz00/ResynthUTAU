#pragma once

#include "resynth/AudioBuffer.hpp"
#include <cstddef>
#include <cstdint>
#include <complex>
#include <vector>

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

// Reusable, per-render workspace. Keep one instance per concurrent render; it is
// intentionally not global so simultaneous UTAU resampler instances cannot race.
struct PitchWorkspace {
    std::vector<double> frame;
    std::vector<double> hann;
    std::vector<std::complex<double>> spectrum;
    std::vector<double> prefix_energy;
    std::vector<double> difference;
    std::vector<double> cmnd;
    std::size_t prepared_window_size{0U};
    std::size_t prepared_fft_size{0U};

    void prepare(std::size_t window_size);
};

// Estimate F0 at a frame center using the YIN cumulative-mean normalized
// difference function. The autocorrelation is computed with the FFT.
// The workspace overload reuses allocations across frames in one render.
[[nodiscard]] PitchEstimate estimatePitch(const AudioBuffer& audio,
                                          std::size_t center_frame,
                                          const PitchConfig& config,
                                          PitchWorkspace& workspace);
[[nodiscard]] PitchEstimate estimatePitch(const AudioBuffer& audio,
                                          std::size_t center_frame,
                                          const PitchConfig& config = {});

} // namespace resynth::dsp
