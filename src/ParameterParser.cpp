#include "resynth/ParameterParser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

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
    if (args.size() > 4 && args[4] != "-") p.flags = args[4];
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
           "Example: ResynthUTAU input.wav output.wav C4 100 - 0 500 100 0 100 0\n";
}

} // namespace resynth
