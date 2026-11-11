# Limitations and implementation status

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
- Harmonic phases are local per-frame observations and are not unwrapped or phase-locked over time.
- Aperiodicity is estimated with a tapered spectral mask around harmonic frequencies. This is an approximate spectral split, not a reconstructed time-domain noise residual.
- The Phase 1 WSOLA-style renderer uses a mono correlation estimate for alignment and a short-segment interpolation fallback.
- Phase 1 velocity timing is a project convention, not a claim to duplicate undocumented host internals.
- Output is converted to PCM16, even when input is 24-bit or floating point.

## Not implemented

- Harmonic oscillator-bank resynthesis or other Phase 3 reconstruction.
- Independent aperiodic waveform/noise synthesis.
- Pitch transposition or pitch bend in the UTAU render path.
- Formant shifting, Super Formant Shifter-like controls, or formant automation.
- Phase-continuous harmonic trajectories and phase-locked partial tracking.
- Time-domain consonant preservation, transient classification, consonant/vowel segmentation, or long-vowel sustain.
- Vibrato, portamento, breathiness, dynamics, voice-voltage, harmonic balance, or the custom expression flags.
- Actual UTAU/OpenUTAU Classic application-level integration tests.
- macOS build and native OpenUTAU renderer support.

## UTAU compatibility warning

The UTAU resampler invocation continues to use the Phase 1 timing renderer. Phase 2 adds a separate offline analysis tool; its F0, harmonic, envelope, and aperiodicity results are not yet connected to note rendering. Target notes are still not transposed. This version is not ready for normal singing production.

## Piapro NT relationship

ResynthUTAU is an independent clean-room project. Piapro Studio NT's exact internal algorithm is not asserted, reproduced, or reverse-engineered. Publicly described signal-processing concepts are used only as high-level inspiration.
