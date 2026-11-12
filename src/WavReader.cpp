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

constexpr std::uint32_t kMaximumDataChunkBytes = 512U * 1024U * 1024U;

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
    if (format == 1U) { // Integer PCM.
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
    if (format == 3U) { // IEEE float.
        if (bits == 32U) {
            const float value = std::bit_cast<float>(readU32(p));
            return std::isfinite(value) ? value : 0.0F;
        }
        if (bits == 64U) {
            std::uint64_t raw = 0U;
            for (unsigned i = 0U; i < 8U; ++i) {
                raw |= static_cast<std::uint64_t>(p[i]) << (8U * i);
            }
            const double value = std::bit_cast<double>(raw);
            return std::isfinite(value) ? static_cast<float>(value) : 0.0F;
        }
        throw std::runtime_error("Unsupported IEEE-float bit depth (expected 32 or 64)");
    }
    throw std::runtime_error("Unsupported WAV encoding (expected PCM or IEEE float)");
}

struct FormatInfo {
    std::uint16_t format{0U};
    std::uint16_t channels{0U};
    std::uint32_t sample_rate{0U};
    std::uint16_t block_align{0U};
    std::uint16_t bits{0U};
    std::uint16_t valid_bits{0U};
};

FormatInfo parseFormatChunk(const std::vector<std::uint8_t>& fmt) {
    if (fmt.size() < 16U) throw std::runtime_error("Invalid WAV fmt chunk");
    FormatInfo info;
    info.format = readU16(fmt.data());
    info.channels = readU16(fmt.data() + 2U);
    info.sample_rate = readU32(fmt.data() + 4U);
    info.block_align = readU16(fmt.data() + 12U);
    info.bits = readU16(fmt.data() + 14U);
    info.valid_bits = info.bits;

    if (info.format == 0xFFFEU) { // WAVE_FORMAT_EXTENSIBLE.
        if (fmt.size() < 40U || readU16(fmt.data() + 16U) < 22U) {
            throw std::runtime_error("Invalid WAVE_FORMAT_EXTENSIBLE fmt extension");
        }
        info.valid_bits = readU16(fmt.data() + 18U);
        const std::uint32_t subformat = readU32(fmt.data() + 24U);
        static constexpr std::array<std::uint8_t, 12> guid_tail{
            0x00U, 0x00U, 0x10U, 0x00U, 0x80U, 0x00U,
            0x00U, 0xAAU, 0x00U, 0x38U, 0x9BU, 0x71U
        };
        if (std::memcmp(fmt.data() + 28U, guid_tail.data(), guid_tail.size()) != 0 ||
            (subformat != 1U && subformat != 3U)) {
            throw std::runtime_error("Unsupported WAVE_FORMAT_EXTENSIBLE subformat GUID");
        }
        info.format = static_cast<std::uint16_t>(subformat);
        if (info.valid_bits == 0U || info.valid_bits > info.bits) {
            throw std::runtime_error("Invalid WAVE_FORMAT_EXTENSIBLE valid-bits value");
        }
    }

    if (info.channels != 1U && info.channels != 2U) {
        throw std::runtime_error("Only mono and stereo WAV files are supported");
    }
    if (info.sample_rate == 0U || info.bits == 0U || info.block_align == 0U) {
        throw std::runtime_error("Invalid WAV format metadata");
    }
    if (info.format == 1U) {
        if (info.bits != 16U && info.bits != 24U && info.bits != 32U) {
            throw std::runtime_error("Unsupported PCM bit depth (expected 16, 24, or 32)");
        }
    } else if (info.format == 3U) {
        if ((info.bits != 32U && info.bits != 64U) || info.valid_bits != info.bits) {
            throw std::runtime_error("Unsupported IEEE-float bit depth or valid-bits value");
        }
    } else {
        throw std::runtime_error("Unsupported WAV encoding (expected PCM or IEEE float)");
    }
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(info.bits / 8U);
    const std::uint32_t expected_align = static_cast<std::uint32_t>(info.channels) * bytes_per_sample;
    if (expected_align != info.block_align) {
        throw std::runtime_error("Inconsistent WAV block alignment");
    }
    return info;
}

} // namespace

WavReadResult readWav(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open input WAV: " + path.string());

    stream.seekg(0, std::ios::end);
    const std::streamoff file_size = stream.tellg();
    if (file_size < 12) throw std::runtime_error("Input WAV is shorter than a RIFF header");
    stream.seekg(0, std::ios::beg);

    std::array<std::uint8_t, 12> riff{};
    stream.read(reinterpret_cast<char*>(riff.data()), static_cast<std::streamsize>(riff.size()));
    if (!stream || std::memcmp(riff.data(), "RIFF", 4U) != 0 ||
        std::memcmp(riff.data() + 8U, "WAVE", 4U) != 0) {
        if (std::memcmp(riff.data(), "RF64", 4U) == 0) {
            throw std::runtime_error("RF64 WAV is not supported; use classic RIFF/WAVE below 4 GiB");
        }
        throw std::runtime_error("Input is not a RIFF/WAVE file");
    }

    const std::uint32_t riff_size = readU32(riff.data() + 4U);
    if (riff_size < 4U) throw std::runtime_error("Invalid RIFF/WAVE container size");
    const std::uint64_t declared_riff_end = 8ULL + static_cast<std::uint64_t>(riff_size);
    const std::uint64_t actual_file_size = static_cast<std::uint64_t>(file_size);
    // Some WAV writers mistakenly store the total file length in the RIFF size
    // field (an overstatement of exactly eight bytes). Accept only this narrowly
    // defined legacy error and clamp to EOF; larger overruns are rejected.
    const std::uint64_t riff_end = declared_riff_end > actual_file_size &&
        declared_riff_end - actual_file_size == 8ULL
        ? actual_file_size : declared_riff_end;
    if (riff_end > actual_file_size) {
        throw std::runtime_error("RIFF/WAVE container extends beyond the input file");
    }

    bool have_fmt = false;
    FormatInfo format{};
    std::vector<std::uint8_t> data;
    while (true) {
        const std::streamoff chunk_header_pos = stream.tellg();
        if (chunk_header_pos < 0) throw std::runtime_error("Invalid WAV chunk position");
        const auto position = static_cast<std::uint64_t>(chunk_header_pos);
        if (position == riff_end) break;
        if (position > riff_end || riff_end - position < 8ULL) {
            throw std::runtime_error("Truncated RIFF/WAVE chunk header");
        }

        std::array<std::uint8_t, 8> chunk_header{};
        stream.read(reinterpret_cast<char*>(chunk_header.data()), 8);
        if (!stream) throw std::runtime_error("Truncated RIFF/WAVE chunk header");
        const std::uint32_t chunk_size = readU32(chunk_header.data() + 4U);
        const std::uint64_t payload_pos = position + 8ULL;
        const std::uint64_t padded_size = static_cast<std::uint64_t>(chunk_size) + (chunk_size & 1U);
        if (padded_size > riff_end - payload_pos) {
            throw std::runtime_error("WAV chunk extends beyond the RIFF container");
        }

        if (std::memcmp(chunk_header.data(), "fmt ", 4U) == 0 && !have_fmt) {
            if (chunk_size > 4096U) throw std::runtime_error("WAV fmt chunk is unreasonably large");
            std::vector<std::uint8_t> fmt(chunk_size);
            if (!fmt.empty()) {
                stream.read(reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(fmt.size()));
                if (!stream) throw std::runtime_error("Truncated WAV fmt chunk");
            }
            format = parseFormatChunk(fmt);
            have_fmt = true;
        } else if (std::memcmp(chunk_header.data(), "data", 4U) == 0 && data.empty()) {
            if (chunk_size == 0U) throw std::runtime_error("WAV data chunk is empty");
            if (chunk_size > kMaximumDataChunkBytes) {
                throw std::runtime_error("WAV data chunk is unreasonably large (over 512 MiB)");
            }
            data.resize(chunk_size);
            stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!stream) throw std::runtime_error("Truncated WAV data chunk");
        } else {
            stream.seekg(static_cast<std::streamoff>(chunk_size), std::ios::cur);
            if (!stream) throw std::runtime_error("Invalid WAV chunk size");
        }
        if ((chunk_size & 1U) != 0U) {
            stream.seekg(1, std::ios::cur);
            if (!stream) throw std::runtime_error("Missing WAV chunk padding byte");
        }
        if (have_fmt && !data.empty()) break;
    }

    if (!have_fmt || data.empty()) throw std::runtime_error("WAV is missing fmt or non-empty data chunk");
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(format.bits / 8U);
    if (data.size() % format.block_align != 0U) {
        throw std::runtime_error("WAV data size is not frame-aligned");
    }

    const std::size_t frame_count = data.size() / format.block_align;
    if (frame_count > std::numeric_limits<std::size_t>::max() / format.channels) {
        throw std::runtime_error("WAV is too large for this platform");
    }

    AudioBuffer audio;
    audio.sample_rate = format.sample_rate;
    audio.channels = format.channels;
    audio.samples.resize(frame_count * format.channels);
    for (std::size_t frame = 0U; frame < frame_count; ++frame) {
        for (std::size_t channel = 0U; channel < format.channels; ++channel) {
            const std::size_t offset = frame * format.block_align + channel * bytes_per_sample;
            float value = decodeSample(data.data() + offset, format.format, format.bits);
            if (!std::isfinite(value)) value = 0.0F;
            audio.samples[frame * format.channels + channel] = value;
        }
    }
    return {std::move(audio), format.format, format.bits};
}

} // namespace resynth
