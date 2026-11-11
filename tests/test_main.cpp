#include "resynth/AudioBuffer.hpp"
#include "resynth/ParameterParser.hpp"
#include "resynth/ResynthesisEngine.hpp"
#include "resynth/WavReader.hpp"
#include "resynth/WavWriter.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
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
        std::cout << "All ResynthUTAU Phase 1 tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILURE: " << e.what() << '\n';
        return 1;
    }
}
