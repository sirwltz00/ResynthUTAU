# Testing — Phase 6

The project uses a dependency-free C++ test executable registered with CTest. Tests generate synthetic signals in memory or temporary WAV files; no commercial voicebanks or copyrighted recordings are required.

The checked source archive contains 26 test functions in `tests/test_main.cpp`. CMake registers three CTest entries: `resynth_tests`, `cli_version`, and `cli_capabilities`.

## Phase 1 regression coverage

- AudioBuffer validation and interleaved indexing.
- PCM16 WAV write/read round-trip and quantization tolerance.
- Positional argument parsing, note-name validation, and numeric pitch-percent parsing (including 100% and 120%).
- Required duration, offset/end-blank cropping, fixed-region handling, stereo preservation, finite output values, and volume attenuation.

## Phase 2 analysis coverage

- Radix-2 FFT forward/inverse round-trip.
- YIN F0 tracking on synthetic 220 Hz, 90 Hz, and 880 Hz signals.
- Voiced-frame classification and recovery of harmonic partials.
- Adaptive spectral-window selection.
- Cepstral spectral-envelope and aperiodicity profile dimensions and finite values.
- White-noise and mixed harmonic/noise classification.
- Silence rejection, non-finite input rejection, and detailed JSON report fields.

## Phase 3 resynthesis coverage

- Harmonic oscillator-bank output preserves duration and reconstructs a synthetic 220 Hz fundamental within tolerance.
- A clean 220 Hz sine does not acquire significant oscillator sidebands with the aperiodic path disabled.
- Measured harmonic phase stays consistent across 1024-, 2048-, and 4096-sample analysis windows.
- Pitch ratios including 2:1 produce the expected F0 change.
- Aperiodic gain zero disables the noise path; harmonic gain zero disables the oscillator path.
- Aperiodic output is deterministic for a fixed random seed.
- Component mixing and peak protection remain within the configured ceiling.

## Phase 4 and Phase 5.1 timing/natural-path coverage

- Time-mapped harmonic synthesis returns the exact stretched frame count while retaining the source F0 within tolerance.
- A synthetic consonant-plus-vowel sample detects voiced and unvoiced regions.
- The initial fixed consonant region is sample-identical at velocity 100.
- Target-note and numeric pitch-percentage changes move voiced F0 as expected.
- Neutral same-duration rendering is sample-transparent.
- TD-PSOLA tracks ±1, ±3, and ±5 semitone shifts within the synthetic fixture's F0 tolerance.
- Required length is enforced with sample accuracy.
- Output is finite and preserves the input channel count.
- Unvoiced-only input receives no fabricated F0 or pitch ratio and uses waveform-preserving fallback.

## Phase 5 expression coverage

- Parses standard-compatible project flags `g`, `t`, `B`, `H`, and `Y` plus the custom multi-character flag set.
- Unknown flags are captured for warnings rather than crashing the renderer.
- Cents pitch-bend points change F0 through continuous oscillator integration.
- Vibrato produces measurable pitch variation on a synthetic sustained tone.
- Note Gain scales output amplitude independently.
- Consonant Rate changes fixed-region duration when no explicit total duration overrides it.
- Combined breathiness, formant movement, voice voltage, harmonic/noise balance, dynamics, attack speed, spectral brightness, low-pass, and decay controls change output while preserving explicitly requested duration.
- Maximum decay reaches zero at the note boundary; all expression output remains finite.

## Phase 6 WAV and workspace coverage

- WAVE_FORMAT_EXTENSIBLE PCM24 and IEEE float32 subtype GUIDs.
- PCM32 and IEEE float64 input decoding.
- Chunk parsing with `data` before `fmt ` and odd-sized chunk padding.
- Narrow tolerance for an RIFF-size field exactly eight bytes larger than the actual file; malformed larger overruns are rejected.
- Reusable YIN pitch workspace produces the same F0, periodicity, and voiced decision as the stateless API.
- UTF-8 file paths and automatic output-directory creation.
- `--version` and `--capabilities` command-line endpoints.

## Run tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## What the tests do not prove

Synthetic tests verify deterministic algorithms, format handling, numerical properties, and integration boundaries. They do not prove naturalness on all singers, complete consonant intelligibility, universal F0 reliability, or successful runtime integration with UTAU/OpenUtau. OpenUtau's encoded pitch curve and signed cutoff conventions are not implemented by the current source. Real-world tuning requires recorded samples across vowels, fricatives, plosives, breathy phonation, registers, and transitions, plus manual listening and actual host-application testing.

For a detailed Phase 6 run record, see [PHASE6_TEST_REPORT.md](PHASE6_TEST_REPORT.md). For performance measurements, see [PERFORMANCE.md](PERFORMANCE.md).
