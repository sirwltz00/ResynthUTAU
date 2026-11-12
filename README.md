# ResynthUTAU

**Version 0.6.0 — Phase 6: WAV/CLI hardening and performance**

ResynthUTAU is an independent C++20 vocal analysis and resynthesis project with a UTAU-style positional command-line renderer. Its default voiced pitch-shifting path uses waveform-derived TD-PSOLA, while WSOLA preserves the fixed region, unvoiced material, and transients. An alternate harmonic/aperiodic synthesis path supplies expression controls that require spectral or oscillator processing.

The project is a clean-room DSP implementation. It is inspired by publicly described vocal-synthesis goals, but it is not affiliated with Crypton Future Media or Yamaha and does not claim to reproduce Piapro Studio NT, VOCALOID, or any other proprietary engine.

> **Compatibility status:** Version 0.6.0 builds and passes its local test suite. It provides a UTAU-style positional CLI, but it is **not yet a fully compatible OpenUtau Classic resampler**. In particular, the current source does not decode OpenUtau's Base64/run-length-encoded 12-bit pitch curve or implement its signed cutoff convention. The OpenUtau manifest is supplied for development/preview only. Actual UTAU/OpenUtau GUI integration has not been verified.

> **Current status:** Phases 1–6 are implemented locally and covered by synthetic regression tests. Phase 6 adds strict-but-tolerant RIFF/WAVE parsing, WAVE_FORMAT_EXTENSIBLE PCM/float support, reusable per-render DSP workspaces, capability reporting, and a reproducible benchmark. The supplied real sample renders identically to Phase 5.1 at neutral pitch and at six tested semitone ratios. Actual runtime testing inside UTAU/OpenUTAU Classic and Windows execution remain unverified.

### Audio analysis

- Radix-2 FFT and inverse FFT.
- FFT-autocorrelation YIN F0 estimation with parabolic lag refinement and periodicity confidence.
- Adaptive analysis windows, harmonic partial measurements, centered phase observations, cepstral spectral-envelope estimation, and aperiodicity profiles.
- RMS, spectral centroid, spectral flatness, voiced/unvoiced classification, and JSON analysis reports.

### Resynthesis and timing

- Default neutral and pitch-shifted voiced path: waveform-derived TD-PSOLA with sub-sample pitch-mark refinement.
- WSOLA waveform path for the initial fixed region, unvoiced material, transients, and the natural-path fallback.
- A conservative acoustic classifier for silence, voiced, unvoiced, and transient regions. It does not identify language-specific phonemes.
- Offset, end trimming, fixed-region duration, required output duration, velocity scaling, volume, and exact output-frame count.
- A separate phase-continuous harmonic/aperiodic path for controls that require oscillator or spectral processing.
- Peak protection and finite-value checks.

### Expression controls

- Standard-compatible project mappings for `g`, `t`, `B`, `H`, and `Y`.
- Custom controls: `br`, `sf`, `vv`, `cr`, `as`, `ng`, `nh`, `hb`, `dy`, `vib`, `vr`, `sb`, and `dc`/`dec`.
- Continuous cents-based pitch points and vibrato on the harmonic expression path.
- Full ranges, defaults, and the clean-room meaning of each flag are documented in [docs/FLAGS.md](docs/FLAGS.md).

### Phase 6 file handling and performance work

- RIFF/WAVE validation for container size, chunk boundaries, padding, block alignment, and frame alignment.
- PCM16/24/32 and IEEE-float32/64 input, including WAVE_FORMAT_EXTENSIBLE with standard PCM or IEEE-float subtype GUIDs.
- Mono and stereo input; PCM16 output at the original sample rate and channel count.
- Reusable per-render pitch-analysis workspaces and precomputed Hann-window tables.
- `--version` and `--capabilities` diagnostic commands.
- A Python benchmark script for measuring fresh-process render time and real-time factor.

## Important OpenUtau limitation

OpenUtau Classic currently appends a tempo token (`!<tempo>`) and a Base64-encoded, run-length-compressed 12-bit pitch array to its external-resampler command line. This source snapshot only accepts literal numeric cents tokens and an optional `Q<tempo>` convention; it does not decode the OpenUtau payload. It also treats the ninth argument as a non-negative end-blank duration, not OpenUtau's signed cutoff field. Therefore do not treat the current package as fully OpenUtau-compatible until the adapter and host tests are completed. See [docs/UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) and [docs/OPENUTAU_INTEGRATION.md](docs/OPENUTAU_INTEGRATION.md).

## Command-line usage

The positional interface implemented by this project is:

```text
ResynthUTAU <input.wav> <output.wav> <pitch_percent_or_note> <velocity> [flags [offset_ms [length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend_tokens...]]]]]]]]
```

Example: render a note target with a 50 ms fixed region and a 650 ms output:

```bash
ResynthUTAU voice.wav rendered.wav A#3 100 - 0 650 50 0 100 0
```

Example: apply a relative 1.2× frequency ratio:

```bash
ResynthUTAU voice.wav rendered.wav 120 100 - 0 650 50 0 100 0
```

The third argument accepts either a note name such as `A#3` or a numeric pitch percentage from 1 to 400 (`100` means unchanged F0; `120` means a 1.2× ratio). When later positional arguments are supplied without flags, use `-` as the empty flag placeholder.

This project's optional trailing pitch points are literal cents values. A leading `Q<tempo>` selects the project's tempo grid. They are **not** OpenUtau's encoded Base64 pitch curve. See [docs/UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) for the exact argument mapping and limitations.

## Diagnostic commands

```bash
ResynthUTAU --version
ResynthUTAU --capabilities
ResynthUTAU --analyze voice_sample.wav [report.json]
ResynthUTAU --resynthesize voice_sample.wav reconstructed.wav [pitch_ratio]
```

`--analyze` prints a summary and optionally writes detailed JSON. `--resynthesize` is the standalone Phase 3 analysis-to-waveform diagnostic; the conventional positional invocation uses the timing, phonetic, and expression renderer.

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler. MSVC 2022, GCC 11+, or Clang 14+ are recommended. The executable has no third-party runtime DSP or audio-library dependency. Python 3 is only required for the optional benchmark script.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

See [docs/BUILDING.md](docs/BUILDING.md), [docs/TESTING.md](docs/TESTING.md), [docs/PERFORMANCE.md](docs/PERFORMANCE.md), and [docs/FLAGS.md](docs/FLAGS.md).

## Platforms

- **Windows x64:** a Windows build is configured in GitHub Actions; this local Phase 6 session did not run the resulting executable on Windows.
- **Linux x64:** built and tested locally.
- **macOS:** not configured or verified.
- **Android/iOS:** no native mobile build is provided.

## Known limitations

- No complete OpenUtau Classic argument adapter yet: Base64/run-length pitch data and signed cutoff semantics are not implemented.
- No native OpenUtau renderer API integration.
- No runtime host validation in UTAU or OpenUtau GUI.
- The phonetic classifier is acoustic and heuristic; it does not parse `oto.ini`, aliases, reclists, or phonemizer output.
- Very short consonants and difficult transients can be smeared by WSOLA or misclassified by the acoustic detector.
- F0 estimates can be unreliable on very breathy, noisy, polyphonic, or strongly varying material.
- The cepstral spectral envelope is not a dedicated high-resolution formant tracker. Extreme shifts can change vowel colour unnaturally.
- The stochastic aperiodic path is shaped by an estimated residual; it does not recover the exact original noise waveform or phase.
- Expression controls are clean-room DSP approximations, not physical vocal-fold parameters or proprietary flag replicas.
- The natural TD-PSOLA path has been tested on synthetic signals and a supplied short sample, but broader perceptual testing across voicebanks and phonation styles is still needed.
- RF64, compressed WAV codecs, more than two channels, and data chunks above 512 MiB are unsupported.

See [docs/LIMITATIONS.md](docs/LIMITATIONS.md) for the detailed status.

## Roadmap

1. **Phase 1 — functional foundation:** implemented.
2. **Phase 2 — audio analysis:** implemented.
3. **Phase 3 — harmonic/aperiodic analysis-to-resynthesis:** implemented as a diagnostic/offline path.
4. **Phase 4 — timing and phonetics:** implemented with acoustic region classification and WSOLA/voiced synthesis integration.
5. **Phase 5 — expression controls:** implemented as documented project-specific controls.
6. **Phase 5.1 — natural voiced refinement:** TD-PSOLA pitch-mark refinement, independent pitch/duration mapping, and cleaner correlated-path transitions implemented.
7. **Phase 6 — WAV/CLI hardening and optimization:** implemented and locally tested.
8. **Phase 7 — host compatibility:** implement OpenUtau's encoded pitch/cutoff arguments, validate the manifest, and run real host tests on Windows and Linux before calling it a host-tested release.
