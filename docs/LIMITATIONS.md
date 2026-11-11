# Limitations and implementation status — Phase 3

## Implemented in Phase 1

- C++20/CMake executable and build workflows.
- RIFF/WAVE reader for PCM16/24/32 and IEEE float32/64, mono/stereo.
- PCM16 WAV output.
- UTAU-style classic positional argument parsing and note-name validation.
- Offset/end-blank trimming, fixed-region velocity duration mapping, WSOLA-style duration adjustment, exact requested output frame count, volume, finite-value handling, and clipping clamp.

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
- Synthetic-signal tests for FFT, F0, harmonics, unvoiced noise, mixed periodic/aperiodic content, and JSON output.

## Approximations

- Analysis downmixes stereo channels to mono by averaging them. It does not preserve independent stereo phase information.
- The YIN tracker uses fixed-size pitch frames, configurable F0 bounds, and initial default thresholds. It can confuse octave-related candidates or become unreliable on noisy, polyphonic, very breathy, or strongly transient material.
- The spectral envelope is cepstrally smoothed and compressed into 256 log-frequency values; it is not a dedicated formant/pole tracker.
- Harmonic phases begin as local per-frame observations; Phase 3 applies bounded corrections while integrating oscillator phase continuously, but does not implement a full phase-locked vocoder.
- Aperiodicity is estimated with a tapered spectral mask around harmonic frequencies. This is an approximate spectral split, not a reconstructed time-domain noise residual.
- The Phase 1 WSOLA-style renderer uses a mono correlation estimate for alignment and a short-segment interpolation fallback.
- Phase 1 velocity timing is a project convention, not a claim to duplicate undocumented host internals.
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

- Phase 3 output is not yet connected to the conventional UTAU positional renderer.
- Note-length, offset, fixed-consonant, end-blank, velocity, and pitch-bend automation are not applied to the Phase 3 render.
- Exact waveform-domain separation of the original harmonic and noise residuals; the noise output is stochastic.
- A dedicated high-resolution formant tracker or Super Formant Shifter-like processor.
- Transient classification, consonant waveform preservation, consonant/vowel segmentation, or long-vowel sustain.
- Vibrato, portamento, time-varying pitch curves, breathiness automation, dynamics, voice voltage, harmonic balance, or custom expression flags.
- Actual UTAU/OpenUTAU Classic application-level integration tests.
- macOS build and native OpenUTAU renderer support.

## UTAU compatibility warning

The UTAU resampler invocation continues to use the Phase 1 timing renderer. Phase 3 adds an independent offline analysis-to-resynthesis command that can reconstruct analyzed audio and apply a pitch ratio, but that output is not yet driven by UTAU note timing or phonetic-region metadata. Target notes in the conventional positional invocation are still not transposed. This version is not ready for normal singing production.

## Piapro NT relationship

ResynthUTAU is an independent clean-room project. Piapro Studio NT's exact internal algorithm is not asserted, reproduced, or reverse-engineered. Publicly described signal-processing concepts are used only as high-level inspiration.
