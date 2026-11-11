#pragma once

#include <complex>
#include <vector>

namespace resynth::dsp {

// In-place radix-2 Cooley-Tukey FFT. The inverse transform is normalized by N.
// Input size must be a non-zero power of two.
void fft(std::vector<std::complex<double>>& values, bool inverse = false);

[[nodiscard]] bool isPowerOfTwo(std::size_t value) noexcept;
[[nodiscard]] std::size_t nextPowerOfTwo(std::size_t value);

} // namespace resynth::dsp
