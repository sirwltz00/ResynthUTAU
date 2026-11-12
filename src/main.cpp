#include "resynth/AnalysisEngine.hpp"
#include "resynth/ParameterParser.hpp"
#include "resynth/ResynthesisEngine.hpp"
#include "resynth/UTAUInterface.hpp"
#include "resynth/WavReader.hpp"
#include "resynth/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

static std::string utf8FromWide(const std::wstring& value) {
    if (value.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) throw std::runtime_error("Could not convert Windows command-line text to UTF-8");
    std::string result(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed, nullptr, nullptr);
    return result;
}
#endif

namespace {

std::filesystem::path pathFromUtf8(const std::string& value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const unsigned char byte : value) encoded.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(encoded);
}

void printAnalysisSummary(const resynth::dsp::AnalysisResult& analysis) {
    std::cout << "ResynthUTAU analysis complete\n"
              << "Sample rate: " << analysis.sample_rate << " Hz\n"
              << "Duration: " << static_cast<double>(analysis.input_frames) /
                                      static_cast<double>(analysis.sample_rate) << " s\n"
              << "Frames: " << analysis.summary.total_frames << "\n"
              << "Voiced / unvoiced: " << analysis.summary.voiced_frames << " / "
                                      << analysis.summary.unvoiced_frames << "\n";
    if (analysis.summary.voiced_frames > 0U) {
        std::cout << "F0 mean / min / max: " << analysis.summary.average_f0_hz << " / "
                  << analysis.summary.minimum_f0_hz << " / "
                  << analysis.summary.maximum_f0_hz << " Hz\n";
    } else {
        std::cout << "F0: no reliably voiced frames detected\n";
    }
    std::cout << "Mean aperiodicity: " << analysis.summary.average_aperiodicity << "\n"
              << "Mean spectral centroid: " << analysis.summary.average_spectral_centroid_hz << " Hz\n";
}

int run(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "ResynthUTAU Phase 5 — expressive vocal resynthesis\n"
                  << "UTAU usage:\n" << resynth::usageText()
                  << "Analysis usage:\n  ResynthUTAU --analyze <input.wav> [report.json]\n"
                  << "Resynthesis usage:\n  ResynthUTAU --resynthesize <input.wav> <output.wav> [pitch_ratio]\n";
        return 0;
    }

    if (args[0] == "--analyze") {
        if (args.size() < 2U || args.size() > 3U) {
            throw std::runtime_error("Usage: ResynthUTAU --analyze <input.wav> [report.json]");
        }
        const auto input_path = pathFromUtf8(args[1]);
        const auto decoded = resynth::readWav(input_path);
        const auto analysis = resynth::dsp::analyzeAudio(decoded.audio);
        printAnalysisSummary(analysis);
        if (args.size() == 3U) {
            const auto report_path = pathFromUtf8(args[2]);
            std::ofstream report(report_path, std::ios::binary | std::ios::trunc);
            if (!report) throw std::runtime_error("Could not create analysis report: " + args[2]);
            report << resynth::dsp::analysisToJson(analysis, true);
            if (!report) throw std::runtime_error("Failed while writing analysis report: " + args[2]);
            std::cout << "Detailed report: " << args[2] << '\n';
        }
        return 0;
    }

    if (args[0] == "--resynthesize") {
        if (args.size() < 3U || args.size() > 4U) {
            throw std::runtime_error("Usage: ResynthUTAU --resynthesize <input.wav> <output.wav> [pitch_ratio]");
        }
        const auto input_path = pathFromUtf8(args[1]);
        const auto output_path = pathFromUtf8(args[2]);
        const auto decoded = resynth::readWav(input_path);
        const auto analysis = resynth::dsp::analyzeAudio(decoded.audio);
        resynth::ResynthesisOptions options;
        if (args.size() == 4U) {
            std::size_t consumed = 0U;
            options.pitch_ratio = std::stod(args[3], &consumed);
            if (consumed != args[3].size() || !(options.pitch_ratio > 0.0) ||
                !std::isfinite(options.pitch_ratio)) {
                throw std::runtime_error("pitch_ratio must be a finite positive number, e.g. 1.0 or 1.059463");
            }
        }
        const auto rendered = resynth::resynthesize(analysis, options);
        resynth::writeWav16(output_path, rendered.audio);
        std::cout << std::fixed << std::setprecision(5)
                  << "ResynthUTAU Analysis-to-resynthesis render complete\n"
                  << "Pitch ratio: " << options.pitch_ratio << "\n"
                  << "Output: " << args[2] << "\n"
                  << "Frames: " << rendered.audio.frames() << "\n"
                  << "Harmonic RMS: " << rendered.metrics.harmonic_rms << "\n"
                  << "Aperiodic RMS: " << rendered.metrics.aperiodic_rms << "\n"
                  << "Mixed RMS: " << rendered.metrics.mixed_rms << "\n";
        for (const auto& warning : rendered.warnings) std::cerr << "[ResynthUTAU note] " << warning << '\n';
        return 0;
    }

    const auto params = resynth::parseArguments(args);
    const auto report = resynth::renderUtauInvocation(params);
    std::cout << "Rendered " << report.output_frames << " frames at " << report.sample_rate << " Hz\n";
    for (const auto& warning : report.warnings) std::cerr << "[ResynthUTAU warning] " << warning << '\n';
    return 0;
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
        for (int i = 1; i < argc; ++i) args.push_back(utf8FromWide(argv[i]));
        return run(args);
    } catch (const std::exception& e) {
        std::cerr << "ResynthUTAU: " << e.what() << '\n';
        return 2;
    }
}
#else
int main(int argc, char** argv) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        return run(args);
    } catch (const std::exception& e) {
        std::cerr << "ResynthUTAU: " << e.what() << '\n';
        return 2;
    }
}
#endif
