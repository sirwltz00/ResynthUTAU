# Testing — Phase 3

The project uses a small dependency-free C++ test executable registered with CTest. Tests generate synthetic signals in memory; no commercial voicebanks or copyrighted recordings are required.

## Phase 1 regression coverage

- AudioBuffer validation and interleaved indexing.
- PCM16 WAV write/read round-trip and quantization tolerance.
- UTAU positional argument parsing and note-name validation.
- Required duration, offset, end blank, fixed region, stereo preservation, finite output values, and volume attenuation.

## Phase 2 analysis coverage

- Radix-2 FFT forward/inverse round-trip; complex sample error must remain below `1e-10` for the deterministic test vector.
- YIN F0 tracking on a synthetic 220 Hz harmonic tone; average F0 must remain within 2 Hz in the configured test range.
- Voiced-frame classification and recovery of at least three harmonic partials.
- Adaptive spectral-window selection for a 220 Hz voiced signal.
- 256-band cepstral spectral-envelope and aperiodicity profiles; values must be finite and non-negative.
- White-noise classification; fewer than 20% of frames may be voiced and mean aperiodicity must exceed 0.95.
- Mixed harmonic/noise signal; both estimated component energies must be non-zero and the aperiodicity ratio must be strictly between zero and one.
- Low/high F0 range checks, adaptive window length checks, silence rejection as voiced content, and non-finite input rejection.
- Detailed JSON report field generation.

## Phase 3 resynthesis coverage

- Harmonic oscillator-bank output preserves the analyzed duration and reconstructs a synthetic 220 Hz fundamental within 4 Hz.
- A clean 220 Hz single-sine fixture is reconstructed without significant oscillator sidebands when the aperiodic path is disabled; the fitted residual RMS must remain below 0.0005.
- A measured harmonic phase remains consistent to within 0.01 radians when the analysis window changes among 1024, 2048, and 4096 samples.
- A 2:1 target pitch ratio reconstructs approximately 440 Hz without changing duration.
- Aperiodic gain zero disables the noise path; harmonic gain zero disables the oscillator path.
- Unvoiced white-noise input produces measurable aperiodic waveform energy.
- Fixed random seed reproduces identical aperiodic samples in the same build.
- Stereo-container output duplicates the reconstructed mono signal exactly.
- Mixed harmonic/noise material produces both components independently.
- The global peak guard activates when needed and keeps output under the configured ceiling.
- Output samples are finite and the harmonic fixture has no large sample-to-sample discontinuities.

## Run tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Numerical interpretation

The Phase 3.1 sine regression isolates the harmonic path (`aperiodic_gain = 0`) and fits the rendered samples to a sine at the known input frequency, allowing amplitude and phase to vary. This specifically detects periodic modulation/sidebands that a simple F0 estimate or maximum-adjacent-sample-step check would miss. The phase-reference regression uses the same source samples and center position under three fixed spectral-window sizes.


The harmonic and white-noise tests are controlled synthetic fixtures, not a claim of universal pitch-tracking or voice/unvoiced accuracy. They validate algorithmic behavior and guard against regressions. The CLI was also manually exercised on a generated 44.1 kHz harmonic-plus-noise WAV: a pitch ratio of 1.059463 produced a measured output F0 of approximately 233.27 Hz from an input at approximately 220.17 Hz, while preserving the 44,100-frame duration. This is a synthetic smoke test, not a perceptual evaluation on a real singer or voicebank. Real-world tuning requires annotated voicebank samples across registers, vowels, fricatives, plosives, breathy phonation, and expressive transitions.

## Not yet measured

Perceptual resynthesis quality on real voicebanks, exact harmonic/noise waveform separation, pitch-bend continuity, formant stability after transposition, consonant intelligibility, render latency in full UTAU projects, and long-session memory use remain outside Phase 3's verified claims. Synthetic tests establish numerical behavior, not production-quality vocal naturalness.
