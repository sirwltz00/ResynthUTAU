#include "resynth/AudioBuffer.hpp"
#include "resynth/AnalysisEngine.hpp"
#include "resynth/FFT.hpp"
#include "resynth/NoiseSynthesizer.hpp"
#include "resynth/ParameterParser.hpp"
#include "resynth/PhoneticAnalyzer.hpp"
#include "resynth/ResynthesisEngine.hpp"
#include "resynth/WavReader.hpp"
#include "resynth/WavWriter.hpp"
#include "resynth/UTAUInterface.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <numeric>
#include <random>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

resynth::AudioBuffer makeTone(const std::uint32_t sample_rate, const std::uint16_t channels,
                              const std::size_t frames) {
    resynth::AudioBuffer audio;
    audio.sample_rate = sample_rate;
    audio.channels = channels;
    audio.samples.resize(frames * channels);
    for (std::size_t i = 0; i < frames; ++i) {
        const float value = 0.35F * std::sin(2.0F * 3.14159265358979323846F * 440.0F *
                                             static_cast<float>(i) / static_cast<float>(sample_rate));
        for (std::size_t ch = 0; ch < channels; ++ch) audio.at(i, ch) = value * (ch == 0 ? 1.0F : 0.75F);
    }
    return audio;
}

void testAudioBuffer() {
    auto audio = makeTone(48000, 2, 480);
    require(audio.valid(), "AudioBuffer should validate");
    require(audio.frames() == 480, "AudioBuffer frame count incorrect");
    audio.at(0, 0) = 0.25F;
    require(std::abs(audio.at(0, 0) - 0.25F) < 1e-7F, "AudioBuffer indexing failed");
}

void testWavRoundTrip() {
    const auto path = std::filesystem::temp_directory_path() / "resynthutau_roundtrip.wav";
    const auto original = makeTone(44100, 2, 4410);
    resynth::writeWav16(path, original);
    const auto decoded = resynth::readWav(path);
    require(decoded.audio.sample_rate == 44100, "WAV sample rate mismatch");
    require(decoded.audio.channels == 2, "WAV channel count mismatch");
    require(decoded.audio.frames() == original.frames(), "WAV duration mismatch");
    require(std::abs(decoded.audio.samples[100] - original.samples[100]) < 0.0001F,
            "WAV PCM16 round-trip error too large");
    std::filesystem::remove(path);
}


void appendU16(std::vector<std::uint8_t>& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void appendChunk(std::vector<std::uint8_t>& file, const char* id,
                 const std::vector<std::uint8_t>& payload) {
    file.insert(file.end(), id, id + 4);
    appendU32(file, static_cast<std::uint32_t>(payload.size()));
    file.insert(file.end(), payload.begin(), payload.end());
    if ((payload.size() & 1U) != 0U) file.push_back(0U);
}

void writeWaveFixture(const std::filesystem::path& path,
                      const std::vector<std::uint8_t>& fmt,
                      const std::vector<std::uint8_t>& data,
                      const bool extensible_data_first = false,
                      const bool add_odd_junk = false,
                      const bool corrupt_riff_size = false,
                      const bool riff_size_includes_header = false) {
    std::vector<std::uint8_t> file{'R','I','F','F',0U,0U,0U,0U,'W','A','V','E'};
    if (add_odd_junk) appendChunk(file, "JUNK", {0x42U});
    if (extensible_data_first) {
        appendChunk(file, "data", data);
        appendChunk(file, "fmt ", fmt);
    } else {
        appendChunk(file, "fmt ", fmt);
        appendChunk(file, "data", data);
    }
    const std::uint32_t riff_size = static_cast<std::uint32_t>(file.size() - 8U);
    const std::uint32_t stored_size = corrupt_riff_size ? riff_size + 128U :
        (riff_size_includes_header ? riff_size + 8U : riff_size);
    for (unsigned i = 0U; i < 4U; ++i) {
        file[4U + i] = static_cast<std::uint8_t>((stored_size >> (i * 8U)) & 0xFFU);
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!stream) throw std::runtime_error("Could not write WAV test fixture");
}

std::vector<std::uint8_t> makeFmtChunk(const std::uint16_t format,
                                       const std::uint16_t channels,
                                       const std::uint32_t sample_rate,
                                       const std::uint16_t bits,
                                       const bool extensible = false,
                                       const std::uint16_t valid_bits = 0U) {
    std::vector<std::uint8_t> fmt;
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(bits / 8U);
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels * bytes_per_sample);
    appendU16(fmt, extensible ? 0xFFFEU : format);
    appendU16(fmt, channels);
    appendU32(fmt, sample_rate);
    appendU32(fmt, sample_rate * block_align);
    appendU16(fmt, block_align);
    appendU16(fmt, bits);
    if (extensible) {
        appendU16(fmt, 22U);
        appendU16(fmt, valid_bits == 0U ? bits : valid_bits);
        appendU32(fmt, channels == 1U ? 0x4U : 0x3U);
        appendU32(fmt, format);
        constexpr std::array<std::uint8_t, 12> tail{
            0x00U,0x00U,0x10U,0x00U,0x80U,0x00U,
            0x00U,0xAAU,0x00U,0x38U,0x9BU,0x71U
        };
        fmt.insert(fmt.end(), tail.begin(), tail.end());
    }
    return fmt;
}

void testWavExtendedCompatibility() {
    const auto base = std::filesystem::temp_directory_path();

    // WAVE_FORMAT_EXTENSIBLE PCM24, with data before fmt and an odd-sized JUNK
    // chunk. This exercises RIFF chunk traversal, padding, and the PCM subtype GUID.
    const auto pcm24_path = base / "resynthutau_extensible_pcm24.wav";
    const std::vector<std::uint8_t> pcm24_data{
        0x00U,0x00U,0x80U, // -1.0
        0x00U,0x00U,0x40U  // +0.5
    };
    writeWaveFixture(pcm24_path, makeFmtChunk(1U, 1U, 48000U, 24U, true),
                     pcm24_data, true, true);
    const auto pcm24 = resynth::readWav(pcm24_path);
    require(pcm24.audio.sample_rate == 48000U && pcm24.audio.channels == 1U,
            "WAVE_FORMAT_EXTENSIBLE PCM24 metadata was not decoded");
    require(pcm24.source_format == 1U && pcm24.source_bits_per_sample == 24U,
            "WAVE_FORMAT_EXTENSIBLE PCM24 source metadata was not normalized");
    require(pcm24.audio.frames() == 2U && std::abs(pcm24.audio.samples[0] + 1.0F) < 1e-6F &&
            std::abs(pcm24.audio.samples[1] - 0.5F) < 1e-6F,
            "WAVE_FORMAT_EXTENSIBLE PCM24 samples were decoded incorrectly");
    std::filesystem::remove(pcm24_path);

    // WAVE_FORMAT_EXTENSIBLE IEEE float32.
    const auto float_path = base / "resynthutau_extensible_float32.wav";
    std::vector<std::uint8_t> float_data;
    appendU32(float_data, std::bit_cast<std::uint32_t>(0.25F));
    appendU32(float_data, std::bit_cast<std::uint32_t>(-0.5F));
    writeWaveFixture(float_path, makeFmtChunk(3U, 1U, 44100U, 32U, true), float_data);
    const auto float_audio = resynth::readWav(float_path);
    require(float_audio.source_format == 3U && float_audio.audio.frames() == 2U,
            "WAVE_FORMAT_EXTENSIBLE IEEE float32 header failed");
    require(std::abs(float_audio.audio.samples[0] - 0.25F) < 1e-7F &&
            std::abs(float_audio.audio.samples[1] + 0.5F) < 1e-7F,
            "WAVE_FORMAT_EXTENSIBLE IEEE float32 samples were decoded incorrectly");
    std::filesystem::remove(float_path);

    // Standard PCM32 and IEEE float64 remain supported.
    const auto pcm32_path = base / "resynthutau_pcm32.wav";
    std::vector<std::uint8_t> pcm32_data;
    appendU32(pcm32_data, 0x40000000U);
    appendU32(pcm32_data, 0xC0000000U);
    writeWaveFixture(pcm32_path, makeFmtChunk(1U, 1U, 32000U, 32U), pcm32_data);
    const auto pcm32 = resynth::readWav(pcm32_path);
    require(std::abs(pcm32.audio.samples[0] - 0.5F) < 1e-7F &&
            std::abs(pcm32.audio.samples[1] + 0.5F) < 1e-7F,
            "PCM32 samples were decoded incorrectly");
    std::filesystem::remove(pcm32_path);

    const auto float64_path = base / "resynthutau_float64.wav";
    std::vector<std::uint8_t> float64_data;
    appendU32(float64_data, static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(0.125)));
    appendU32(float64_data, static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(0.125) >> 32U));
    appendU32(float64_data, static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(-0.75)));
    appendU32(float64_data, static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(-0.75) >> 32U));
    writeWaveFixture(float64_path, makeFmtChunk(3U, 1U, 22050U, 64U), float64_data);
    const auto float64 = resynth::readWav(float64_path);
    require(std::abs(float64.audio.samples[0] - 0.125F) < 1e-7F &&
            std::abs(float64.audio.samples[1] + 0.75F) < 1e-7F,
            "IEEE float64 samples were decoded incorrectly");
    std::filesystem::remove(float64_path);

    const auto legacy_path = base / "resynthutau_legacy_riff_size.wav";
    writeWaveFixture(legacy_path, makeFmtChunk(1U, 1U, 44100U, 16U),
                     {0x00U,0x20U}, false, false, false, true);
    const auto legacy = resynth::readWav(legacy_path);
    require(legacy.audio.frames() == 1U,
            "Reader should accept the common RIFF size field that includes the eight-byte header");
    std::filesystem::remove(legacy_path);

    const auto malformed_path = base / "resynthutau_bad_riff_size.wav";
    writeWaveFixture(malformed_path, makeFmtChunk(1U, 1U, 44100U, 16U),
                     {0U,0U}, false, false, true);
    bool rejected = false;
    try { (void)resynth::readWav(malformed_path); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "Malformed RIFF container length must be rejected");
    std::filesystem::remove(malformed_path);
}

void testParameterParser() {
    const std::vector<std::string> args = {
        "voice.wav", "out.wav", "F#4", "100", "-", "10.5", "250", "75", "5", "80", "0"
    };
    const auto parsed = resynth::parseArguments(args);
    require(parsed.target_midi == 66, "F#4 should parse to MIDI 66");
    require(parsed.offset_ms == 10.5, "Offset parse failed");
    require(parsed.length_required_ms && *parsed.length_required_ms == 250.0, "Length parse failed");
    require(parsed.fixed_length_ms == 75.0, "Fixed length parse failed");
    require(parsed.volume_percent == 80.0, "Volume parse failed");
    require(parsed.target_pitch_is_note && parsed.target_midi == 66,
            "A named pitch argument should be interpreted as a target note");

    const auto percent_pitch = resynth::parseArguments({
        "voice.wav", "out.wav", "120", "100"
    });
    require(!percent_pitch.target_pitch_is_note && percent_pitch.pitch_percent == 120.0,
            "A numeric third argument should be interpreted as pitch percent");
    const auto unity_pitch = resynth::parseArguments({
        "voice.wav", "out.wav", "100", "100"
    });
    require(!unity_pitch.target_pitch_is_note && unity_pitch.pitch_percent == 100.0,
            "100 percent should represent unchanged pitch ratio");
}

void testOpenUtauClassicArgumentAdapter() {
    const auto parsed = resynth::parseArguments({
        "voice input.wav", "render output.wav", "A3", "100", "g10br20",
        "100", "500", "40", "-400", "85", "0", "!120", "AAAB#2#//"
    });
    require(parsed.openutau_classic,
            "The 13-argument !tempo invocation should be detected as OpenUtau Classic");
    require(parsed.offset_ms == 100.0 && parsed.length_required_ms &&
            *parsed.length_required_ms == 500.0 && parsed.fixed_length_ms == 40.0,
            "OpenUtau timing arguments were mapped incorrectly");
    require(parsed.openutau_cutoff_ms == -400.0,
            "OpenUtau signed cutoff must be preserved rather than coerced to end-blank");
    require(parsed.volume_percent == 85.0 && parsed.modulation_percent == 0.0,
            "OpenUtau volume/modulation arguments were mapped incorrectly");
    require(parsed.pitch_points_on_output_timeline,
            "OpenUtau pitch samples must be treated as output-timeline points");
    require(parsed.pitch_bend_tokens.size() == 6U && parsed.pitch_bend_tokens[0] == "Q120" &&
            parsed.pitch_bend_tokens[1] == "0" && parsed.pitch_bend_tokens[2] == "1" &&
            parsed.pitch_bend_tokens[3] == "1" && parsed.pitch_bend_tokens[4] == "1" &&
            parsed.pitch_bend_tokens[5] == "-1",
            "OpenUtau signed 12-bit Base64 and run-length pitch decoding failed");

    bool rejected = false;
    try {
        (void)resynth::parseArguments({
            "in.wav", "out.wav", "C4", "100", "", "0", "500", "40", "0", "100", "0", "!120", "#2#"
        });
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "OpenUtau pitch repeat markers must not be accepted without a previous point");

    rejected = false;
    try {
        (void)resynth::parseArguments({
            "in.wav", "out.wav", "C4", "100", "", "0", "500", "40", "0", "100", "0", "!", "AA"
        });
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "A malformed !tempo token must be rejected as an OpenUtau invocation");
}

void testOpenUtauClassicSignedCutoffRendering() {
    constexpr std::uint32_t sample_rate = 8000U;
    const auto input_path = std::filesystem::temp_directory_path() / "resynthutau_openutau_cutoff_input.wav";
    const auto output_path = std::filesystem::temp_directory_path() / "resynthutau_openutau_cutoff_output.wav";
    const auto source = makeTone(sample_rate, 1U, sample_rate);
    resynth::writeWav16(input_path, source);
    const auto parameters = resynth::parseArguments({
        input_path.string(), output_path.string(), "A4", "100", "", "100", "500", "40", "-400", "100", "0", "!120", "AA"
    });
    const auto report = resynth::renderUtauInvocation(parameters);
    const auto rendered = resynth::readWav(output_path);
    require(report.output_frames == 4000U && rendered.audio.frames() == 4000U,
            "OpenUtau negative cutoff should crop to offset + abs(cutoff) before stretching to requested duration");
    std::filesystem::remove(input_path);
    std::filesystem::remove(output_path);
}

void testWsolaCompressionUsesWholeSource() {
    resynth::AudioBuffer input;
    input.sample_rate = 8000;
    input.channels = 1;
    input.samples.assign(2000, 0.0F);
    for (std::size_t i = 1000; i < input.frames(); ++i) input.samples[i] = 0.5F;

    resynth::ResamplerParameters p;
    p.target_note = "C4";
    p.target_midi = 60;
    p.velocity = 100.0;
    p.length_required_ms = 125.0; // 1000 frames; shorter than the nominal WSOLA window.
    const auto rendered = resynth::renderPhase1(input, p);
    require(rendered.audio.frames() == 1000, "Compressed output length mismatch");
    float peak = 0.0F;
    for (const float sample : rendered.audio.samples) peak = std::max(peak, std::abs(sample));
    require(peak > 0.4F, "Duration compression must include the later part of the source, not only crop its start");
}


resynth::AudioBuffer makePureSine(const std::uint32_t sample_rate,
                                  const std::size_t frames,
                                  const double frequency_hz,
                                  const double amplitude = 0.4) {
    resynth::AudioBuffer audio;
    audio.sample_rate = sample_rate;
    audio.channels = 1;
    audio.samples.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double time = static_cast<double>(i) / static_cast<double>(sample_rate);
        audio.samples[i] = static_cast<float>(amplitude * std::sin(
            2.0 * std::numbers::pi * frequency_hz * time));
    }
    return audio;
}

double fittedSineResidualRms(const std::vector<float>& samples,
                             const std::uint32_t sample_rate,
                             const double frequency_hz,
                             const std::size_t begin,
                             const std::size_t end) {
    if (end <= begin || end > samples.size()) {
        throw std::invalid_argument("Invalid fitted-sine measurement range");
    }
    double sin_sin = 0.0;
    double cos_cos = 0.0;
    double sin_cos = 0.0;
    double value_sin = 0.0;
    double value_cos = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        const double angle = 2.0 * std::numbers::pi * frequency_hz *
            static_cast<double>(i) / static_cast<double>(sample_rate);
        const double sine = std::sin(angle);
        const double cosine = std::cos(angle);
        const double value = static_cast<double>(samples[i]);
        sin_sin += sine * sine;
        cos_cos += cosine * cosine;
        sin_cos += sine * cosine;
        value_sin += value * sine;
        value_cos += value * cosine;
    }
    const double determinant = sin_sin * cos_cos - sin_cos * sin_cos;
    if (!(determinant > 1.0e-20)) {
        throw std::runtime_error("Fitted-sine measurement is singular");
    }
    const double coefficient_sin = (value_sin * cos_cos - value_cos * sin_cos) / determinant;
    const double coefficient_cos = (value_cos * sin_sin - value_sin * sin_cos) / determinant;
    double residual_power = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        const double angle = 2.0 * std::numbers::pi * frequency_hz *
            static_cast<double>(i) / static_cast<double>(sample_rate);
        const double fitted = coefficient_sin * std::sin(angle) +
                              coefficient_cos * std::cos(angle);
        const double error = static_cast<double>(samples[i]) - fitted;
        residual_power += error * error;
    }
    return std::sqrt(residual_power / static_cast<double>(end - begin));
}

resynth::AudioBuffer makeHarmonicTone(const std::uint32_t sample_rate,
                                      const std::size_t frames,
                                      const double f0,
                                      const double noise_amplitude = 0.0) {
    resynth::AudioBuffer audio;
    audio.sample_rate = sample_rate;
    audio.channels = 1;
    audio.samples.resize(frames);
    std::uint32_t state = 0x12345678U;
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sample_rate);
        const double tone = 0.35 * std::sin(2.0 * std::numbers::pi * f0 * t) +
                            0.16 * std::sin(2.0 * std::numbers::pi * f0 * 2.0 * t + 0.2) +
                            0.08 * std::sin(2.0 * std::numbers::pi * f0 * 3.0 * t + 0.4);
        state = state * 1664525U + 1013904223U;
        const double random = (static_cast<double>(state) / 4294967295.0) * 2.0 - 1.0;
        audio.samples[i] = static_cast<float>(tone + random * noise_amplitude);
    }
    return audio;
}

resynth::AudioBuffer makeWhiteNoise(const std::uint32_t sample_rate,
                                    const std::size_t frames) {
    resynth::AudioBuffer audio;
    audio.sample_rate = sample_rate;
    audio.channels = 1;
    audio.samples.resize(frames);
    std::uint32_t state = 0xA5A5A5A5U;
    for (float& sample : audio.samples) {
        state = state * 1103515245U + 12345U;
        const double value = static_cast<double>((state >> 8U) & 0x00FFFFFFU) / 16777215.0;
        sample = static_cast<float>((value * 2.0 - 1.0) * 0.2);
    }
    return audio;
}

void testFftRoundTrip() {
    std::vector<std::complex<double>> values(1024U);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = {std::sin(static_cast<double>(i) * 0.13),
                     std::cos(static_cast<double>(i) * 0.07)};
    }
    const auto original = values;
    resynth::dsp::fft(values);
    resynth::dsp::fft(values, true);
    for (std::size_t i = 0; i < values.size(); ++i) {
        require(std::abs(values[i] - original[i]) < 1.0e-10,
                "FFT forward/inverse round-trip error exceeded tolerance");
    }
}

void testPitchAndHarmonicAnalysis() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto audio = makeHarmonicTone(sample_rate, sample_rate, 220.0);
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto analysis = resynth::dsp::analyzeAudio(audio, config);
    require(analysis.summary.total_frames > 50U, "Analysis should produce a frame sequence");
    require(analysis.summary.voiced_frames > analysis.summary.total_frames / 2U,
            "A stable harmonic tone should mostly be classified as voiced");
    require(std::abs(analysis.summary.average_f0_hz - 220.0) < 2.0,
            "YIN F0 estimate should be within 2 Hz for a 220 Hz test tone");

    const auto found = std::find_if(analysis.frames.begin(), analysis.frames.end(),
        [](const resynth::dsp::AnalysisFrame& frame) {
            return frame.voiced && frame.center_sample > 5000U && frame.center_sample < 11000U;
        });
    require(found != analysis.frames.end(), "A stable voiced analysis frame should exist");
    require(found->harmonics.size() >= 3U,
            "Harmonic analysis should recover at least the first three partials");
    require(std::abs(found->harmonics.front().frequency_hz - 220.0) < 2.0,
            "First harmonic frequency should follow the estimated F0");
    require(found->spectral_envelope_bands.size() == config.profile_bands,
            "Spectral envelope should be stored in the configured compact log-frequency profile");
    require(found->aperiodic_magnitude_bands.size() == config.profile_bands,
            "Aperiodic spectrum should be stored in the configured compact profile");
    for (const float value : found->spectral_envelope_bands) {
        require(std::isfinite(value) && value >= 0.0F,
                "Spectral envelope must contain only finite non-negative values");
    }
    require(found->window_size == 1024U,
            "The analysis window should adapt to the 220 Hz fundamental");
    require(found->aperiodicity_ratio < 0.10,
            "A clean harmonic signal should not be reported as strongly aperiodic");
}

void testUnvoicedAndAperiodicAnalysis() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto noise = makeWhiteNoise(sample_rate, sample_rate / 2U);
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto noise_analysis = resynth::dsp::analyzeAudio(noise, config);
    require(noise_analysis.summary.voiced_frames < noise_analysis.summary.total_frames / 5U,
            "White noise should not be classified as voiced in most frames");
    require(noise_analysis.summary.average_aperiodicity > 0.95,
            "Unvoiced noise should be assigned a high aperiodicity ratio");

    const auto clean_analysis = resynth::dsp::analyzeAudio(
        makeHarmonicTone(sample_rate, sample_rate, 220.0), config);
    const auto mixed = makeHarmonicTone(sample_rate, sample_rate, 220.0, 0.12);
    const auto mixed_analysis = resynth::dsp::analyzeAudio(mixed, config);
    require(mixed_analysis.summary.average_aperiodicity >
                clean_analysis.summary.average_aperiodicity + 0.02,
            "Adding broadband noise should measurably increase aperiodicity");
    const auto voiced_frame = std::find_if(mixed_analysis.frames.begin(), mixed_analysis.frames.end(),
        [](const resynth::dsp::AnalysisFrame& frame) {
            return frame.voiced && frame.center_sample > 5000U && frame.center_sample < 11000U;
        });
    require(voiced_frame != mixed_analysis.frames.end(), "Mixed periodic/aperiodic signal should retain voicing");
    require(voiced_frame->aperiodicity_ratio > 0.0 && voiced_frame->aperiodicity_ratio < 1.0,
            "Mixed signal should expose independent harmonic and aperiodic energy estimates");
    require(voiced_frame->harmonic_energy > 0.0 && voiced_frame->aperiodic_energy > 0.0,
            "Both estimated components should contain energy for a mixed signal");
}


void testPitchRangeAndAnalysisValidation() {
    constexpr std::uint32_t sample_rate = 16000U;
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 65.0;
    config.max_f0_hz = 1000.0;

    const auto low = resynth::dsp::analyzeAudio(
        makeHarmonicTone(sample_rate, sample_rate / 2U, 90.0), config);
    require(std::abs(low.summary.average_f0_hz - 90.0) < 2.0,
            "YIN should track a low 90 Hz test tone within 2 Hz");
    const auto low_frame = std::find_if(low.frames.begin(), low.frames.end(),
        [](const resynth::dsp::AnalysisFrame& frame) { return frame.voiced; });
    require(low_frame != low.frames.end() && low_frame->window_size == 2048U,
            "Low-F0 analysis should select a longer spectral window");

    const auto high = resynth::dsp::analyzeAudio(
        makeHarmonicTone(sample_rate, sample_rate / 2U, 880.0), config);
    require(std::abs(high.summary.average_f0_hz - 880.0) < 3.0,
            "YIN should track a high 880 Hz test tone within 3 Hz");
    const auto high_frame = std::find_if(high.frames.begin(), high.frames.end(),
        [](const resynth::dsp::AnalysisFrame& frame) { return frame.voiced; });
    require(high_frame != high.frames.end() && high_frame->window_size == 1024U,
            "High-F0 analysis should select a shorter spectral window");

    resynth::AudioBuffer silence;
    silence.sample_rate = sample_rate;
    silence.channels = 1;
    silence.samples.assign(sample_rate / 4U, 0.0F);
    const auto silent_analysis = resynth::dsp::analyzeAudio(silence, config);
    require(silent_analysis.summary.voiced_frames == 0U,
            "Silence must not be classified as voiced");
    require(silent_analysis.summary.average_aperiodicity == 0.0,
            "Silence should not be mislabeled as noise when it has no measurable energy");

    silence.samples[10] = std::numeric_limits<float>::quiet_NaN();
    bool rejected = false;
    try {
        (void)resynth::dsp::analyzeAudio(silence, config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Analysis must reject non-finite input samples");
}

void testAnalysisJsonReport() {
    const auto audio = makeHarmonicTone(16000U, 8000U, 180.0);
    const auto analysis = resynth::dsp::analyzeAudio(audio);
    const auto json = resynth::dsp::analysisToJson(analysis, true);
    require(json.find("\"summary\"") != std::string::npos,
            "Analysis JSON should include a summary object");
    require(json.find("\"profile_frequency_hz\"") != std::string::npos,
            "Analysis JSON should describe the log-frequency profile axis");
    require(json.find("\"yin_cmnd\"") != std::string::npos,
            "Analysis JSON should include the YIN confidence metric");
    require(json.find("\"spectral_envelope_bands\"") != std::string::npos,
            "Detailed analysis JSON should include spectral-envelope profiles");
    require(json.find("\"aperiodic_magnitude_bands\"") != std::string::npos,
            "Detailed analysis JSON should include aperiodic profiles");
}

void testPhase1TimingAndVolume() {
    const auto input = makeTone(44100, 2, 11025); // 250 ms
    resynth::ResamplerParameters p;
    p.input_path = "unused";
    p.output_path = "unused";
    p.target_note = "C4";
    p.target_midi = 60;
    p.velocity = 100.0;
    p.offset_ms = 10.0;
    p.length_required_ms = 120.0;
    p.fixed_length_ms = 20.0;
    p.end_blank_ms = 10.0;
    p.volume_percent = 50.0;
    const auto rendered = resynth::renderPhase1(input, p);
    require(rendered.audio.frames() == 5292, "Required duration should be sample-accurate");
    require(rendered.audio.channels == 2, "Stereo layout should be preserved");
    float peak = 0.0F;
    for (const float sample : rendered.audio.samples) {
        require(std::isfinite(sample), "Renderer emitted NaN or infinity");
        peak = std::max(peak, std::abs(sample));
    }
    require(peak < 0.25F, "50 percent volume should attenuate the test signal");
    require(!rendered.warnings.empty(), "Phase 1 limitations should be disclosed");
}


void testHarmonicPhaseUsesCenterReferenceAcrossWindowSizes() {
    constexpr std::uint32_t sample_rate = 44100U;
    const auto source = makePureSine(sample_rate, static_cast<std::size_t>(sample_rate) * 2U, 220.0);
    std::vector<double> measured_phases;

    for (const std::size_t window_size : {1024U, 2048U, 4096U}) {
        resynth::dsp::AnalysisConfig config;
        config.hop_size = 256U;
        config.min_f0_hz = 70.0;
        config.max_f0_hz = 700.0;
        config.minimum_spectral_window = window_size;
        config.maximum_spectral_window = window_size;
        const auto analysis = resynth::dsp::analyzeAudio(source, config);
        const auto frame = std::find_if(analysis.frames.begin(), analysis.frames.end(),
            [](const resynth::dsp::AnalysisFrame& item) {
                return item.center_sample == 44032U;
            });
        require(frame != analysis.frames.end(),
                "The fixed-window analysis should contain the requested center sample");
        require(!frame->harmonics.empty() && frame->harmonics.front().order == 1U,
                "The pure sine should expose its fundamental partial");
        measured_phases.push_back(frame->harmonics.front().phase_radians);
    }

    for (std::size_t i = 1U; i < measured_phases.size(); ++i) {
        const double phase_difference = std::remainder(
            measured_phases[i] - measured_phases.front(), 2.0 * std::numbers::pi);
        require(std::abs(phase_difference) < 0.01,
                "Harmonic phase must keep the same center-sample reference across analysis-window sizes");
    }
}

void testPhase3PureSineResynthesisHasNoOscillatorSidebands() {
    constexpr std::uint32_t sample_rate = 44100U;
    constexpr std::size_t frame_count = static_cast<std::size_t>(sample_rate) * 2U;
    constexpr double frequency_hz = 220.0;
    const auto source = makePureSine(sample_rate, frame_count, frequency_hz);

    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto analysis = resynth::dsp::analyzeAudio(source, config);

    resynth::ResynthesisOptions options;
    options.aperiodic_gain = 0.0; // Isolate the harmonic oscillator path.
    options.edge_fade_ms = 0.0;
    require(options.phase_observation_strength == 1.0,
            "The default renderer should fully track reliable measured phase observations");
    const auto rendered = resynth::resynthesize(analysis, options);
    require(rendered.audio.frames() == source.frames(),
            "Pure-sine reconstruction must preserve duration");

    const std::size_t begin = static_cast<std::size_t>(sample_rate) / 4U;
    const std::size_t end = frame_count - begin;
    const double residual = fittedSineResidualRms(
        rendered.audio.samples, sample_rate, frequency_hz, begin, end);
    require(residual < 0.0005,
            "A clean sine must not acquire audible oscillator-like sidebands during harmonic resynthesis");
}

void testPhase3HarmonicResynthesisAndPitchRatio() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto source = makeHarmonicTone(sample_rate, sample_rate, 220.0);
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto analysis = resynth::dsp::analyzeAudio(source, config);

    resynth::ResynthesisOptions options;
    options.aperiodic_gain = 0.0;
    options.edge_fade_ms = 0.0;
    options.phase_observation_strength = 1.0;
    const auto rendered = resynth::resynthesize(analysis, options);
    require(rendered.audio.valid(), "Phase 3 harmonic renderer must return a valid audio buffer");
    require(rendered.audio.frames() == source.frames(),
            "Phase 3 resynthesis must preserve the analyzed duration");
    require(rendered.metrics.harmonic_rms > 0.05,
            "A voiced harmonic source must produce a non-zero harmonic component");
    require(rendered.metrics.aperiodic_rms == 0.0,
            "Setting aperiodic gain to zero must disable the noise component");

    const auto reconstructed_analysis = resynth::dsp::analyzeAudio(rendered.audio, config);
    require(std::abs(reconstructed_analysis.summary.average_f0_hz - 220.0) < 4.0,
            "Phase 3 oscillator bank should reconstruct the original fundamental frequency");
    double maximum_step = 0.0;
    for (std::size_t i = 1U; i < rendered.audio.samples.size(); ++i) {
        maximum_step = std::max(maximum_step,
            std::abs(static_cast<double>(rendered.audio.samples[i] - rendered.audio.samples[i - 1U])));
    }
    require(maximum_step < 0.8,
            "Continuous-phase harmonic synthesis should not introduce large sample discontinuities");

    options.pitch_ratio = 2.0;
    const auto octave_up = resynth::resynthesize(analysis, options);
    const auto octave_analysis = resynth::dsp::analyzeAudio(octave_up.audio, config);
    require(std::abs(octave_analysis.summary.average_f0_hz - 440.0) < 6.0,
            "A 2:1 pitch ratio must reconstruct one octave above the source");
    require(octave_up.audio.frames() == source.frames(),
            "Pitch transposition must not change output duration");
}

void testPhase3AperiodicResynthesisDeterminism() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto noise = makeWhiteNoise(sample_rate, sample_rate / 2U);
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto analysis = resynth::dsp::analyzeAudio(noise, config);
    require(analysis.summary.voiced_frames < analysis.summary.total_frames / 3U,
            "Noise test fixture should be predominantly unvoiced");

    resynth::dsp::NoiseSynthesisOptions noise_options;
    noise_options.random_seed = 0x12345678ULL;
    const auto first = resynth::dsp::synthesizeAperiodicComponent(analysis, noise_options);
    const auto second = resynth::dsp::synthesizeAperiodicComponent(analysis, noise_options);
    require(first.size() == noise.frames(), "Aperiodic synthesis must preserve frame count");
    require(first == second,
            "Identical analysis and random seed must produce repeatable aperiodic output");
    double power = 0.0;
    for (const float sample : first) {
        require(std::isfinite(sample), "Aperiodic synthesis must not emit NaN or infinity");
        power += static_cast<double>(sample) * sample;
    }
    const double rms = std::sqrt(power / static_cast<double>(first.size()));
    require(rms > 0.025,
            "Unvoiced noise analysis must produce measurable aperiodic waveform energy");

    resynth::ResynthesisOptions options;
    options.harmonic_gain = 0.0;
    options.noise_seed = noise_options.random_seed;
    options.edge_fade_ms = 0.0;
    options.output_channels = 2U;
    const auto rendered = resynth::resynthesize(analysis, options);
    require(rendered.metrics.harmonic_rms == 0.0,
            "Setting harmonic gain to zero must disable the harmonic component");
    require(rendered.metrics.aperiodic_rms > 0.025,
            "Aperiodic-only rendering must retain noise energy");
    require(rendered.audio.channels == 2U && rendered.audio.frames() == noise.frames(),
            "Resynthesis must support duplicated mono output in stereo containers");
    for (std::size_t i = 0U; i < rendered.audio.frames(); ++i) {
        require(rendered.audio.at(i, 0U) == rendered.audio.at(i, 1U),
                "Stereo output should contain matching reconstructed mono channels");
    }

    noise_options.time_stretch_ratio = 1.5;
    const auto stretched_noise = resynth::dsp::synthesizeAperiodicComponent(analysis, noise_options);
    require(stretched_noise.size() == noise.frames() * 3U / 2U,
            "Time-mapped aperiodic synthesis must produce the requested output length");
    require(std::all_of(stretched_noise.begin(), stretched_noise.end(),
                        [](const float value) { return std::isfinite(value); }),
            "Time-mapped aperiodic synthesis must remain finite");
}

void testPhase3ComponentMixAndPeakGuard() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto mixed = makeHarmonicTone(sample_rate, sample_rate, 220.0, 0.18);
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto analysis = resynth::dsp::analyzeAudio(mixed, config);

    resynth::ResynthesisOptions options;
    options.harmonic_gain = 2.0;
    options.aperiodic_gain = 2.0;
    options.peak_limit = 0.75;
    options.edge_fade_ms = 0.0;
    const auto rendered = resynth::resynthesize(analysis, options);
    require(rendered.metrics.harmonic_rms > 0.0,
            "Mixed input should reconstruct a harmonic component");
    require(rendered.metrics.aperiodic_rms > 0.0,
            "Mixed input should reconstruct an aperiodic component");
    require(rendered.metrics.peak_limited,
            "Peak guard should activate when component gains exceed the configured output ceiling");
    float peak = 0.0F;
    for (const float sample : rendered.audio.samples) {
        require(std::isfinite(sample), "Merged resynthesis must contain only finite samples");
        peak = std::max(peak, std::abs(sample));
    }
    require(peak <= 0.75001F,
            "Peak guard must keep the final output below its configured ceiling");
}


resynth::AudioBuffer makeConsonantVowelSample(const std::uint32_t sample_rate,
                                               const std::size_t fixed_noise_frames,
                                               const std::size_t tail_noise_frames,
                                               const std::size_t vowel_frames) {
    resynth::AudioBuffer audio;
    audio.sample_rate = sample_rate;
    audio.channels = 1U;
    audio.samples.resize(fixed_noise_frames + tail_noise_frames + vowel_frames, 0.0F);
    std::uint32_t state = 0xA341316CU;
    for (std::size_t i = 0U; i < fixed_noise_frames + tail_noise_frames; ++i) {
        state = state * 1664525U + 1013904223U;
        const double noise = (static_cast<double>(state) / 4294967295.0) * 2.0 - 1.0;
        const double envelope = i < fixed_noise_frames ? 0.22 : 0.14;
        audio.samples[i] = static_cast<float>(noise * envelope);
    }
    const std::size_t vowel_begin = fixed_noise_frames + tail_noise_frames;
    for (std::size_t i = 0U; i < vowel_frames; ++i) {
        const double time = static_cast<double>(i) / static_cast<double>(sample_rate);
        const double f0 = 220.0;
        const double value = 0.32 * std::sin(2.0 * std::numbers::pi * f0 * time) +
                             0.12 * std::sin(2.0 * std::numbers::pi * 2.0 * f0 * time + 0.2) +
                             0.055 * std::sin(2.0 * std::numbers::pi * 3.0 * f0 * time + 0.4);
        audio.samples[vowel_begin + i] = static_cast<float>(value);
    }
    return audio;
}

void testTimeMappedHarmonicSynthesisPreservesPitch() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto source = makeHarmonicTone(sample_rate, sample_rate, 220.0);
    resynth::dsp::AnalysisConfig config;
    config.hop_size = 256U;
    config.min_f0_hz = 70.0;
    config.max_f0_hz = 700.0;
    const auto analysis = resynth::dsp::analyzeAudio(source, config);

    resynth::ResynthesisOptions options;
    options.aperiodic_gain = 0.0;
    options.edge_fade_ms = 0.0;
    options.time_stretch_ratio = 1.5;
    const auto stretched = resynth::resynthesize(analysis, options);
    require(stretched.audio.frames() == 24000U,
            "Time-mapped harmonic synthesis must produce the requested 1.5x duration");

    const auto reconstructed = resynth::dsp::analyzeAudio(stretched.audio, config);
    require(std::abs(reconstructed.summary.average_f0_hz - 220.0) < 4.0,
            "Time-mapped vowel synthesis must preserve F0 instead of lowering pitch with duration");
}

void testPhase4TimingAndConsonantVowelRendering() {
    constexpr std::uint32_t sample_rate = 16000U;
    constexpr std::size_t fixed_noise_frames = 960U; // 60 ms fixed consonant region.
    constexpr std::size_t tail_noise_frames = 480U;  // 30 ms unvoiced onset in the tail.
    constexpr std::size_t vowel_frames = 6560U;
    const auto source = makeConsonantVowelSample(sample_rate, fixed_noise_frames,
                                                 tail_noise_frames, vowel_frames);
    resynth::ResamplerParameters parameters;
    parameters.input_path = "phase4-input.wav";
    parameters.output_path = "phase4-output.wav";
    parameters.target_note = "A#3";
    parameters.target_midi = 58;
    parameters.velocity = 100.0;
    parameters.fixed_length_ms = 60.0;
    parameters.length_required_ms = 850.0;
    parameters.volume_percent = 100.0;

    const auto rendered = resynth::renderPhase4(source, parameters);
    require(rendered.audio.valid(), "Phase 4 must produce a valid audio buffer");
    require(rendered.audio.frames() == 13600U,
            "Phase 4 must honor required note length with sample accuracy");
    require(rendered.audio.channels == source.channels,
            "Phase 4 must preserve the source channel count");
    require(rendered.voiced_regions > 0U,
            "Phase 4 phonetic analysis should identify the vowel region");
    require(rendered.unvoiced_regions > 0U || rendered.transient_regions > 0U,
            "Phase 4 phonetic analysis should identify unvoiced consonant material");
    require(rendered.estimated_source_f0_hz > 210.0 && rendered.estimated_source_f0_hz < 230.0,
            "Phase 4 should estimate the source vowel fundamental");
    require(rendered.pitch_ratio > 1.03 && rendered.pitch_ratio < 1.09,
            "A#3 target note should transpose the 220 Hz source upward by about one semitone");

    for (std::size_t i = 0U; i < fixed_noise_frames; ++i) {
        require(std::abs(rendered.audio.samples[i] - source.samples[i]) < 1.0e-7F,
                "The fixed consonant region must remain sample-identical at velocity 100");
    }

    resynth::AudioBuffer vowel_tail;
    vowel_tail.sample_rate = sample_rate;
    vowel_tail.channels = 1U;
    const std::size_t vowel_start_out = fixed_noise_frames + tail_noise_frames + 1500U;
    vowel_tail.samples.assign(rendered.audio.samples.begin() +
        static_cast<std::ptrdiff_t>(vowel_start_out), rendered.audio.samples.end());
    const auto vowel_analysis = resynth::dsp::analyzeAudio(vowel_tail);
    require(std::abs(vowel_analysis.summary.average_f0_hz - 233.08) < 5.0,
            "Phase 4 voiced rendering should follow the requested A#3 target pitch");

    auto percent_parameters = parameters;
    percent_parameters.target_pitch_is_note = false;
    percent_parameters.pitch_percent = 120.0;
    const auto percent_rendered = resynth::renderPhase4(source, percent_parameters);
    require(std::abs(percent_rendered.pitch_ratio - 1.2) < 1.0e-9,
            "Numeric pitch_percent must apply the requested frequency ratio");

    for (const float sample : rendered.audio.samples) {
        require(std::isfinite(sample), "Phase 4 output must not contain NaN or infinity");
    }
}

void testNaturalPsolaSemitoneRangeAndNeutralTransparency() {
    constexpr std::uint32_t sample_rate = 16000U;
    constexpr std::size_t fixed_noise_frames = 960U;
    constexpr std::size_t tail_noise_frames = 480U;
    constexpr std::size_t vowel_frames = 6560U;
    const auto source = makeConsonantVowelSample(sample_rate, fixed_noise_frames,
                                                 tail_noise_frames, vowel_frames);

    resynth::ResamplerParameters neutral;
    neutral.input_path = "natural-neutral.wav";
    neutral.output_path = "natural-neutral-out.wav";
    neutral.target_pitch_is_note = false;
    neutral.pitch_percent = 100.0;
    neutral.velocity = 100.0;
    neutral.fixed_length_ms = 0.0;
    neutral.volume_percent = 100.0;
    const auto identity = resynth::renderPhase5(source, neutral);
    require(identity.audio.samples == source.samples,
            "Neutral same-duration rendering must be sample-transparent and avoid unnecessary resynthesis");

    const std::array<int, 6> shifts{1, 3, 5, -1, -3, -5};
    for (const int semitones : shifts) {
        auto parameters = neutral;
        parameters.output_path = "natural-pitch-test.wav";
        parameters.fixed_length_ms = 60.0;
        parameters.pitch_percent = 100.0 * std::pow(2.0, static_cast<double>(semitones) / 12.0);
        const auto rendered = resynth::renderPhase5(source, parameters);
        require(rendered.audio.frames() == source.frames(),
                "Natural semitone shifts must preserve source duration at unity timing");
        for (std::size_t frame = 0U; frame < fixed_noise_frames; ++frame) {
            require(std::abs(rendered.audio.samples[frame] - source.samples[frame]) < 1.0e-7F,
                    "Natural semitone shifts must preserve the fixed consonant region");
        }

        resynth::AudioBuffer vowel;
        vowel.sample_rate = sample_rate;
        vowel.channels = 1U;
        const std::size_t begin = fixed_noise_frames + tail_noise_frames + 1200U;
        vowel.samples.assign(rendered.audio.samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             rendered.audio.samples.end());
        const auto measured = resynth::dsp::analyzeAudio(vowel);
        const double expected = 220.0 * std::pow(2.0, static_cast<double>(semitones) / 12.0);
        require(std::abs(measured.summary.average_f0_hz - expected) < 5.0,
                "Natural TD-PSOLA must track each requested semitone shift within 5 Hz");
        for (const float sample : rendered.audio.samples) {
            require(std::isfinite(sample), "Natural TD-PSOLA output must remain finite");
        }
    }
}

void testPhase4UnvoicedOnlyFallback() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto source = makeWhiteNoise(sample_rate, sample_rate / 4U);
    resynth::ResamplerParameters parameters;
    parameters.target_note = "C4";
    parameters.target_midi = 60;
    parameters.velocity = 100.0;
    parameters.length_required_ms = 400.0;
    const auto rendered = resynth::renderPhase4(source, parameters);
    require(rendered.audio.frames() == sample_rate * 2U / 5U,
            "Unvoiced fallback must still honor requested duration");
    require(rendered.estimated_source_f0_hz == 0.0,
            "Unvoiced-only material should not be assigned a fabricated F0");
    require(rendered.pitch_ratio == 1.0,
            "Unvoiced-only material should not receive an invented pitch ratio");
    require(!rendered.warnings.empty(),
            "Phase 4 should disclose when the waveform-preserving path is used");
}


void testPhase5ExpressionFlagParsing() {
    const auto parsed = resynth::parseArguments({
        "voice.wav", "out.wav", "A3", "100",
        "g+20t-2B30H25Y50br40sf+3vv120cr110as80ng95nh75hb120dy130vib60vr5.8sb115dc40Z99"
    });
    const auto& e = parsed.expression;
    require(std::abs(e.gender_factor - 20.0) < 1.0e-9, "Standard g flag must parse independently");
    require(std::abs(e.fine_pitch_cents + 20.0) < 1.0e-9, "Standard t flag must use 10-cent steps");
    require(std::abs(e.breathiness_percent - 40.0) < 1.0e-9, "Custom br should override the lower B value");
    require(std::abs(e.lowpass_percent - 25.0) < 1.0e-9, "H low-pass flag parse failed");
    require(std::abs(e.noise_suppression_percent - 50.0) < 1.0e-9, "Y noise suppression flag parse failed");
    require(std::abs(e.formant_shift_semitones - 3.0) < 1.0e-9, "sf formant shift parse failed");
    require(std::abs(e.voice_voltage_percent - 120.0) < 1.0e-9, "vv voice voltage parse failed");
    require(std::abs(e.consonant_rate_percent - 110.0) < 1.0e-9, "cr consonant rate parse failed");
    require(std::abs(e.attack_speed_percent - 80.0) < 1.0e-9, "as attack speed parse failed");
    require(std::abs(e.note_gain_percent - 95.0) < 1.0e-9, "ng note gain parse failed");
    require(std::abs(e.noise_level_percent - 75.0) < 1.0e-9, "nh noise level parse failed");
    require(std::abs(e.harmonic_balance_percent - 120.0) < 1.0e-9, "hb harmonic balance parse failed");
    require(std::abs(e.dynamics_percent - 130.0) < 1.0e-9, "dy dynamics parse failed");
    require(std::abs(e.vibrato_depth_percent - 60.0) < 1.0e-9, "vib depth parse failed");
    require(std::abs(e.vibrato_rate_hz - 5.8) < 1.0e-9, "vr rate parse failed");
    require(std::abs(e.spectral_brightness_percent - 115.0) < 1.0e-9, "sb brightness parse failed");
    require(std::abs(e.decay_percent - 40.0) < 1.0e-9, "dc decay parse failed");
    const auto decay_alias = resynth::parseArguments({"voice.wav", "out.wav", "A3", "100", "dec25"});
    require(std::abs(decay_alias.expression.decay_percent - 25.0) < 1.0e-9,
            "The dec alias must parse as the decay expression");
    require(!e.unknown_flags.empty() && e.unknown_flags.back() == "Z99",
            "Unknown flags should be retained for a warning instead of crashing");
}

void testPhase5PitchBendAndVibrato() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto source = makeHarmonicTone(sample_rate, sample_rate, 220.0, 0.0);
    resynth::ResamplerParameters bend_parameters;
    bend_parameters.target_note = "100";
    bend_parameters.target_pitch_is_note = false;
    bend_parameters.pitch_percent = 100.0;
    bend_parameters.velocity = 100.0;
    bend_parameters.length_required_ms = 1000.0;
    bend_parameters.pitch_bend_tokens = {"0", "300"};
    const auto bent = resynth::renderPhase5(source, bend_parameters);
    resynth::dsp::AnalysisConfig pitch_config;
    pitch_config.hop_size = 128U;
    pitch_config.pitch_window_size = 1024U;
    pitch_config.minimum_spectral_window = 512U;
    pitch_config.maximum_spectral_window = 1024U;
    const auto bent_analysis = resynth::dsp::analyzeAudio(bent.audio, pitch_config);
    std::vector<double> late_f0;
    for (const auto& frame : bent_analysis.frames) {
        if (frame.center_sample >= sample_rate / 4U && frame.voiced && frame.f0_hz > 0.0) {
            late_f0.push_back(frame.f0_hz);
        }
    }
    require(!late_f0.empty(), "Pitch-bend render must retain voiced frames");
    const double late_mean = std::accumulate(late_f0.begin(), late_f0.end(), 0.0) /
                             static_cast<double>(late_f0.size());
    require(std::abs(late_mean - 220.0 * std::pow(2.0, 300.0 / 1200.0)) < 8.0,
            "Pitch-bend cents must change the target F0 continuously");

    auto vibrato_parameters = bend_parameters;
    vibrato_parameters.pitch_bend_tokens.clear();
    vibrato_parameters.expression.vibrato_depth_percent = 100.0;
    vibrato_parameters.expression.vibrato_rate_hz = 5.5;
    const auto vibrato = resynth::renderPhase5(source, vibrato_parameters);
    const auto vibrato_analysis = resynth::dsp::analyzeAudio(vibrato.audio, pitch_config);
    std::vector<double> vibrato_f0;
    for (const auto& frame : vibrato_analysis.frames) {
        if (frame.center_sample >= sample_rate / 5U &&
            frame.center_sample < sample_rate * 4U / 5U && frame.voiced && frame.f0_hz > 0.0) {
            vibrato_f0.push_back(frame.f0_hz);
        }
    }
    require(vibrato_f0.size() > 10U, "Vibrato render must have enough voiced analysis frames");
    const auto [minimum, maximum] = std::minmax_element(vibrato_f0.begin(), vibrato_f0.end());
    require(*maximum - *minimum > 2.0,
            "Vibrato control should create a measurable continuous pitch variation");
}

void testPhase5GainTimingAndExpressionDSP() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto source = makeConsonantVowelSample(sample_rate, 800U, 320U, 4880U);
    resynth::ResamplerParameters neutral;
    neutral.target_note = "100";
    neutral.target_pitch_is_note = false;
    neutral.pitch_percent = 100.0;
    neutral.velocity = 100.0;
    neutral.fixed_length_ms = 50.0;
    neutral.length_required_ms = 500.0;
    const auto baseline = resynth::renderPhase5(source, neutral);

    auto half_gain_parameters = neutral;
    half_gain_parameters.expression.note_gain_percent = 50.0;
    const auto half_gain = resynth::renderPhase5(source, half_gain_parameters);
    double base_power = 0.0;
    double half_power = 0.0;
    for (std::size_t i = 0U; i < baseline.audio.samples.size(); ++i) {
        base_power += static_cast<double>(baseline.audio.samples[i]) * baseline.audio.samples[i];
        half_power += static_cast<double>(half_gain.audio.samples[i]) * half_gain.audio.samples[i];
    }
    require(std::abs(std::sqrt(half_power / base_power) - 0.5) < 0.03,
            "Note Gain should independently scale the final note amplitude");

    auto consonant_parameters = neutral;
    consonant_parameters.length_required_ms.reset();
    consonant_parameters.expression.consonant_rate_percent = 150.0;
    const auto slower_consonant = resynth::renderPhase5(source, consonant_parameters);
    require(slower_consonant.audio.frames() > source.frames(),
            "Consonant Rate should alter the fixed consonant duration");

    auto decay_parameters = neutral;
    decay_parameters.expression.decay_percent = 100.0;
    decay_parameters.expression.attack_speed_percent = 50.0;
    decay_parameters.expression.spectral_brightness_percent = 140.0;
    decay_parameters.expression.lowpass_percent = 20.0;
    decay_parameters.expression.noise_level_percent = 150.0;
    decay_parameters.expression.breathiness_percent = 35.0;
    decay_parameters.expression.voice_voltage_percent = 120.0;
    decay_parameters.expression.harmonic_balance_percent = 110.0;
    decay_parameters.expression.dynamics_percent = 125.0;
    decay_parameters.expression.formant_shift_semitones = 2.0;
    const auto expressive = resynth::renderPhase5(source, decay_parameters);
    require(expressive.audio.frames() == baseline.audio.frames(),
            "Expression controls must not change explicitly required note duration");
    require(std::abs(expressive.audio.samples.back()) < 1.0e-7F,
            "Maximum decay should fade the note to silence at its end");
    bool changed = false;
    for (std::size_t i = 0U; i < expressive.audio.samples.size(); ++i) {
        if (std::abs(expressive.audio.samples[i] - baseline.audio.samples[i]) > 1.0e-5F) {
            changed = true;
            break;
        }
    }
    require(changed, "The combined expression controls must affect the rendered audio");
    for (float sample : expressive.audio.samples) {
        require(std::isfinite(sample), "Phase 5 expression processing must not emit NaN or infinity");
    }
}

void testConventionalUtauInvocationUsesPhase5Renderer() {
    constexpr std::uint32_t sample_rate = 16000U;
    const auto source = makeConsonantVowelSample(sample_rate, 800U, 320U, 4880U);
    const auto base = std::filesystem::temp_directory_path();
    const auto input_path = base / "resynthutau_phase5_integration_input.wav";
    const auto output_path = base / "resynthutau_phase5_integration_output.wav";
    resynth::writeWav16(input_path, source);

    resynth::ResamplerParameters parameters;
    parameters.input_path = input_path.string();
    parameters.output_path = output_path.string();
    parameters.target_note = "A3";
    parameters.target_midi = 57;
    parameters.velocity = 100.0;
    parameters.fixed_length_ms = 50.0;
    parameters.length_required_ms = 500.0;
    parameters.volume_percent = 100.0;

    const auto report = resynth::renderUtauInvocation(parameters);
    const auto output = resynth::readWav(output_path);
    require(report.output_frames == sample_rate / 2U,
            "The conventional UTAU interface must report the requested 500 ms output");
    require(output.audio.frames() == sample_rate / 2U,
            "The conventional UTAU interface must write the Phase 5 duration to WAV");
    require(output.audio.sample_rate == sample_rate && output.audio.channels == 1U,
            "The conventional UTAU interface must preserve WAV format properties");

    std::filesystem::remove(input_path);
    std::filesystem::remove(output_path);
}


std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

void testUtf8PathsAndOutputDirectoryCreation() {
    const auto base = std::filesystem::temp_directory_path();
    const auto input_path = base / std::filesystem::path(u8"ResynthUTAU_音声 input.wav");
    const auto output_dir = base / std::filesystem::path(u8"ResynthUTAU_音声 output folder");
    const auto output_path = output_dir / std::filesystem::path(u8"rendered sample.wav");
    std::error_code ignored;
    std::filesystem::remove(input_path, ignored);
    std::filesystem::remove_all(output_dir, ignored);

    const auto audio = makePureSine(44100U, 8192U, 220.0, 0.25);
    resynth::writeWav16(input_path, audio);
    resynth::ResamplerParameters parameters;
    parameters.input_path = pathToUtf8(input_path);
    parameters.output_path = pathToUtf8(output_path);
    parameters.target_note = "100";
    parameters.target_pitch_is_note = false;
    parameters.pitch_percent = 100.0;
    parameters.velocity = 100.0;
    const auto report = resynth::renderUtauInvocation(parameters);
    require(std::filesystem::exists(output_path),
            "UTF-8 output path and automatic parent directory creation failed");
    const auto decoded = resynth::readWav(output_path);
    require(report.output_frames == audio.frames() && decoded.audio.frames() == audio.frames(),
            "UTF-8 path render changed the expected output duration");

    std::filesystem::remove(input_path, ignored);
    std::filesystem::remove_all(output_dir, ignored);
}

void testReusablePitchWorkspaceMatchesStateless() {
    const auto audio = makePureSine(44100U, 44100U, 220.0, 0.35);
    resynth::dsp::PitchConfig config;
    config.window_size = 4096U;
    resynth::dsp::PitchWorkspace workspace;
    for (std::size_t center = 0U; center < audio.frames(); center += 1024U) {
        const auto reusable = resynth::dsp::estimatePitch(audio, center, config, workspace);
        const auto stateless = resynth::dsp::estimatePitch(audio, center, config);
        require(reusable.voiced == stateless.voiced,
                "Reusable pitch workspace changed the voiced decision");
        require(std::abs(reusable.f0_hz - stateless.f0_hz) < 1e-9,
                "Reusable pitch workspace changed F0 estimation");
        require(std::abs(reusable.periodicity - stateless.periodicity) < 1e-12,
                "Reusable pitch workspace changed periodicity estimation");
    }
}

} // namespace

int main() {
    try {
        testAudioBuffer();
        testWavRoundTrip();
        testWavExtendedCompatibility();
        testParameterParser();
        testOpenUtauClassicArgumentAdapter();
        testOpenUtauClassicSignedCutoffRendering();
        testWsolaCompressionUsesWholeSource();
        testPhase1TimingAndVolume();
        testFftRoundTrip();
        testPitchAndHarmonicAnalysis();
        testUnvoicedAndAperiodicAnalysis();
        testAnalysisJsonReport();
        testPitchRangeAndAnalysisValidation();
        testHarmonicPhaseUsesCenterReferenceAcrossWindowSizes();
        testPhase3PureSineResynthesisHasNoOscillatorSidebands();
        testPhase3HarmonicResynthesisAndPitchRatio();
        testPhase3AperiodicResynthesisDeterminism();
        testPhase3ComponentMixAndPeakGuard();
        testTimeMappedHarmonicSynthesisPreservesPitch();
        testPhase4TimingAndConsonantVowelRendering();
        testNaturalPsolaSemitoneRangeAndNeutralTransparency();
        testPhase4UnvoicedOnlyFallback();
        testPhase5ExpressionFlagParsing();
        testPhase5PitchBendAndVibrato();
        testPhase5GainTimingAndExpressionDSP();
        testConventionalUtauInvocationUsesPhase5Renderer();
        testReusablePitchWorkspaceMatchesStateless();
        testUtf8PathsAndOutputDirectoryCreation();
        std::cout << "All ResynthUTAU Phase 1 + Phase 2 + Phase 3 + Phase 4 + Phase 5 + Phase 6 tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILURE: " << e.what() << '\n';
        return 1;
    }
}
