#pragma once

#include <cstdint>
#include <vector>

namespace resynth {

struct AudioBuffer {
    std::uint32_t sample_rate{44100};
    std::uint16_t channels{1};
    std::vector<float> samples; // Interleaved, normalized to [-1, 1].

    [[nodiscard]] std::size_t frames() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] float& at(std::size_t frame, std::size_t channel);
    [[nodiscard]] const float& at(std::size_t frame, std::size_t channel) const;
};

} // namespace resynth
