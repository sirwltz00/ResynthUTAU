#include "resynth/UTAUInterface.hpp"
#include "resynth/ResynthesisEngine.hpp"
#include "resynth/WavReader.hpp"
#include "resynth/WavWriter.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace resynth {
namespace {

std::filesystem::path pathFromUtf8(const std::string& value) {
    const auto* first = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path(std::u8string(first, first + value.size()));
}

} // namespace

RenderReport renderUtauInvocation(const ResamplerParameters& parameters) {
    const WavReadResult input = readWav(pathFromUtf8(parameters.input_path));
    ProcessResult processed = renderPhase4(input.audio, parameters);
    const std::filesystem::path output_path = pathFromUtf8(parameters.output_path);
    if (output_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec) throw std::runtime_error("Cannot create output directory: " + ec.message());
    }
    writeWav16(output_path, processed.audio);

    RenderReport report;
    report.input_frames = input.audio.frames();
    report.output_frames = processed.audio.frames();
    report.sample_rate = processed.audio.sample_rate;
    report.warnings = std::move(processed.warnings);
    return report;
}

} // namespace resynth
