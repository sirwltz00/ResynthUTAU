#include "resynth/AudioBuffer.hpp"

#include <stdexcept>

namespace resynth {

std::size_t AudioBuffer::frames() const noexcept {
    return channels == 0 ? 0 : samples.size() / static_cast<std::size_t>(channels);
}

bool AudioBuffer::valid() const noexcept {
    return sample_rate > 0 && (channels == 1 || channels == 2) &&
           samples.size() % static_cast<std::size_t>(channels) == 0;
}

float& AudioBuffer::at(const std::size_t frame, const std::size_t channel) {
    if (frame >= frames() || channel >= channels) {
        throw std::out_of_range("AudioBuffer index out of range");
    }
    return samples[frame * static_cast<std::size_t>(channels) + channel];
}

const float& AudioBuffer::at(const std::size_t frame, const std::size_t channel) const {
    if (frame >= frames() || channel >= channels) {
        throw std::out_of_range("AudioBuffer index out of range");
    }
    return samples[frame * static_cast<std::size_t>(channels) + channel];
}

} // namespace resynth
