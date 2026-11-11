# Limitations and implementation status

## Implemented in Phase 1

- C++20/CMake project and executable entry point.
- RIFF/WAVE reader for PCM16/24/32 and IEEE float32/64, mono/stereo.
- PCM16 WAV output.
- UTAU-style classic positional argument parsing and note-name validation.
- Offset and end-blank trimming.
- Fixed-region velocity duration mapping.
- WSOLA-style duration adjustment for sufficiently long regions, with a short-region interpolation fallback.
- Required output-duration handling, volume, finite-value handling, and clipping clamp.
- Unit tests and Windows/Linux CI/release workflow definitions.

## Approximations

- The WSOLA implementation is an initial waveform-domain renderer, not an advanced transient classifier. Its correlation uses a mono downmix, while its chosen alignment is shared by all channels.
- Very short segments use linear interpolation because there is insufficient context for a stable WSOLA search.
- Phase 1's velocity duration formula is a documented project convention. It is not a claim to duplicate undocumented UTAU internals.
- Output is converted to PCM16, even when input is 24-bit or floating point.

## Not implemented

- F0 analysis and pitch shifting.
- Harmonic extraction and resynthesis.
- Aperiodic/noise extraction and resynthesis.
- Spectral-envelope and formant analysis or formant shifting.
- Pitch bends, vibrato, portamento, or continuous pitch automation.
- Breathiness, harmonic balance, voice voltage, noise shaping, or other expression controls.
- Consonant/vowel classification, transient-aware segmentation, or vowel-sustain modeling.
- FFT/STFT, cepstral/LPC analysis, or phase-coherent harmonic synthesis.
- Actual UTAU/OpenUTAU Classic application-level integration tests.
- macOS build and native OpenUTAU renderer support.

## Compatibility warning

The Phase 1 binary is a development foundation, not yet a musically complete resampler. It can validate the host invocation, WAV decoding, timing, duration, and basic output generation. It will not transpose notes to the requested pitch, so it should not be treated as ready for normal UTAU production until later stages are implemented and verified.

## Piapro NT relationship

ResynthUTAU is an independent clean-room project. Piapro Studio NT's exact internal algorithm is not asserted, reproduced, or reverse-engineered. Publicly described signal-processing concepts are used only as high-level inspiration.
