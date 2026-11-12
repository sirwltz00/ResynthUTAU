# ResynthUTAU

**Version 0.6.1 — OpenUtau Classic external-resampler adapter / Windows x64 packaging**

ResynthUTAU is an independent C++20 vocal analysis and resynthesis project with a UTAU-style positional command-line renderer. Its default voiced pitch-shifting path uses waveform-derived TD-PSOLA, while WSOLA preserves the fixed region, unvoiced material, and transients. An alternate harmonic/aperiodic synthesis path supplies expression controls that require spectral or oscillator processing.

The project is a clean-room DSP implementation. It is inspired by publicly described vocal-synthesis goals, but it is not affiliated with Crypton Future Media or Yamaha and does not claim to reproduce Piapro Studio NT, VOCALOID, or any other proprietary engine.

> **Compatibility status:** Version 0.6.1 implements OpenUtau Classic's 13-argument external-resampler command-line adapter, including its `!tempo` token, signed 12-bit Base64/run-length pitch curve, and signed cutoff semantics. Automated adapter tests pass. However, no real OpenUtau GUI session has launched this build yet, so host integration remains **unverified**, not guaranteed. It is an external Classic resampler, not a native OpenUtau renderer.

The project is a clean-room DSP implementation. It is inspired by publicly described vocal-synthesis goals, but it is not affiliated with Crypton Future Media or Yamaha and does not claim to reproduce Piapro Studio NT, VOCALOID, or any other proprietary engine.

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
- Continuous cents-based pitch points and vibrato; OpenUtau's encoded pitch curve is also applied on the natural TD-PSOLA path when no oscillator/spectral expression forces the harmonic path.
- Full ranges, defaults, and the clean-room meaning of each flag are documented in [docs/FLAGS.md](docs/FLAGS.md).

### Phase 6 file handling and performance work

- RIFF/WAVE validation for container size, chunk boundaries, padding, block alignment, and frame alignment.
- PCM16/24/32 and IEEE-float32/64 input, including WAVE_FORMAT_EXTENSIBLE with standard PCM or IEEE-float subtype GUIDs.
- Mono and stereo input; PCM16 output at the original sample rate and channel count.
- Reusable per-render pitch-analysis workspaces and precomputed Hann-window tables.
- `--version` and `--capabilities` diagnostic commands.
- A Python benchmark script for measuring fresh-process render time and real-time factor.

## OpenUtau Classic integration

The CLI recognises OpenUtau Classic's 13-argument external-resampler invocation. It decodes the signed 12-bit Base64 pitch points and `#count#` repeat markers, parses `!<tempo>`, maps the signed cutoff convention, and applies the curve on the note's output timeline. The original standalone UTAU-style CLI remains available. The included `ResynthUTAU.yaml` manifest exposes the project's expression controls. Actual GUI launch, path quoting, render cancellation, manifest discovery, and real voicebank behavior have not been tested in OpenUtau yet. See [docs/UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) and [docs/OPENUTAU_INTEGRATION.md](docs/OPENUTAU_INTEGRATION.md).

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

The standalone command accepts literal cents points and an optional `Q<tempo>` grid. OpenUtau Classic sends its own `!<tempo>` plus Base64/run-length pitch payload, which the adapter decodes automatically when it detects the 13-argument host invocation. See [docs/UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) for the exact mapping.

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

See [docs/BUILDING.md](docs/BUILDING.md), [docs/TESTING.md](docs/TESTING.md), [docs/PERFORMANCE.md](docs/PERFORMANCE.md), and [docs/FLAGS.md](docs/FLAGS.md). For the Windows x64 package, run `scripts/build-windows-x64.ps1` on a Visual Studio 2022 machine, or push the source to GitHub and run the **Windows x64 package** workflow from the Actions tab. The workflow can also build automatically on relevant pushes and pull requests; it uploads an artifact named `ResynthUTAU-windows-x64` containing the installable ZIP and its SHA-256 file.

## Platforms

- **Windows x64:** an MSVC x64 build/package script and a dedicated GitHub Actions workflow are included. The workflow runs on `windows-2022` with Visual Studio 2022 x64, runs the test suite, and uploads the installable package as a 30-day Actions artifact. The package uses the static MSVC runtime. A Windows binary must be produced by running that script on Windows or the workflow on GitHub Actions; this Linux build environment has no Windows SDK or cross-compiler, so no `.exe` is included in this source archive.
- **Linux x64:** built and tested locally.
- **macOS:** not configured or verified.
- **Android/iOS:** no native mobile build is provided.

## Known limitations

- OpenUtau Classic adapter is not yet validated inside the actual OpenUtau GUI; command-line tests cannot prove end-to-end host behavior.
- No native OpenUtau renderer API integration; the project is an external Classic resampler.
- Phonetic timing is acoustic and heuristic; the executable does not directly parse oto.ini aliases or phonemizer output.
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
8. **Phase 7 — OpenUtau Classic adapter:** the 13-argument adapter, pitch decoder, tempo handling, and signed cutoff path are implemented; next validate the manifest and test real host rendering on Windows and Linux.
