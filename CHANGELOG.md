# Changelog

## 0.6.0 — Phase 6 compatibility and optimization

- Added WAVE_FORMAT_EXTENSIBLE support for standard PCM and IEEE-float subtypes.
- Added strict RIFF container, chunk boundary, padding, block alignment, and frame alignment validation.
- Added clear RF64 and oversized-data rejection messages.
- Added per-render reusable pitch-analysis buffers and precomputed Hann windows.
- Reused FFT scratch storage and precomputed spectral Hann tables during analysis.
- Added `--version` and `--capabilities` CLI queries.
- Added a standalone benchmark script reporting median render time and real-time factor.
- Added compatibility and optimization tests for WAV formats, malformed containers, and workspace equivalence.
- Actual UTAU/OpenUTAU Classic integration remains unverified and is not claimed as complete.


## 0.5.1 — Natural voiced resynthesis refinement

- Replaced integer pitch marks with sub-sample parabolic peak refinement for smoother pitch-synchronous overlap-add.
- Corrected TD-PSOLA target-period mapping so pitch remains independent from duration stretching.
- Reused one waveform-preserving WSOLA fallback instead of generating and blending duplicate fallbacks.
- Changed correlated waveform-path transitions to complementary linear blends to avoid equal-power level buildup and phasey cancellation.
- Preserved exact sample transparency for neutral, same-duration rendering.
- Added regression coverage for unity transparency and ±1, ±3, and ±5 semitone pitch targets.
- Updated natural-path diagnostics and DSP documentation.

## 0.5.0 — Phase 5 expressive vocal resynthesis

- Added a documented expression-control layer to the conventional UTAU rendering path.
- Implemented standard-compatible `g`, `t`, `B`, `H`, and `Y` controls with case-sensitive parsing, validated ranges, and clean-room behavior notes.
- Implemented custom `br`, `sf`, `vv`, `cr`, `as`, `ng`, `nh`, `hb`, `dy`, `vib`, `vr`, `sb`, and `dc`/`dec` controls.
- Added independent formant-envelope movement, breath/noise scaling, harmonic balance, and a voice-voltage-inspired upper-partial tilt.
- Added continuous cents-based pitch automation, UTAU modulation-contour handling, and vibrato. The pitch curve is interpolated inside the oscillator phase integration and does not reset phase at control points.
- Added attack shaping, consonant-rate timing, dynamics contour processing, spectral brightness, low-pass filtering, note gain, and terminal decay.
- Preserved safe defaults, unknown-flag warnings, exact required note duration, and peak protection.
- Added regression coverage for flag parsing, invalid/unknown flag tolerance, pitch bend, vibrato, note gain, consonant rate, decay, and finite output.
- Updated README, flag reference, testing documentation, and project version to 0.5.0.

Phase 5 was verified with generated synthetic signals. Real voicebank perceptual tuning and host-application integration remain follow-up validation work beyond Phase 6.

## 0.4.0 — Phase 4 timing and phonetics

- Connected the conventional UTAU positional invocation to a new timing-aware `renderPhase4` path.
- Applied source offset, end-blank trimming, required duration, velocity-scaled fixed-region timing, and volume with sample-accurate output frame counts.
- Added conservative frame-level phonetic classification for voiced, unvoiced, transient, and silent regions using periodicity, aperiodicity, spectral flatness, spectral centroid, and energy-rise cues.
- Kept the fixed initial region and unvoiced/transient areas on a waveform-preserving WSOLA path.
- Added voicing-mask blending so the harmonic resynthesis path is used mainly for stable voiced material, while retaining a small amount of original waveform texture.
- Added time-mapped harmonic and aperiodic synthesis, allowing duration changes without changing the oscillator's target F0.
- Added median voiced-F0 targeting from the requested UTAU note, short join ramps, final peak protection, and unvoiced-only fallback behavior.
- Added tests for F0-stable vowel time stretching, consonant preservation, target-note transposition, exact note duration, unvoiced fallback, finite output, and the conventional UTAU WAV invocation.
- Updated architecture, DSP, UTAU compatibility, testing, and limitation documentation.
- Clarified third-argument compatibility: a numeric pitch percentage from 1–400 or a named note target is accepted.

This phase is verified with synthetic tests only. It is not a claim of production-quality naturalness or successful integration in the UTAU/OpenUTAU host applications.

## 0.3.1 — Phase 3 resynthesis quality update

- Changed the default harmonic phase-observation strength from 0.85 to 1.0.
- Referenced extracted harmonic phases to the center sample of each FFT frame.
- Added clean-sine residual and analysis-window phase-reference regression tests.

## 0.3.0 — Phase 3 harmonic/aperiodic resynthesis

- Added continuous-phase harmonic oscillator synthesis and an independently synthesized aperiodic path.
- Added spectral-envelope compensation, component gains, deterministic noise, metrics, and peak protection.

## 0.2.0 — Phase 2 audio analysis

- Added FFT, YIN F0 estimation, harmonic analysis, cepstral spectral-envelope estimation, and aperiodicity profiles.

## 0.1.0 — Phase 1 functional foundation

- Added WAV I/O, a C++20/CMake project, classic UTAU argument parsing, initial timing rendering, tests, and CI workflows.
