#pragma once

#include <optional>
#include <string>
#include <vector>

namespace resynth {

struct ResamplerParameters {
    std::string input_path;
    std::string output_path;
    std::string target_note;
    int target_midi{60};
    bool target_pitch_is_note{true};
    double pitch_percent{100.0};
    double velocity{100.0};
    std::string flags;
    double offset_ms{0.0};
    std::optional<double> length_required_ms;
    double fixed_length_ms{0.0};
    double end_blank_ms{0.0};
    double volume_percent{100.0};
    double modulation_percent{0.0};
    std::vector<std::string> pitch_bend_tokens;
};

[[nodiscard]] int parseNoteToMidi(const std::string& note);
[[nodiscard]] ResamplerParameters parseArguments(const std::vector<std::string>& args);
[[nodiscard]] std::string usageText();

} // namespace resynth
