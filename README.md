# ResynthUTAU

**Phase 3.1 — resynthesis quality update, version 0.3.1**

ResynthUTAU is an independent C++20 UTAU resampler and vocal-analysis/resynthesis project. Its long-term direction is a harmonic-plus-aperiodic engine inspired by publicly described vocal-synthesis principles associated with Piapro Studio NT. It is not affiliated with or endorsed by Crypton Future Media or Yamaha, and it does not copy proprietary source code, voicebank data, or undocumented algorithms.

> **Current status:** Phase 1 provides WAV I/O and a functional UTAU-style timing renderer. Phase 2 adds FFT/YIN vocal analysis, harmonic partial measurements, cepstral spectral-envelope estimates, and aperiodicity profiles. Phase 3 now reconstructs the analysis as a continuous-phase harmonic oscillator bank plus independently synthesized, spectrally shaped aperiodic noise. It supports offline pitch-ratio changes and component mixing. **The conventional UTAU positional invocation still uses the Phase 1 timing renderer**; Phase 3 is exposed through a separate diagnostic command. Consonant/transient-aware rendering, note timing integration, and real-world UTAU/OpenUTAU validation remain future work.

## Phase 3.1 resynthesis-quality update

- Harmonic partial phases are now referenced to the center sample of each analysis frame, rather than the start of the FFT frame. This keeps the phase trajectory consistent when the adaptive analysis-window size changes.
- Full measured phase following (`phase_observation_strength = 1.0`) is now the default. This prevents the default renderer from leaving a systematic fraction of the phase error uncorrected on clean periodic inputs.
- Added regression coverage for a clean 220 Hz sine and for phase-reference consistency across 1024-, 2048-, and 4096-sample windows.

This is a targeted quality pass, not Phase 4. The current diagnostic renderer still does not integrate UTAU note timing or phoneme transitions.

## What's new in Phase 3

- Harmonic oscillator-bank resynthesis with sample-by-sample phase integration.
- Bounded inter-frame phase-observation corrections to preserve measured phase movement without resetting oscillators at analysis-frame boundaries.
- Interpolated harmonic amplitudes and F0 trajectories.
- Optional spectral-envelope compensation during pitch-ratio changes, using the Phase 2 cepstral profile as an approximation.
- Independent stochastic aperiodic synthesis from the analyzed log-frequency residual profile.
- Deterministic noise generation from a configurable seed and window-power-normalized overlap-add.
- Independent harmonic and aperiodic gains, output-channel selection, edge fades, and a global peak guard.
- `--resynthesize` command for offline analysis-to-waveform reconstruction.

## Offline resynthesis

Render a file at its analyzed pitch:

```text
ResynthUTAU --resynthesize voice_sample.wav reconstructed.wav
```

Transpose the harmonic component by a pitch ratio while keeping the analyzed duration:

```text
ResynthUTAU --resynthesize voice_sample.wav one_semitone_up.wav 1.059463094
```

A pitch ratio is a frequency multiplier: `1.0` is unchanged, `2.0` is one octave up, and `0.5` is one octave down. The command analyzes the input first, reconstructs the periodic and aperiodic paths, merges them, applies a peak guard, and writes PCM16 WAV at the input sample rate. The current CLI returns mono output because Phase 2 analysis averages input channels. The C++ API can duplicate the reconstructed mono signal into a stereo container.

The public C++ options are declared in `include/resynth/ResynthesisEngine.hpp`. They include pitch ratio, harmonic/noise gains, phase-observation strength, spectral-envelope compensation, noise seed, noise FFT size, output channels, peak ceiling, and edge-fade duration.

## Analyze a WAV file

Print a concise report:

```text
ResynthUTAU --analyze voice_sample.wav
```

Write a detailed JSON report:

```text
ResynthUTAU --analyze voice_sample.wav analysis.json
```

The report contains per-frame F0, periodicity, harmonic partials, spectral-envelope bands, and aperiodicity profiles. JSON reports can be large for long recordings.

## Phase 1 capabilities retained

- C++20 and CMake; no Python, Node.js, MATLAB, or runtime dependency for the executable.
- RIFF/WAVE input: PCM16/24/32 and IEEE float32/64, mono or stereo.
- PCM16 WAV output at the source sample rate.
- Unicode Windows command-line argument conversion and filesystem path handling.
- UTAU-style positional argument parsing and note-name validation.
- Offset/end-blank trimming, fixed-region velocity timing, WSOLA-style duration adjustment, requested duration, and volume.
- CTest regression suite and Windows/Linux GitHub Actions workflow definitions.

## UTAU command-line usage

```text
ResynthUTAU <input.wav> <output.wav> <pitch_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

The conventional invocation still uses the Phase 1 timing renderer. Its target note is parsed and validated but not yet applied as pitch transposition. Use `--resynthesize` to exercise Phase 3's analysis-to-resynthesis path. Phase 4 will integrate the analysis/synthesis engine with UTAU timing and phonetic regions.

See [UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) and [FLAGS.md](docs/FLAGS.md).

## DSP architecture and limitations

Read [DSP_ALGORITHMS.md](docs/DSP_ALGORITHMS.md) for the analysis and resynthesis methods, [ARCHITECTURE.md](docs/ARCHITECTURE.md) for module boundaries, and [LIMITATIONS.md](docs/LIMITATIONS.md) for the distinction between implemented features and approximations.

The noise path is a stochastic reconstruction shaped by an estimated spectral residual; it is not an exact inverse-STFT subtraction of the original source. The spectral-envelope compensation is a compact cepstral approximation, not a dedicated formant tracker. Harmonic phase observations are followed by continuous oscillators with bounded phase corrections, but this is not a full phase-locked vocoder. Consonants and transients are not yet classified or protected separately.

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler (MSVC 2022, GCC 11+, or Clang 14+ recommended).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

See [BUILDING.md](docs/BUILDING.md) for platform notes and GitHub Actions details, and [TESTING.md](docs/TESTING.md) for synthetic regression coverage.

## Platforms

- **Windows 10/11 x64:** primary target; the workflow builds `ResynthUTAU.exe`.
- **Linux x64:** supported development/test build.
- **macOS:** not configured or verified yet.

## Project roadmap

1. **Phase 1 — functional foundation:** WAV I/O, UTAU arguments, timing renderer, initial CI. Implemented.
2. **Phase 2 — analysis:** FFT, F0, harmonic measurements, spectral envelope, aperiodicity, voiced/unvoiced labels. Implemented.
3. **Phase 3 — resynthesis:** continuous-phase harmonic reconstruction, independent aperiodic synthesis, spectral interpolation, and component merging. Implemented as an offline path.
4. **Phase 4 — timing and phonetics:** integrate note timing, protect consonants/transients, vowel sustain, and phoneme transitions.
5. **Phase 5 — expression:** pitch automation, formant controls, breathiness, dynamics, and additional expression parameters.
6. **Phase 6/7 — compatibility, optimization, and release:** test in real UTAU/OpenUTAU Classic environments and publish verified packages.
