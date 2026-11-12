#include "resynth/AnalysisEngine.hpp"

#include "resynth/FFT.hpp"
#include "resynth/NoiseAnalyzer.hpp"
#include "resynth/SpectralEnvelope.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace resynth::dsp {
namespace {

constexpr double kLogFloor = 1.0e-20;

void validateConfig(const AudioBuffer& audio, const AnalysisConfig& config) {
    if (!audio.valid() || audio.frames() == 0U) {
        throw std::invalid_argument("Analysis requires a non-empty valid audio buffer");
    }
    if (config.hop_size == 0U || !isPowerOfTwo(config.pitch_window_size) ||
        !isPowerOfTwo(config.minimum_spectral_window) ||
        !isPowerOfTwo(config.maximum_spectral_window) ||
        config.minimum_spectral_window < 64U ||
        config.maximum_spectral_window < config.minimum_spectral_window ||
        config.pitch_window_size < 64U) {
        throw std::invalid_argument("Analysis window and hop settings are invalid");
    }
    if (!(config.min_f0_hz > 0.0) || !(config.max_f0_hz > config.min_f0_hz) ||
        config.max_f0_hz >= static_cast<double>(audio.sample_rate) * 0.5 ||
        !(config.target_cycles_per_window > 0.0) || !(config.cepstral_lifter_ms > 0.0) ||
        !(config.yin_threshold > 0.0 && config.yin_threshold < 1.0) ||
        !(config.minimum_periodicity > 0.0 && config.minimum_periodicity <= 1.0) ||
        !(config.minimum_rms >= 0.0) || config.profile_bands < 8U ||
        config.maximum_harmonics == 0U) {
        throw std::invalid_argument("Analysis frequency or feature settings are invalid");
    }
    for (const float sample : audio.samples) {
        if (!std::isfinite(sample)) {
            throw std::invalid_argument("Analysis input contains a NaN or infinite sample");
        }
    }
}

double monoSample(const AudioBuffer& audio, const std::int64_t frame) {
    if (frame < 0 || static_cast<std::uint64_t>(frame) >= audio.frames()) return 0.0;
    double sum = 0.0;
    for (std::size_t channel = 0; channel < audio.channels; ++channel) {
        sum += static_cast<double>(audio.at(static_cast<std::size_t>(frame), channel));
    }
    return sum / static_cast<double>(audio.channels);
}

std::size_t selectSpectralWindow(const std::uint32_t sample_rate,
                                 const double f0_hz,
                                 const AnalysisConfig& config) {
    if (!(f0_hz > 0.0)) {
        const std::size_t preferred = std::min<std::size_t>(2048U, config.maximum_spectral_window);
        return std::max(preferred, config.minimum_spectral_window);
    }
    const double desired = static_cast<double>(sample_rate) *
                           config.target_cycles_per_window / f0_hz;
    const auto bounded = static_cast<std::size_t>(std::clamp(
        desired, static_cast<double>(config.minimum_spectral_window),
        static_cast<double>(config.maximum_spectral_window)));
    return std::clamp(nextPowerOfTwo(bounded), config.minimum_spectral_window,
                      config.maximum_spectral_window);
}

struct SpectralWindowTable {
    std::vector<double> coefficients;
    double sum{0.0};
};

void fillWindowedSpectrum(const AudioBuffer& audio,
                          const std::size_t center,
                          const std::span<const double> window,
                          std::vector<std::complex<double>>& spectrum) {
    const std::size_t window_size = window.size();
    const std::int64_t first = static_cast<std::int64_t>(center) -
                               static_cast<std::int64_t>(window_size / 2U);
    spectrum.resize(window_size);
    for (std::size_t i = 0U; i < window_size; ++i) {
        spectrum[i] = {monoSample(audio, first + static_cast<std::int64_t>(i)) * window[i], 0.0};
    }
    fft(spectrum);
}

std::vector<float> logFrequencyProfile(const std::vector<float>& values,
                                       const std::uint32_t sample_rate,
                                       const std::size_t fft_size,
                                       const std::size_t band_count) {
    std::vector<float> bands(band_count, 0.0F);
    if (values.empty() || band_count == 0U) return bands;
    const double nyquist = static_cast<double>(sample_rate) * 0.5;
    const double min_frequency = std::min(30.0, nyquist / 8.0);
    const double max_frequency = nyquist;
    const double min_log = std::log(min_frequency);
    const double max_log = std::log(max_frequency);
    const double bin_hz = static_cast<double>(sample_rate) / static_cast<double>(fft_size);
    for (std::size_t band = 0; band < band_count; ++band) {
        const double fraction = band_count == 1U ? 0.0 :
            static_cast<double>(band) / static_cast<double>(band_count - 1U);
        const double frequency = std::exp(min_log + fraction * (max_log - min_log));
        const double position = std::clamp(frequency / bin_hz, 0.0,
                                           static_cast<double>(values.size() - 1U));
        const std::size_t left = static_cast<std::size_t>(position);
        const std::size_t right = std::min(left + 1U, values.size() - 1U);
        const float amount = static_cast<float>(position - static_cast<double>(left));
        bands[band] = values[left] + (values[right] - values[left]) * amount;
    }
    return bands;
}

void calculateSpectralMetrics(const std::vector<std::complex<double>>& spectrum,
                               const std::uint32_t sample_rate,
                               const std::size_t fft_size,
                               double& centroid_hz,
                               double& flatness) {
    const std::size_t half = fft_size / 2U;
    const double bin_hz = static_cast<double>(sample_rate) / static_cast<double>(fft_size);
    double power_sum = 0.0;
    double weighted_frequency = 0.0;
    double log_power_sum = 0.0;
    std::size_t bins_used = 0U;
    for (std::size_t bin = 1U; bin <= half; ++bin) {
        const double power = std::norm(spectrum[bin]);
        power_sum += power;
        weighted_frequency += power * static_cast<double>(bin) * bin_hz;
        log_power_sum += std::log(std::max(power, kLogFloor));
        ++bins_used;
    }
    centroid_hz = power_sum > 1.0e-20 ? weighted_frequency / power_sum : 0.0;
    if (bins_used == 0U || power_sum <= 1.0e-20) {
        flatness = 0.0;
    } else {
        const double geometric_mean = std::exp(log_power_sum / static_cast<double>(bins_used));
        const double arithmetic_mean = power_sum / static_cast<double>(bins_used);
        flatness = std::clamp(geometric_mean / std::max(arithmetic_mean, kLogFloor), 0.0, 1.0);
    }
}

void jsonNumber(std::ostringstream& out, const double value) {
    if (std::isfinite(value)) out << value;
    else out << "null";
}

} // namespace

AnalysisResult analyzeAudio(const AudioBuffer& audio, const AnalysisConfig& config) {
    validateConfig(audio, config);
    if (config.max_f0_hz >= static_cast<double>(audio.sample_rate) * 0.5) {
        throw std::invalid_argument("Configured maximum F0 must be below Nyquist");
    }

    AnalysisResult result;
    result.sample_rate = audio.sample_rate;
    result.input_frames = audio.frames();
    result.config = config;

    PitchConfig pitch_config;
    pitch_config.window_size = config.pitch_window_size;
    pitch_config.min_f0_hz = config.min_f0_hz;
    pitch_config.max_f0_hz = config.max_f0_hz;
    pitch_config.yin_threshold = config.yin_threshold;
    pitch_config.minimum_periodicity = config.minimum_periodicity;
    pitch_config.minimum_rms = config.minimum_rms;

    const std::size_t expected_frames = 1U + (audio.frames() - 1U) / config.hop_size;
    result.frames.reserve(expected_frames);
    double f0_sum = 0.0;
    double aperiodicity_sum = 0.0;
    double centroid_sum = 0.0;
    double min_f0 = std::numeric_limits<double>::infinity();
    double max_f0 = 0.0;

    // The spectral window only changes among a small set of power-of-two sizes.
    // Precompute its Hann coefficients once and reuse an FFT buffer for every frame.
    std::unordered_map<std::size_t, SpectralWindowTable> spectral_windows;
    spectral_windows.reserve(8U);
    for (std::size_t size = config.minimum_spectral_window;
         size <= config.maximum_spectral_window; size <<= 1U) {
        auto& table = spectral_windows[size];
        table.coefficients.resize(size);
        const double scale = 2.0 * std::numbers::pi / static_cast<double>(size - 1U);
        for (std::size_t i = 0U; i < size; ++i) {
            table.coefficients[i] = 0.5 - 0.5 * std::cos(scale * static_cast<double>(i));
            table.sum += table.coefficients[i];
        }
        if (size > config.maximum_spectral_window / 2U) break;
    }
    std::vector<std::complex<double>> spectrum;
    spectrum.reserve(config.maximum_spectral_window);
    PitchWorkspace pitch_workspace;

    for (std::size_t center = 0U; center < audio.frames(); center += config.hop_size) {
        const PitchEstimate pitch = estimatePitch(audio, center, pitch_config, pitch_workspace);
        const std::size_t window_size = selectSpectralWindow(audio.sample_rate, pitch.f0_hz, config);
        const auto& window_table = spectral_windows.at(window_size);
        fillWindowedSpectrum(audio, center, window_table.coefficients, spectrum);
        const double window_sum = window_table.sum;

        std::vector<float> envelope = estimateSpectralEnvelope(
            spectrum, audio.sample_rate, config.cepstral_lifter_ms, window_sum);
        const auto harmonics = pitch.voiced
            ? extractHarmonics(spectrum, audio.sample_rate, pitch.f0_hz,
                               window_sum, config.maximum_harmonics)
            : std::vector<HarmonicPartial>{};
        const auto aperiodicity = estimateAperiodicity(
            spectrum, audio.sample_rate, pitch.voiced ? pitch.f0_hz : 0.0, window_sum);

        AnalysisFrame frame;
        frame.center_sample = center;
        frame.window_size = window_size;
        frame.rms = pitch.rms;
        frame.f0_hz = pitch.f0_hz;
        frame.periodicity = pitch.periodicity;
        frame.yin_cmnd = pitch.yin_cmnd;
        frame.voiced = pitch.voiced;
        frame.harmonic_energy = aperiodicity.harmonic_energy;
        frame.aperiodic_energy = aperiodicity.aperiodic_energy;
        frame.aperiodicity_ratio = aperiodicity.aperiodicity_ratio;
        frame.harmonics = harmonics;
        frame.spectral_envelope_bands = logFrequencyProfile(
            envelope, audio.sample_rate, window_size, config.profile_bands);
        frame.aperiodic_magnitude_bands = logFrequencyProfile(
            aperiodicity.aperiodic_magnitude, audio.sample_rate, window_size, config.profile_bands);
        calculateSpectralMetrics(spectrum, audio.sample_rate, window_size,
                                 frame.spectral_centroid_hz, frame.spectral_flatness);

        if (frame.voiced) {
            ++result.summary.voiced_frames;
            f0_sum += frame.f0_hz;
            min_f0 = std::min(min_f0, frame.f0_hz);
            max_f0 = std::max(max_f0, frame.f0_hz);
        } else {
            ++result.summary.unvoiced_frames;
        }
        aperiodicity_sum += frame.aperiodicity_ratio;
        centroid_sum += frame.spectral_centroid_hz;
        result.frames.push_back(std::move(frame));
    }

    result.summary.total_frames = result.frames.size();
    if (result.summary.voiced_frames > 0U) {
        result.summary.average_f0_hz = f0_sum / static_cast<double>(result.summary.voiced_frames);
        result.summary.minimum_f0_hz = min_f0;
        result.summary.maximum_f0_hz = max_f0;
    }
    if (!result.frames.empty()) {
        result.summary.average_aperiodicity = aperiodicity_sum / static_cast<double>(result.frames.size());
        result.summary.average_spectral_centroid_hz = centroid_sum / static_cast<double>(result.frames.size());
    }
    return result;
}

std::string analysisToJson(const AnalysisResult& analysis, const bool include_frames) {
    std::ostringstream out;
    out << std::setprecision(10);
    out << "{\n  \"engine\": \"ResynthUTAU analysis\",\n"
        << "  \"analysis_version\": 1,\n"
        << "  \"sample_rate\": " << analysis.sample_rate << ",\n"
        << "  \"input_frames\": " << analysis.input_frames << ",\n"
        << "  \"duration_seconds\": ";
    jsonNumber(out, analysis.sample_rate > 0U
        ? static_cast<double>(analysis.input_frames) / static_cast<double>(analysis.sample_rate) : 0.0);
    out << ",\n  \"config\": {\n"
        << "    \"hop_size\": " << analysis.config.hop_size << ",\n"
        << "    \"pitch_window_size\": " << analysis.config.pitch_window_size << ",\n"
        << "    \"minimum_spectral_window\": " << analysis.config.minimum_spectral_window << ",\n"
        << "    \"maximum_spectral_window\": " << analysis.config.maximum_spectral_window << ",\n"
        << "    \"minimum_f0_hz\": ";
    jsonNumber(out, analysis.config.min_f0_hz);
    out << ",\n    \"maximum_f0_hz\": ";
    jsonNumber(out, analysis.config.max_f0_hz);
    out << ",\n    \"yin_threshold\": ";
    jsonNumber(out, analysis.config.yin_threshold);
    out << ",\n    \"minimum_periodicity\": ";
    jsonNumber(out, analysis.config.minimum_periodicity);
    out << ",\n    \"cepstral_lifter_ms\": ";
    jsonNumber(out, analysis.config.cepstral_lifter_ms);
    out << ",\n    \"profile_bands\": " << analysis.config.profile_bands << "\n  },\n"
        << "  \"profile_frequency_hz\": [";
    const double nyquist = static_cast<double>(analysis.sample_rate) * 0.5;
    const double profile_min_hz = std::min(30.0, nyquist / 8.0);
    for (std::size_t band = 0; band < analysis.config.profile_bands; ++band) {
        if (band != 0U) out << ',';
        const double fraction = analysis.config.profile_bands == 1U ? 0.0 :
            static_cast<double>(band) / static_cast<double>(analysis.config.profile_bands - 1U);
        jsonNumber(out, std::exp(std::log(profile_min_hz) + fraction *
                                 (std::log(nyquist) - std::log(profile_min_hz))));
    }
    out << "],\n  \"summary\": {\n"
        << "    \"total_frames\": " << analysis.summary.total_frames << ",\n"
        << "    \"voiced_frames\": " << analysis.summary.voiced_frames << ",\n"
        << "    \"unvoiced_frames\": " << analysis.summary.unvoiced_frames << ",\n"
        << "    \"average_f0_hz\": ";
    jsonNumber(out, analysis.summary.average_f0_hz);
    out << ",\n    \"minimum_f0_hz\": ";
    jsonNumber(out, analysis.summary.minimum_f0_hz);
    out << ",\n    \"maximum_f0_hz\": ";
    jsonNumber(out, analysis.summary.maximum_f0_hz);
    out << ",\n    \"average_aperiodicity\": ";
    jsonNumber(out, analysis.summary.average_aperiodicity);
    out << ",\n    \"average_spectral_centroid_hz\": ";
    jsonNumber(out, analysis.summary.average_spectral_centroid_hz);
    out << "\n  }";

    if (include_frames) {
        out << ",\n  \"frames\": [\n";
        for (std::size_t i = 0; i < analysis.frames.size(); ++i) {
            const auto& frame = analysis.frames[i];
            out << "    {\"center_sample\": " << frame.center_sample
                << ", \"window_size\": " << frame.window_size
                << ", \"voiced\": " << (frame.voiced ? "true" : "false")
                << ", \"f0_hz\": ";
            jsonNumber(out, frame.f0_hz);
            out << ", \"periodicity\": ";
            jsonNumber(out, frame.periodicity);
            out << ", \"yin_cmnd\": ";
            jsonNumber(out, frame.yin_cmnd);
            out << ", \"rms\": ";
            jsonNumber(out, frame.rms);
            out << ", \"harmonic_energy\": ";
            jsonNumber(out, frame.harmonic_energy);
            out << ", \"aperiodic_energy\": ";
            jsonNumber(out, frame.aperiodic_energy);
            out << ", \"aperiodicity_ratio\": ";
            jsonNumber(out, frame.aperiodicity_ratio);
            out << ", \"spectral_centroid_hz\": ";
            jsonNumber(out, frame.spectral_centroid_hz);
            out << ", \"spectral_flatness\": ";
            jsonNumber(out, frame.spectral_flatness);

            out << ", \"harmonics\": [";
            for (std::size_t h = 0; h < frame.harmonics.size(); ++h) {
                if (h != 0U) out << ',';
                const auto& partial = frame.harmonics[h];
                out << "{\"order\":" << partial.order << ",\"frequency_hz\":";
                jsonNumber(out, partial.frequency_hz);
                out << ",\"amplitude\":";
                jsonNumber(out, partial.amplitude);
                out << ",\"phase_radians\":";
                jsonNumber(out, partial.phase_radians);
                out << '}';
            }
            out << "]";
            const auto writeBands = [&out](const char* name, const std::vector<float>& bands) {
                out << ", \"" << name << "\": [";
                for (std::size_t b = 0; b < bands.size(); ++b) {
                    if (b != 0U) out << ',';
                    jsonNumber(out, static_cast<double>(bands[b]));
                }
                out << ']';
            };
            writeBands("spectral_envelope_bands", frame.spectral_envelope_bands);
            writeBands("aperiodic_magnitude_bands", frame.aperiodic_magnitude_bands);
            out << '}' << (i + 1U == analysis.frames.size() ? "\n" : ",\n");
        }
        out << "  ]\n";
    } else {
        out << '\n';
    }
    out << "}\n";
    return out.str();
}

} // namespace resynth::dsp
