# Changelog

All entries describe the state of this repository snapshot. Host compatibility is only claimed where a real host test has been performed.

## 0.6.0 — Phase 6 WAV/CLI hardening and performance

- Added RIFF/WAVE_FORMAT_EXTENSIBLE input support for standard PCM and IEEE-float subtype GUIDs.
- Added RIFF container, chunk boundary, odd-byte padding, block-alignment, and frame-alignment validation.
- Added a narrowly scoped tolerance for the legacy RIFF-size error where the declared size is exactly eight bytes larger than the actual file; larger overruns remain errors.
- Added clear rejection of RF64, unsupported channel counts, unsupported codecs, and data chunks above 512 MiB.
- Added reusable per-render YIN pitch-analysis workspaces and precomputed Hann-window tables.
- Added `--version` and `--capabilities` CLI queries.
- Added a standalone benchmark script reporting median render time and real-time factor.
- Added regression coverage for extensible PCM24/float32, PCM32, float64, RIFF chunk ordering/padding, malformed containers, Unicode paths, output-directory creation, and workspace equivalence.
- Updated the documentation to distinguish the implemented UTAU-style positional subset from unimplemented OpenUtau-specific pitch-curve and cutoff encodings.
- **Not implemented in 0.6.0:** OpenUtau's Base64/run-length-encoded 12-bit pitch array, `!tempo` argument handling, and signed cutoff semantics. OpenUtau GUI integration remains unverified.

## 0.5.1 — Natural voiced resynthesis refinement

- Replaced integer pitch marks with sub-sample parabolic peak refinement for smoother pitch-synchronous overlap-add.
- Corrected TD-PSOLA target-period mapping so pitch remains independent from duration stretching.
- Reused one waveform-preserving WSOLA fallback instead of generating and blending duplicate fallbacks.
- Changed correlated waveform-path transitions to complementary linear blends to avoid equal-power level buildup and phasey cancellation.
- Preserved exact sample transparency for neutral, same-duration rendering.
- Added regression coverage for unity transparency and ±1, ±3, and ±5 semitone pitch targets.
- Updated natural-path diagnostics and DSP documentation.

## 0.5.0 — Phase 5 expression controls

- Added a documented expression-control layer to the conventional positional renderer.
- Implemented project-defined mappings for `g`, `t`, `B`, `H`, and `Y`, with case-sensitive parsing and validated ranges.
- Implemented custom `br`, `sf`, `vv`, `cr`, `as`, `ng`, `nh`, `hb`, `dy`, `vib`, `vr`, `sb`, and `dc`/`dec` controls.
- Added independent formant-envelope movement, breath/noise scaling, harmonic balance, and a voice-voltage-inspired upper-partial tilt.
- Added continuous cents-based pitch automation and vibrato on the harmonic-expression route.
- Added attack shaping, consonant-rate timing, dynamics contour processing, spectral brightness, low-pass filtering, note gain, and terminal decay.
- Preserved safe defaults, unknown-flag warnings, exact required note duration, and peak protection.
- Added regression coverage for expression parsing and DSP behavior.

## 0.4.0 — Phase 4 timing and phonetics

- Connected the conventional positional invocation to a timing-aware renderer.
- Applied source offset, end-blank trimming, required duration, velocity-scaled fixed-region timing, and volume with exact requested output-frame counts.
- Added conservative acoustic classification for voiced, unvoiced, transient, and silent regions.
- Kept fixed and unvoiced/transient areas on waveform-preserving paths and blended resynthesis into stable voiced regions.
- Added time-mapped synthesis, note-target pitch adjustment, unvoiced-only fallback, and peak protection.
- Added tests for timing, consonant preservation, target pitch, and WAV output.

## 0.3.1 — Phase 3 phase-reference refinement

- Changed the default harmonic phase-observation strength from 0.85 to 1.0.
- Referenced extracted harmonic phases to the center sample of each FFT frame.
- Added clean-sine residual and analysis-window phase-reference regression tests.

## 0.3.0 — Phase 3 harmonic/aperiodic resynthesis

- Added continuous-phase harmonic oscillator synthesis and an independently synthesized aperiodic path.
- Added spectral-envelope compensation, component gains, deterministic noise, metrics, and peak protection.

## 0.2.0 — Phase 2 audio analysis

- Added FFT, YIN F0 estimation, harmonic analysis, cepstral spectral-envelope estimation, and aperiodicity profiles.

## 0.1.0 — Phase 1 functional foundation

- Added WAV I/O, a C++20/CMake project, positional argument parsing, initial timing rendering, tests, and CI workflows.
