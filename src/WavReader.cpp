#include "resynth/WavReader.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace resynth {
namespace {

std::uint16_t readU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8U);
}

std::uint32_t readU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) |
           (static_cast<std::uint32_t>(p[3]) << 24U);
}

std::int32_t signExtend24(const std::uint8_t* p) {
    std::uint32_t value = static_cast<std::uint32_t>(p[0]) |
                          (static_cast<std::uint32_t>(p[1]) << 8U) |
                          (static_cast<std::uint32_t>(p[2]) << 16U);
    if ((value & 0x00800000U) != 0U) value |= 0xFF000000U;
    return std::bit_cast<std::int32_t>(value);
}

float decodeSample(const std::uint8_t* p, const std::uint16_t format,
                   const std::uint16_t bits) {
    if (format == 1) { // Integer PCM.
        switch (bits) {
        case 16: {
            const auto value = std::bit_cast<std::int16_t>(readU16(p));
            return static_cast<float>(value) / 32768.0F;
        }
        case 24:
            return static_cast<float>(signExtend24(p)) / 8388608.0F;
        case 32: {
            const auto value = std::bit_cast<std::int32_t>(readU32(p));
            return static_cast<float>(static_cast<double>(value) / 2147483648.0);
        }
        default:
            throw std::runtime_error("Unsupported PCM bit depth (expected 16, 24, or 32)");
        }
    }
    if (format == 3) { // IEEE float.
        if (bits == 32) {
            const std::uint32_t raw = readU32(p);
            const float value = std::bit_cast<float>(raw);
            return std::isfinite(value) ? value : 0.0F;
        }
        if (bits == 64) {
            std::uint64_t raw = 0;
            for (unsigned i = 0; i < 8; ++i) raw |= static_cast<std::uint64_t>(p[i]) << (8U * i);
            const double value = std::bit_cast<double>(raw);
            return std::isfinite(value) ? static_cast<float>(value) : 0.0F;
        }
        throw std::runtime_error("Unsupported IEEE-float bit depth (expected 32 or 64)");
    }
    throw std::runtime_error("Unsupported WAV encoding (expected PCM or IEEE float)");
}

} // namespace

WavReadResult readWav(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open input WAV: " + path.string());

    std::array<std::uint8_t, 12> riff{};
    stream.read(reinterpret_cast<char*>(riff.data()), static_cast<std::streamsize>(riff.size()));
    if (!stream || std::memcmp(riff.data(), "RIFF", 4) != 0 ||
        std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
        throw std::runtime_error("Input is not a RIFF/WAVE file");
    }

    bool have_fmt = false;
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits = 0;
    std::vector<std::uint8_t> data;

    while (stream && (!have_fmt || data.empty())) {
        std::array<std::uint8_t, 8> chunk_header{};
        stream.read(reinterpret_cast<char*>(chunk_header.data()), 8);
        if (!stream) break;
        const std::uint32_t chunk_size = readU32(chunk_header.data() + 4);
        if (chunk_size > 512U * 1024U * 1024U) {
            throw std::runtime_error("WAV chunk is unreasonably large");
        }
        if (std::memcmp(chunk_header.data(), "fmt ", 4) == 0) {
            if (chunk_size < 16) throw std::runtime_error("Invalid WAV fmt chunk");
            std::vector<std::uint8_t> fmt(chunk_size);
            stream.read(reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(fmt.size()));
            if (!stream) throw std::runtime_error("Truncated WAV fmt chunk");
            format = readU16(fmt.data());
            channels = readU16(fmt.data() + 2);
            sample_rate = readU32(fmt.data() + 4);
            block_align = readU16(fmt.data() + 12);
            bits = readU16(fmt.data() + 14);
            if (format == 0xFFFE) {
                throw std::runtime_error("WAVE_FORMAT_EXTENSIBLE is not yet supported");
            }
            have_fmt = true;
        } else if (std::memcmp(chunk_header.data(), "data", 4) == 0) {
            data.resize(chunk_size);
            if (chunk_size > 0) {
                stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                if (!stream) throw std::runtime_error("Truncated WAV data chunk");
            }
        } else {
            stream.seekg(static_cast<std::streamoff>(chunk_size), std::ios::cur);
            if (!stream) throw std::runtime_error("Invalid WAV chunk size");
        }
        if ((chunk_size & 1U) != 0U) stream.seekg(1, std::ios::cur);
    }

    if (!have_fmt || data.empty()) throw std::runtime_error("WAV is missing fmt or non-empty data chunk");
    if (channels != 1 && channels != 2) throw std::runtime_error("Only mono and stereo WAV files are supported");
    if (sample_rate == 0 || bits == 0 || block_align == 0) throw std::runtime_error("Invalid WAV format metadata");
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>((bits + 7U) / 8U);
    if (block_align != channels * bytes_per_sample) throw std::runtime_error("Inconsistent WAV block alignment");
    if (data.size() % block_align != 0) throw std::runtime_error("WAV data size is not frame-aligned");

    const std::size_t frame_count = data.size() / block_align;
    if (frame_count > std::numeric_limits<std::size_t>::max() / channels) {
        throw std::runtime_error("WAV is too large for this platform");
    }

    AudioBuffer audio;
    audio.sample_rate = sample_rate;
    audio.channels = channels;
    audio.samples.resize(frame_count * channels);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const std::size_t offset = frame * block_align + channel * bytes_per_sample;
            float value = decodeSample(data.data() + offset, format, bits);
            if (!std::isfinite(value)) value = 0.0F;
            audio.samples[frame * channels + channel] = value;
        }
    }
    return {std::move(audio), format, bits};
}

} // namespace resynth
