#include "resynth/ParameterParser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace resynth {
namespace {

double parseNumber(const std::string& text, const char* name) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (...) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    if (consumed != text.size() || !std::isfinite(value)) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    return value;
}

struct FlagName {
    std::string_view name;
    double ExpressionControls::* member;
};

void parseFlags(const std::string& flags, ExpressionControls& expression) {
    // Long names are tested before single-character standard UTAU flags.
    static constexpr std::array<FlagName, 14> custom{{
        {"vib", &ExpressionControls::vibrato_depth_percent},
        {"vr", &ExpressionControls::vibrato_rate_hz},
        {"br", &ExpressionControls::breathiness_percent},
        {"sf", &ExpressionControls::formant_shift_semitones},
        {"vv", &ExpressionControls::voice_voltage_percent},
        {"cr", &ExpressionControls::consonant_rate_percent},
        {"as", &ExpressionControls::attack_speed_percent},
        {"ng", &ExpressionControls::note_gain_percent},
        {"nh", &ExpressionControls::noise_level_percent},
        {"hb", &ExpressionControls::harmonic_balance_percent},
        {"dy", &ExpressionControls::dynamics_percent},
        {"dc", &ExpressionControls::decay_percent},
        {"sb", &ExpressionControls::spectral_brightness_percent},
        {"dec", &ExpressionControls::decay_percent}
    }};

    std::size_t i = 0U;
    while (i < flags.size()) {
        if (std::isspace(static_cast<unsigned char>(flags[i])) != 0 ||
            flags[i] == ',' || flags[i] == ';' || flags[i] == '_' || flags[i] == '|') {
            ++i;
            continue;
        }

        std::string_view matched;
        double ExpressionControls::* member = nullptr;
        for (const auto& item : custom) {
            if (flags.compare(i, item.name.size(), item.name) == 0) {
                matched = item.name;
                member = item.member;
                break;
            }
        }

        bool standard_g = false;
        bool standard_t = false;
        bool standard_b = false;
        bool standard_h = false;
        bool standard_y = false;
        if (member == nullptr) {
            switch (flags[i]) {
            case 'g': standard_g = true; matched = "g"; break;
            case 't': standard_t = true; matched = "t"; break;
            case 'B': standard_b = true; matched = "B"; break;
            case 'H': standard_h = true; matched = "H"; break;
            case 'Y': standard_y = true; matched = "Y"; break;
            default: {
                const std::size_t unknown_begin = i++;
                if (i < flags.size() && (flags[i] == '+' || flags[i] == '-')) ++i;
                while (i < flags.size()) {
                    const unsigned char c = static_cast<unsigned char>(flags[i]);
                    if (std::isalnum(c) == 0 && flags[i] != '.') break;
                    ++i;
                }
                expression.unknown_flags.push_back(flags.substr(unknown_begin, i - unknown_begin));
                continue;
            }
            }
        }
        i += matched.size();
        while (i < flags.size() && (flags[i] == '=' || flags[i] == ':')) ++i;
        const std::size_t value_begin = i;
        if (i < flags.size() && (flags[i] == '+' || flags[i] == '-')) ++i;
        bool has_digit = false;
        bool has_dot = false;
        while (i < flags.size()) {
            const unsigned char c = static_cast<unsigned char>(flags[i]);
            if (std::isdigit(c) != 0) { has_digit = true; ++i; continue; }
            if (flags[i] == '.' && !has_dot) { has_dot = true; ++i; continue; }
            break;
        }
        if (!has_digit) {
            expression.unknown_flags.emplace_back(matched);
            continue;
        }
        const std::string number = flags.substr(value_begin, i - value_begin);
        const double value = parseNumber(number, "expression flag");

        if (member != nullptr) {
            if (matched == "br") expression.breathiness_percent =
                std::max(expression.breathiness_percent, value);
            else expression.*member = value;
        } else if (standard_g) {
            if (value < -100.0 || value > 100.0) throw std::invalid_argument("g flag must be between -100 and 100");
            // Conventional gender factor: positive values lower the perceived
            // vocal tract resonances; this is an inspired clean-room mapping.
            expression.gender_factor = value;
        } else if (standard_t) {
            if (value < -100.0 || value > 100.0) throw std::invalid_argument("t flag must be between -100 and 100");
            // The implementation uses 10-cent steps, as documented by the
            // UTAU flag reference used for this compatibility profile.
            expression.fine_pitch_cents += value * 10.0;
        } else if (standard_b) {
            if (value < 0.0 || value > 100.0) throw std::invalid_argument("B flag must be between 0 and 100");
            expression.breathiness_percent = std::max(expression.breathiness_percent, value);
        } else if (standard_h) {
            if (value < 0.0 || value > 99.0) throw std::invalid_argument("H flag must be between 0 and 99");
            expression.lowpass_percent = value;
        } else if (standard_y) {
            if (value < 0.0 || value > 100.0) throw std::invalid_argument("Y flag must be between 0 and 100");
            expression.noise_suppression_percent = value;
        }
    }

    const auto range = [](const double value, const double lo, const double hi,
                          const char* name) {
        if (!std::isfinite(value) || value < lo || value > hi) {
            throw std::invalid_argument(std::string("Expression ") + name +
                                        " must be between " + std::to_string(lo) +
                                        " and " + std::to_string(hi));
        }
    };
    range(expression.dynamics_percent, 0.0, 200.0, "dy");
    range(expression.breathiness_percent, 0.0, 100.0, "br/B");
    range(expression.formant_shift_semitones, -24.0, 24.0, "sf");
    range(expression.gender_factor, -100.0, 100.0, "g");
    range(expression.voice_voltage_percent, 0.0, 200.0, "vv");
    range(expression.consonant_rate_percent, 50.0, 200.0, "cr");
    range(expression.attack_speed_percent, 25.0, 200.0, "as");
    range(expression.note_gain_percent, 0.0, 400.0, "ng");
    range(expression.vibrato_depth_percent, 0.0, 100.0, "vib");
    range(expression.vibrato_rate_hz, 2.0, 10.0, "vr");
    range(expression.noise_level_percent, 0.0, 200.0, "nh");
    range(expression.spectral_brightness_percent, 0.0, 200.0, "sb");
    range(expression.harmonic_balance_percent, 0.0, 200.0, "hb");
    range(expression.decay_percent, 0.0, 100.0, "dc");
    range(expression.lowpass_percent, 0.0, 99.0, "H");
    range(expression.noise_suppression_percent, 0.0, 100.0, "Y");
    range(expression.fine_pitch_cents, -1200.0, 1200.0, "t");
}

} // namespace

int parseNoteToMidi(const std::string& note) {
    if (note.size() < 2 || note.size() > 4) throw std::invalid_argument("Pitch must be a note such as C4 or F#3");
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(note[0])));
    int semitone = -1;
    switch (letter) {
    case 'C': semitone = 0; break;
    case 'D': semitone = 2; break;
    case 'E': semitone = 4; break;
    case 'F': semitone = 5; break;
    case 'G': semitone = 7; break;
    case 'A': semitone = 9; break;
    case 'B': semitone = 11; break;
    default: throw std::invalid_argument("Invalid pitch note: " + note);
    }
    std::size_t index = 1;
    if (index < note.size() && (note[index] == '#' || note[index] == 'b')) {
        semitone += note[index] == '#' ? 1 : -1;
        ++index;
    }
    if (index >= note.size()) throw std::invalid_argument("Pitch note is missing its octave: " + note);
    int octave = 0;
    try {
        std::size_t consumed = 0;
        octave = std::stoi(note.substr(index), &consumed);
        if (consumed != note.size() - index) throw std::invalid_argument("bad octave");
    } catch (...) {
        throw std::invalid_argument("Invalid pitch octave: " + note);
    }
    const int midi = (octave + 1) * 12 + semitone;
    if (midi < 0 || midi > 127) throw std::invalid_argument("Pitch note is outside MIDI range: " + note);
    return midi;
}

ResamplerParameters parseArguments(const std::vector<std::string>& args) {
    if (args.size() < 4) throw std::invalid_argument(usageText());
    ResamplerParameters p;
    p.input_path = args[0];
    p.output_path = args[1];
    p.target_note = args[2];
    try {
        p.target_midi = parseNoteToMidi(p.target_note);
        p.target_pitch_is_note = true;
    } catch (const std::invalid_argument&) {
        p.pitch_percent = parseNumber(p.target_note, "pitch percent");
        if (p.pitch_percent < 1.0 || p.pitch_percent > 400.0) {
            throw std::invalid_argument("Pitch percent must be between 1 and 400, or a note such as C4");
        }
        p.target_pitch_is_note = false;
    }
    p.velocity = parseNumber(args[3], "velocity");
    if (p.velocity < 0.0 || p.velocity > 200.0) throw std::invalid_argument("Velocity must be between 0 and 200");

    // UTAU's optional positional values are trailing-only. An empty flags slot is
    // represented by "-" by this implementation when later timing values follow.
    if (args.size() > 4 && args[4] != "-") {
        p.flags = args[4];
        parseFlags(p.flags, p.expression);
    }
    if (args.size() > 5) p.offset_ms = parseNumber(args[5], "offset");
    if (args.size() > 6) p.length_required_ms = parseNumber(args[6], "length_required");
    if (args.size() > 7) p.fixed_length_ms = parseNumber(args[7], "fixed_length");
    if (args.size() > 8) p.end_blank_ms = parseNumber(args[8], "end_blank");
    if (args.size() > 9) p.volume_percent = parseNumber(args[9], "volume");
    if (args.size() > 10) p.modulation_percent = parseNumber(args[10], "modulation");
    if (args.size() > 11) {
        p.pitch_bend_tokens.assign(args.begin() + 11, args.end());
    }

    if (p.offset_ms < 0.0 || p.fixed_length_ms < 0.0 || p.end_blank_ms < 0.0) {
        throw std::invalid_argument("Offset, fixed length, and end blank must be non-negative");
    }
    if (p.length_required_ms && *p.length_required_ms <= 0.0) {
        throw std::invalid_argument("Required length must be greater than zero");
    }
    if (p.volume_percent < 0.0 || p.volume_percent > 1000.0) {
        throw std::invalid_argument("Volume must be between 0 and 1000 percent");
    }
    if (p.modulation_percent < 0.0 || p.modulation_percent > 100.0) {
        throw std::invalid_argument("Modulation must be between 0 and 100 percent");
    }
    return p;
}

std::string usageText() {
    return "Usage: ResynthUTAU <input.wav> <output.wav> <pitch_percent_or_note> <velocity> "
           "[flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms "
           "[volume_percent [modulation_percent [pitch_bend...]]]]]]]\n"
           "Pitch may be a numeric percentage (100 = unchanged) or a note such as C4.\n"
           "Expression flags: g,t,B,H,Y and br,sf,vv,cr,as,ng,nh,hb,dy,vib,vr,sb,dc.\n"
           "Pitch-bend points are cents; optional leading Q<tempo> sets a 96-points-per-quarter grid.\n"
           "Example: ResynthUTAU input.wav output.wav C4 100 - 0 500 100 0 100 0\n";
}

} // namespace resynth
