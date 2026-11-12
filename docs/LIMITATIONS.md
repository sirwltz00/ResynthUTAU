# Limitations and implementation status — Phase 6

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

- Conventional UTAU positional invocation uses the timing-aware renderer and is connected to the Phase 5 expression layer.
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

## Not implemented

- Dedicated high-resolution formant tracking or a full articulatory/vocal-fold model.
- Explicit phoneme labels, `oto.ini` parsing, language-aware consonant classification, or phoneme-specific DSP profiles.
- Arbitrary multi-segment vibrato shapes, portamento curves beyond the cents pitch-bend stream, or note-specific vibrato phase/delay editing.
- Full transient-aware phase-vocoder or neural consonant reconstruction.
- Actual UTAU/OpenUTAU Classic application-level integration tests and broad listening evaluation on real voicebanks.
- macOS build and native OpenUTAU renderer support.

## UTAU compatibility status

The conventional positional renderer now runs Phase 5 timing, phonetic, and expression processing, including target-note pitch adjustment and pitch automation on stable voiced material. Phase 6 improves WAV compatibility and reuses per-render analysis memory. The application has not yet been exercised inside UTAU or OpenUTAU Classic in this development environment. Do not treat this build as production-validated until host tests and real voicebank listening tests are complete. Phase 6 adds a local benchmark and file-format compatibility tests, but does not replace actual host validation.

## Piapro NT relationship

ResynthUTAU is an independent clean-room project. Piapro Studio NT's exact internal algorithm is not asserted, reproduced, or reverse-engineered. Publicly described synthesis principles serve only as high-level inspiration.


## Implemented in Phase 5

- Case-sensitive standard-compatible flags `g`, `t`, `B`, `H`, and `Y`, plus custom controls `br`, `sf`, `vv`, `cr`, `as`, `ng`, `nh`, `hb`, `dy`, `vib`, `vr`, `sb`, and `dc`/`dec`.
- Continuous cents pitch-bend interpolation, legacy 256-source-sample and Q-tempo grids, source-contour modulation, and vibrato integrated into oscillator phase without resetting oscillators.
- Independent spectral-envelope movement, harmonic/noise balance, breathiness, and voice-voltage-inspired partial tilt.
- Note Gain, dynamics contour, Consonant Rate, Attack Speed, spectral brightness, H low-pass, Y noise suppression, and Decay.
- Unknown flag tokens are ignored safely and surfaced as warnings.
- Synthetic regression tests for flag parsing, pitch bend, vibrato, note gain, consonant-rate timing, combined expression changes, output duration, and finite samples.

## Phase 5 approximations and remaining work

- Formant movement is a bounded re-indexing of a cepstral spectral envelope, not a dedicated formant tracker or LPC pole-shift engine.
- Voice Voltage, Dynamics, Breathiness, and Attack Speed are DSP-inspired perceptual controls, not models of vocal-fold physiology. Their scales are ResynthUTAU-defined and are not proprietary SynthV/Piapro parameter scales.
- The current `Q<tempo>` pitch automation grid is a documented interpretation of public UTAU resampler descriptions. Pitch-bend point values in this project are cents; other engines may use different encodings.
- Vibrato uses a fixed sinusoidal shape with a configurable depth and rate, and short fade-in/out; it does not yet provide arbitrary vibrato curves or note-specific delay/phase controls.
- Spectral brightness and low-pass are simple one-pole filters; they are not high-order mastering EQ.
- Actual UTAU/OpenUTAU integration, recorded-bank perceptual tuning, and Windows runtime testing remain outstanding after Phase 6. A baseline and optimized performance comparison is available in `PERFORMANCE_BENCHMARK.md`.


## Phase 6 compatibility notes

- WAVE_FORMAT_EXTENSIBLE PCM and IEEE-float subtype GUIDs are supported for one or two channels.
- The reader accepts the supplied sample's legacy RIFF-size error when the header overstates the file length by exactly eight bytes; larger overruns are rejected.
- RF64, compressed WAV codecs, and more than two channels remain unsupported.
- The optimization is intended to reduce per-frame allocation and window-generation work without changing render output. It is not a claim that all CPU architectures or voicebanks will show the same speed gain.
