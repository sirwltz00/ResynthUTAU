#include "resynth/AudioBuffer.hpp"
#include "resynth/AnalysisEngine.hpp"
#include "resynth/FFT.hpp"
#include "resynth/ParameterParser.hpp"
#include "resynth/ResynthesisEngine.hpp"
#include "resynth/WavReader.hpp"
#include "resynth/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <random>
#include <filesystem>
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

} // namespace

int main() {
    try {
        testAudioBuffer();
        testWavRoundTrip();
        testParameterParser();
        testWsolaCompressionUsesWholeSource();
        testPhase1TimingAndVolume();
        testFftRoundTrip();
        testPitchAndHarmonicAnalysis();
        testUnvoicedAndAperiodicAnalysis();
        testAnalysisJsonReport();
        testPitchRangeAndAnalysisValidation();
        std::cout << "All ResynthUTAU Phase 1 + Phase 2 tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILURE: " << e.what() << '\n';
        return 1;
    }
}
