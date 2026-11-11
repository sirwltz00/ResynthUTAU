#include "resynth/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace resynth {
namespace {

void writeU16(std::ostream& stream, const std::uint16_t value) {
    const char bytes[2] = {static_cast<char>(value & 0xFFU), static_cast<char>((value >> 8U) & 0xFFU)};
    stream.write(bytes, 2);
}

void writeU32(std::ostream& stream, const std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xFFU),
                            static_cast<char>((value >> 8U) & 0xFFU),
                            static_cast<char>((value >> 16U) & 0xFFU),
                            static_cast<char>((value >> 24U) & 0xFFU)};
    stream.write(bytes, 4);
}

} // namespace

void writeWav16(const std::filesystem::path& path, const AudioBuffer& audio) {
    if (!audio.valid() || audio.frames() == 0) throw std::runtime_error("Cannot write an empty or invalid audio buffer");
    const std::uint64_t data_size_64 = static_cast<std::uint64_t>(audio.frames()) * audio.channels * 2ULL;
    if (data_size_64 > std::numeric_limits<std::uint32_t>::max() - 36ULL) {
        throw std::runtime_error("Output exceeds classic RIFF/WAV size limit (4 GiB)");
    }
    const auto data_size = static_cast<std::uint32_t>(data_size_64);
    const std::uint32_t byte_rate = audio.sample_rate * static_cast<std::uint32_t>(audio.channels) * 2U;
    const std::uint16_t block_align = static_cast<std::uint16_t>(audio.channels * 2U);

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Cannot create output WAV: " + path.string());
    stream.write("RIFF", 4);
    writeU32(stream, 36U + data_size);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    writeU32(stream, 16);
    writeU16(stream, 1);
    writeU16(stream, audio.channels);
    writeU32(stream, audio.sample_rate);
    writeU32(stream, byte_rate);
    writeU16(stream, block_align);
    writeU16(stream, 16);
    stream.write("data", 4);
    writeU32(stream, data_size);

    for (const float sample : audio.samples) {
        const float finite_sample = std::isfinite(sample) ? sample : 0.0F;
        const float clamped = std::clamp(finite_sample, -1.0F, 1.0F);
        const long rounded = std::lround(static_cast<double>(clamped) * 32767.0);
        const auto value = static_cast<std::int16_t>(std::clamp(rounded, -32768L, 32767L));
        writeU16(stream, static_cast<std::uint16_t>(value));
    }
    stream.flush();
    if (!stream) throw std::runtime_error("Failed while writing output WAV: " + path.string());
}

} // namespace resynth
