# ResynthUTAU

**Phase 2 — analysis engine, version 0.2.0**

ResynthUTAU is an independent C++20 UTAU resampler and vocal-analysis project. Its long-term direction is a harmonic-plus-aperiodic resynthesis engine inspired by publicly described vocal-synthesis principles associated with Piapro Studio NT. It is not affiliated with or endorsed by Crypton Future Media or Yamaha, and it does not copy proprietary source code, voicebank data, or undocumented algorithms.

> **Current status:** Phase 1 provides WAV I/O and a functional UTAU-style timing renderer. Phase 2 adds an offline analysis engine: FFT, YIN F0 tracking, voiced/unvoiced classification, harmonic partial measurements, cepstral spectral-envelope estimation, aperiodicity/noise estimates, and detailed JSON reports. Phase 2 analysis is not yet connected to the UTAU render path. Pitch transposition and harmonic-plus-aperiodic waveform reconstruction are not implemented, so this is still a development build rather than a production singing resampler.

## What's new in Phase 2

- In-place radix-2 FFT and inverse FFT with no external DSP dependency.
- FFT-autocorrelation YIN F0 analysis with parabolic lag refinement and periodicity confidence.
- Adaptive spectral-window selection based on the tracked fundamental frequency.
- Harmonic partial amplitude and local phase analysis.
- Cepstral-lifter spectral-envelope estimation.
- Independent spectral harmonic-mask and aperiodic/noise energy estimates.
- Spectral centroid and spectral flatness descriptors.
- Compact 256-band log-frequency spectral-envelope and aperiodicity profiles per frame.
- `--analyze` command for console summaries and detailed JSON report export.

## Phase 1 capabilities retained

- C++20 and CMake; no Python, Node.js, MATLAB, or runtime dependency for the executable.
- RIFF/WAVE input: PCM16/24/32 and IEEE float32/64, mono or stereo.
- PCM16 WAV output at the source sample rate.
- Unicode Windows command-line argument conversion and filesystem path handling.
- UTAU-style positional argument parsing and note-name validation.
- Offset/end-blank trimming, fixed-region velocity timing, WSOLA-style duration adjustment, requested duration, and volume.
- CTest regression suite and Windows/Linux GitHub Actions workflow definitions.

## Analyze a WAV file

Print a concise report:

```text
ResynthUTAU --analyze voice_sample.wav
```

Write a detailed JSON report, including per-frame F0, periodicity, harmonic partials, spectral-envelope bands, and aperiodicity bands:

```text
ResynthUTAU --analyze voice_sample.wav analysis.json
```

The analysis path downmixes stereo to mono for feature extraction and leaves the source file unchanged. JSON reports can be large because they contain 256-band profiles for every analysis frame.

## UTAU command-line usage

```text
ResynthUTAU <input.wav> <output.wav> <pitch_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

Example:

```text
ResynthUTAU input.wav output.wav C4 100 - 0 500 100 0 100 0
```

The classic UTAU-style render path still uses the Phase 1 timing renderer. The note is parsed and validated but is not applied to the waveform yet. Unsupported flags, modulation, and pitch-bend values are reported as limitations. Do not use this development build for finished UTAU production.

See [UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) and [FLAGS.md](docs/FLAGS.md) for details.

## DSP architecture and limitations

Read [DSP_ALGORITHMS.md](docs/DSP_ALGORITHMS.md) for the analysis methods, parameter defaults, and known approximations. [ARCHITECTURE.md](docs/ARCHITECTURE.md) describes the module boundaries. [LIMITATIONS.md](docs/LIMITATIONS.md) separates implemented features from approximations and future work.

The aperiodicity estimator is currently a spectral mask/residual estimate, not a time-domain noise residual. Harmonic phase values are local frame observations, not continuous phase trajectories. These outputs are analysis features for the next stage, not finished synthesis controls.

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler (MSVC 2022, GCC 11+, or Clang 14+ recommended).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

See [BUILDING.md](docs/BUILDING.md) for platform notes and GitHub Actions details, and [TESTING.md](docs/TESTING.md) for coverage and objective tolerances.

## Platforms

- **Windows 10/11 x64:** primary target; the workflow builds `ResynthUTAU.exe`.
- **Linux x64:** supported development/test build.
- **macOS:** not configured or verified yet.

## Project roadmap

1. **Phase 1 — functional foundation:** WAV I/O, UTAU arguments, timing renderer, initial CI.
2. **Phase 2 — analysis:** FFT, F0, harmonic measurements, spectral envelope, aperiodicity, voiced/unvoiced labels. Implemented in this version.
3. **Phase 3 — resynthesis:** phase-continuous harmonic synthesis, independent aperiodic/noise reconstruction, spectral interpolation, and component merging.
4. **Phase 4 — timing and phonetics:** transient-aware consonant preservation, vowel sustain, and transitions.
5. **Phase 5 — expression:** pitch automation, formant controls, breathiness, dynamics, and additional expression parameters.
6. **Phase 6/7 — compatibility, optimization, and release:** testing in real UTAU/OpenUTAU Classic environments and packaged releases.
