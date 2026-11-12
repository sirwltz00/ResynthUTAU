# Testing — Phase 5

The project uses a dependency-free C++ test executable registered with CTest. Tests generate synthetic signals in memory or temporary WAV files; no commercial voicebanks or copyrighted recordings are required.

## Phase 1 regression coverage

- AudioBuffer validation and interleaved indexing.
- PCM16 WAV write/read round-trip and quantization tolerance.
- UTAU positional argument parsing, note-name validation, and numeric pitch-percent parsing (including 100% and 120%).
- Required duration, offset/end blank cropping, fixed-region handling, stereo preservation, finite output values, and volume attenuation.

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

## Phase 4 timing and phonetics coverage

- Time-mapped harmonic synthesis returns the exact stretched frame count while retaining the source F0 within 4 Hz.
- A synthetic consonant-plus-vowel sample detects voiced and unvoiced regions.
- The initial fixed consonant region is sample-identical at velocity 100.
- The target note changes voiced median F0 to the requested note within test tolerance; numeric pitch percentages produce the requested frequency ratio.
- Neutral same-duration rendering is sample-transparent.
- TD-PSOLA tracks ±1, ±3, and ±5 semitone shifts within a 5 Hz F0 tolerance on a synthetic consonant–vowel fixture.
- Required length is enforced with sample accuracy.
- Output is finite and preserves the input channel count.
- Unvoiced-only input receives no fabricated F0 or pitch ratio and uses waveform-preserving fallback.
- The conventional UTAU interface writes a WAV of the requested duration through the Phase 4/5 renderer.

## Phase 5 expression coverage

- Parses standard flags `g`, `t`, `B`, `H`, and `Y` plus the custom multi-character flag set.
- Unknown flags are captured for warnings rather than crashing the renderer.
- Cents pitch-bend points change F0 through continuous oscillator integration.
- Vibrato produces measurable pitch variation on a synthetic sustained tone.
- Note Gain scales output amplitude independently.
- Consonant Rate changes fixed-region duration when no explicit total duration overrides it.
- Combined breathiness, formant movement, voice voltage, harmonic/noise balance, dynamics, attack speed, spectral brightness, low-pass, and decay controls change output while preserving explicitly requested duration.
- Maximum decay reaches zero at the note boundary; all expression output remains finite.

## Run tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Numerical interpretation and limitations

These tests verify deterministic algorithms and integration boundaries using controlled synthetic signals. They do not establish perceptual quality on real singers, complete consonant intelligibility, universal F0 reliability, or successful runtime integration with UTAU/OpenUTAU. Real-world tuning requires recorded samples across vowels, fricatives, plosives, breathy phonation, registers, and transitions, plus manual listening and host-application testing.
