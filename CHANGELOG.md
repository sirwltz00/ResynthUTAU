# Changelog

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

This phase is verified with synthetic tests only. It is not a claim of production-quality naturalness or successful integration in the UTAU/OpenUTAU host applications. Phase 5 expression controls remain unimplemented.

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
