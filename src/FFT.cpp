#include "resynth/FFT.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace resynth::dsp {

bool isPowerOfTwo(const std::size_t value) noexcept {
    return value != 0U && (value & (value - 1U)) == 0U;
}

std::size_t nextPowerOfTwo(const std::size_t value) {
    if (value <= 1U) return 1U;
    std::size_t result = 1U;
    while (result < value) {
        if (result > static_cast<std::size_t>(-1) / 2U) {
            throw std::overflow_error("FFT size is too large");
        }
        result <<= 1U;
    }
    return result;
}

void fft(std::vector<std::complex<double>>& values, const bool inverse) {
    const std::size_t n = values.size();
    if (!isPowerOfTwo(n)) {
        throw std::invalid_argument("FFT input length must be a non-zero power of two");
    }

    // Bit-reversal permutation.
    for (std::size_t i = 1U, j = 0U; i < n; ++i) {
        std::size_t bit = n >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }

    const double direction = inverse ? 1.0 : -1.0;
    for (std::size_t length = 2U; length <= n; length <<= 1U) {
        const double angle = direction * 2.0 * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> root(std::cos(angle), std::sin(angle));
        const std::size_t half = length >> 1U;
        for (std::size_t start = 0U; start < n; start += length) {
            std::complex<double> factor(1.0, 0.0);
            for (std::size_t j = 0U; j < half; ++j) {
                const auto even = values[start + j];
                const auto odd = values[start + j + half] * factor;
                values[start + j] = even + odd;
                values[start + j + half] = even - odd;
                factor *= root;
            }
        }
        if (length == n) break; // Prevent unsigned overflow on the next shift.
    }

    if (inverse) {
        const double scale = 1.0 / static_cast<double>(n);
        for (auto& value : values) value *= scale;
    }
}

} // namespace resynth::dsp
