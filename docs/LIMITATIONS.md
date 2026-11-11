# Limitations and implementation status — Phase 4

## Implemented in Phase 1

- C++20/CMake executable and Windows/Linux build workflows.
- RIFF/WAVE reader for PCM16/24/32 and IEEE float32/64, mono/stereo.
- PCM16 WAV output.
- UTAU-style classic positional argument parsing, note-name targets, and numeric pitch percentages.
- Offset/end-blank cropping, fixed-region velocity duration mapping, WSOLA-style duration adjustment, exact requested output frame count, volume, finite-value handling, and clipping protection.

## Implemented in Phase 2

- In-place radix-2 FFT and normalized inverse FFT.
- FFT-autocorrelation YIN F0 estimation with parabolic lag refinement and periodicity confidence.
- Voiced/unvoiced frame labels based on RMS and periodicity thresholds.
- Adaptive power-of-two spectral windows targeting a configurable number of F0 cycles.
- Harmonic partial amplitude and local phase observations.
- Cepstral-lifter spectral-envelope estimation.
- Soft harmonic-mask spectral aperiodicity/noise estimate.
- Spectral centroid and flatness.
- Compact 256-band log-frequency spectral-envelope and aperiodicity profiles per frame.
- `--analyze` CLI mode and detailed JSON report export.

## Implemented in Phase 3

- Harmonic oscillator-bank reconstruction from analyzed harmonic partial amplitudes.
- Sample-by-sample oscillator phase integration and bounded inter-frame phase corrections.
- Pitch-ratio transposition in the offline API and `--resynthesize` diagnostic command.
- Optional spectral-envelope amplitude compensation using the compact cepstral profile.
- Independent stochastic aperiodic/noise synthesis with spectral shaping and deterministic seed behavior.
- Component gains, output metrics, edge fades, and global peak guard.
- Phase 3.1 center-referenced phase measurements and clean-sine residual regression.

## Implemented in Phase 4

- Conventional UTAU positional invocation is connected to `renderPhase4`.
- Offset, end blank, fixed length, velocity duration mapping, required length, volume, and exact output frame count.
- The initial fixed region is waveform-preserved at velocity 100 and WSOLA-adjusted when its duration changes.
- Frame-level voiced/unvoiced/silence/transient classification using periodicity, local aperiodicity, spectral flatness, centroid, and RMS rise.
- Smoothed voicing confidence for consonant-to-vowel and vowel-to-consonant transitions.
- Waveform-preserving WSOLA path for unvoiced and transient regions.
- Time-mapped harmonic resynthesis for voiced material, preserving oscillator pitch when output duration changes.
- A low-level synthesized aperiodic component and a small original-waveform blend to retain some natural noise texture.
- Median voiced-F0 targeting from a requested note, or relative pitch adjustment from a numeric percentage.
- Short join ramp, peak guard, and unvoiced-only fallback.
- Synthetic tests for timing, fixed-region preservation, F0-preserving sustain, target pitch, unvoiced fallback, and conventional WAV output.

## Approximations and caveats

- Stereo input is downmixed to mono for analysis. The mono synthesized component is blended into each channel's waveform-preserved path; independent stereo phase is not reconstructed.
- The YIN tracker can be unreliable on very breathy, noisy, polyphonic, or strongly transient material, and median source F0 may not represent all multi-pitch or expressive samples.
- The spectral envelope is cepstrally smoothed and compressed into 256 log-frequency values; it is not an LPC/pole formant tracker.
- The aperiodicity estimate is an approximate spectral residual. Its resynthesized noise is stochastic and does not recreate the exact original residual waveform.
- The phonetic classifier is not a phoneme recognizer. It cannot know language, alias, consonant identity, or OTO boundaries. It can misclassify unusual vowels, breathy phonation, and short consonants.
- Transient detection is heuristic. It relies on local energy rise plus spectral characteristics; subtle stops can be missed and sharp vowel attacks may be overprotected.
- WSOLA may smear very short consonants under extreme time changes. Regions shorter than 96 samples use a linear interpolation fallback.
- The initial fixed-region velocity mapping is retained from the earlier project convention and is not claimed to match every UTAU host/resampler implementation.
- Output is converted to PCM16, even when input is 24-bit or floating point.

## Implemented in Phase 3

- Harmonic oscillator-bank reconstruction from analyzed harmonic partial amplitudes.
- Sample-by-sample oscillator phase integration based on interpolated F0.
- Bounded, distributed phase corrections from local frame phase observations; no per-frame oscillator resets.
- Linear interpolation of F0 and harmonic amplitudes between analysis frames.
- Pitch-ratio transposition in the offline resynthesis API and `--resynthesize` command.
- Optional spectral-envelope amplitude compensation using the compact cepstral profile.
- Independent stochastic aperiodic/noise synthesis with spectral shaping from the estimated residual profile.
- Deterministic random-phase generation for a fixed seed and build.
- Window-power-normalized overlap-add for noise blocks.
- Independent harmonic/noise gain controls, component metrics, edge fades, mono/stereo-container output, and global peak guard.
- `--resynthesize input.wav output.wav [pitch_ratio]` diagnostic CLI.

## Not implemented

- Pitch-bend curves, modulation, vibrato, portamento, and time-varying pitch automation.
- Breathiness, dynamics, note gain, voice-voltage-inspired control, consonant-rate, attack-speed, harmonic balance, noise-level, and custom expression flags.
- Dedicated high-resolution formant tracking and independent formant shifting.
- Explicit phoneme labels, `oto.ini` parsing, language-aware consonant classification, or phoneme-specific DSP profiles.
- Full transient-aware phase-vocoder or neural consonant reconstruction.
- Actual UTAU/OpenUTAU Classic application-level integration tests and listening evaluation on real voicebanks.
- macOS build and native OpenUTAU renderer support.

## UTAU compatibility status

The conventional positional renderer now runs Phase 4 timing and phonetic processing, including target-note pitch adjustment on stable voiced material. The application has not yet been exercised inside UTAU or OpenUTAU Classic in this development environment. Do not treat this build as production-validated until host tests and real voicebank listening tests are complete.

## Piapro NT relationship

ResynthUTAU is an independent clean-room project. Piapro Studio NT's exact internal algorithm is not asserted, reproduced, or reverse-engineered. Publicly described synthesis principles serve only as high-level inspiration.
