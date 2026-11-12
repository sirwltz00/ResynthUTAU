#pragma once

#include <optional>
#include <string>
#include <vector>

namespace resynth {

struct ExpressionControls {
    // All percentages use neutral defaults so an empty flag string preserves
    // the Phase 4 rendering behavior as closely as possible.
    double dynamics_percent{100.0};
    double breathiness_percent{0.0};
    double formant_shift_semitones{0.0};
    double gender_factor{0.0};
    double voice_voltage_percent{100.0};
    double consonant_rate_percent{100.0};
    double attack_speed_percent{100.0};
    double note_gain_percent{100.0};
    double vibrato_depth_percent{0.0};
    double vibrato_rate_hz{5.5};
    double noise_level_percent{100.0};
    double spectral_brightness_percent{100.0};
    double harmonic_balance_percent{100.0};
    double decay_percent{0.0};
    double lowpass_percent{0.0};
    double noise_suppression_percent{0.0};
    double fine_pitch_cents{0.0};
    std::vector<std::string> unknown_flags;
};

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
    // OpenUtau Classic passes cutoff as a signed oto.ini right-blank value:
    // >= 0 trims from the WAV end; < 0 sets the end relative to offset.
    bool openutau_classic{false};
    double openutau_cutoff_ms{0.0};
    bool pitch_points_on_output_timeline{false};
    double volume_percent{100.0};
    double modulation_percent{0.0};
    std::vector<std::string> pitch_bend_tokens;
    ExpressionControls expression;
};

[[nodiscard]] int parseNoteToMidi(const std::string& note);
[[nodiscard]] ResamplerParameters parseArguments(const std::vector<std::string>& args);
[[nodiscard]] std::string usageText();

} // namespace resynth
