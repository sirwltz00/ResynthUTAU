# Limitations and implementation status — version 0.6.0

This page separates implemented functionality from approximations and work that is not yet complete.

## Implemented functionality

### Phase 1 — file and render foundation

- C++20/CMake executable and Windows/Linux build workflows.
- RIFF/WAVE reader for PCM16/24/32 and IEEE float32/64, mono/stereo.
- PCM16 WAV output.
- UTAU-style positional argument parsing, note-name targets, and numeric pitch percentages.
- Offset/end-blank cropping, fixed-region velocity duration mapping, WSOLA-style timing adjustment, exact requested output frame count, volume, finite-value handling, and clipping protection.

### Phase 2 — audio analysis

- In-place radix-2 FFT and normalized inverse FFT.
- FFT-autocorrelation YIN F0 estimation with parabolic lag refinement and periodicity confidence.
- Voiced/unvoiced evidence, adaptive analysis windows, harmonic partial amplitude/phase measurements, cepstral spectral envelope, aperiodicity profiles, spectral centroid, and spectral flatness.
- Compact 256-band log-frequency profiles, analysis summary, and detailed JSON report export.

### Phase 3 — harmonic/aperiodic diagnostic resynthesis

- Harmonic oscillator-bank reconstruction with continuous phase integration and bounded inter-frame phase corrections.
- Pitch-ratio transposition, optional spectral-envelope compensation, stochastic aperiodic/noise synthesis, deterministic seed behavior, component gains, metrics, and peak protection.
- Phase 3.1 center-referenced phase observations and clean-sine residual tests.

### Phase 4 and Phase 5.1 — timing, phonetics, and natural pitch shifting

- Fixed-region timing, offset/end blank, required length, velocity scaling, and volume.
- Acoustic classification for voiced, unvoiced, transient, and silent regions.
- WSOLA waveform-preserving path for fixed, unvoiced, transient, and fallback material.
- Default voiced pitch-shift path using waveform-derived TD-PSOLA with fractional pitch-mark refinement and separate pitch/time mapping.
- Single complementary transition between correlated waveform paths.
- Exact neutral same-duration transparency as a tested invariant.

### Phase 5 — expression controls

- Project-defined standard-compatible flags `g`, `t`, `B`, `H`, and `Y`.
- Custom `br`, `sf`, `vv`, `cr`, `as`, `ng`, `nh`, `hb`, `dy`, `vib`, `vr`, `sb`, and `dc`/`dec` controls.
- Harmonic/aperiodic gains, breath/noise shaping, envelope-based formant movement, brightness/filtering, dynamics, attack, decay, vibrato, and cents-based pitch automation.
- Unknown flag warnings and value-range validation.

### Phase 6 — WAV/CLI hardening and optimization

- WAVE_FORMAT_EXTENSIBLE PCM and IEEE-float subtype handling.
- RIFF chunk/container boundary, padding, block-alignment, and data-frame validation.
- Narrow tolerance for the exact eight-byte RIFF-size overstatement found in one supplied sample.
- RF64 and data chunks above 512 MiB rejected explicitly.
- Reusable per-render YIN workspaces and precomputed Hann-window tables.
- `--version`, `--capabilities`, and a standalone performance benchmark script.
- UTF-8 Windows command-line conversion is implemented in `wmain`; actual Windows runtime behavior remains unverified.

## Approximations and caveats

- Stereo input is downmixed to mono for analysis. The mono synthesized component is blended into each channel's waveform-preserved path; independent stereo phase is not reconstructed.
- YIN can be unreliable on very breathy, noisy, polyphonic, or strongly transient material. Median source F0 may not represent a strongly changing phrase.
- The phonetic classifier is acoustic, not language-aware. It does not read `oto.ini`, aliases, phoneme labels, reclist entries, or phonemizer output.
- The transient detector can miss subtle stops or overprotect sharp vowel attacks. WSOLA can smear short consonants at extreme time changes.
- TD-PSOLA is waveform-derived, but its pitch-mark and grain selection are still heuristic and may produce roughness on irregular, creaky, breathy, or highly modulated voices.
- The cepstral spectral envelope is not a dedicated high-resolution formant tracker. Extreme formant movement can alter vowel colour unnaturally.
- The stochastic aperiodic path is shaped by an estimated residual and does not recreate the exact original residual waveform or phase.
- Expression controls are DSP approximations, not physically measured vocal-fold parameters or proprietary parameter replicas.
- The short real-sample comparisons in the Phase 6 report do not establish universal perceptual quality.

## Not implemented in 0.6.0

- OpenUtau's Base64/run-length-encoded signed 12-bit pitch curve decoding.
- OpenUtau's `!<tempo>` trailing token semantics.
- OpenUtau's signed cutoff/from-end semantics.
- Complete host adapters for all UTAU-derived resamplers and their custom flag conventions.
- Native OpenUtau renderer API.
- Language-aware phoneme recognition or direct `oto.ini` parsing.
- RF64, compressed WAV codecs, more than two channels, or streaming of very large data chunks.
- macOS, Android, or iOS builds.

## Host compatibility status

The standalone positional interface and WAV renderer are tested through local command-line tests. The project has not yet been launched by UTAU or OpenUtau GUI. The presence of `ResynthUTAU.yaml` is only an experimental expression-manifest integration; it is not proof of OpenUtau compatibility. See [OPENUTAU_INTEGRATION.md](OPENUTAU_INTEGRATION.md).

## Piapro NT relationship

ResynthUTAU is independent software. It uses conventional DSP techniques such as FFT analysis, YIN F0 estimation, TD-PSOLA, WSOLA, cepstral envelope estimation, and harmonic/aperiodic synthesis. It does not use proprietary Piapro NT source code, internal models, encrypted voicebank assets, or undocumented private parameters, and it does not claim to reproduce Piapro NT internally.
