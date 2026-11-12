# DSP algorithms and implementation status — ResynthUTAU 0.6.0

## Scope

Phases 2 and 3 provide frame-based vocal analysis and a separate harmonic/aperiodic analysis-to-resynthesis path. Phase 4 connected analysis to the positional timing path. Phase 5 added expression controls, and Phase 5.1 made waveform-derived TD-PSOLA the default voiced pitch-shifting path. Phase 6 focuses on WAV robustness and reusable analysis memory. The design uses independently implemented, conventional DSP techniques and does not claim to reproduce Piapro Studio NT's undocumented implementation.

## Framing and adaptive windows

The default analysis hop is 256 samples. F0 is estimated with a 4096-sample window. After the F0 estimate, the spectral window is selected to target approximately eight fundamental cycles:

`requested_window = sample_rate * target_cycles_per_window / F0`

The request is rounded to a power of two and clamped to the configured 1024–4096 sample range. Unvoiced frames use a 2048-sample spectral window by default. Both the pitch and spectral windows use centered frames with zero padding at the beginning and end of the source. A Hann window is applied before spectral analysis.

The analysis path downmixes stereo input to mono by averaging channels. It does not modify the source `AudioBuffer` or the UTAU render path.

## FFT

`FFT.cpp` implements an in-place radix-2 Cooley–Tukey transform with bit-reversal permutation. The inverse transform is normalized by the transform length. The implementation uses `std::complex<double>` internally to keep analysis arithmetic stable. It has no external FFT dependency.

## Fundamental-frequency estimation (YIN)

The pitch analyzer computes the autocorrelation of a windowed frame from the inverse FFT of the squared-magnitude spectrum. It then derives the YIN difference function from prefix sums of squared samples and the autocorrelation:

`d(tau) = E_left(tau) + E_right(tau) - 2 * autocorrelation(tau)`

The cumulative-mean normalized difference function (CMNDF) is:

`CMNDF(tau) = d(tau) * tau / sum(d(j), j=1..tau)`

The first trough below the configured threshold is selected, with a global-minimum fallback for weaker but still periodic frames. A three-point parabolic interpolation refines the lag. F0 is `sample_rate / refined_lag`. Periodicity confidence is `1 - CMNDF`; frames below the RMS or confidence threshold are marked unvoiced and receive F0 = 0.

Default F0 bounds are 65–1000 Hz, the YIN trough threshold is 0.18, and the minimum periodicity confidence is 0.65. These are initial general-purpose defaults, not a guarantee for every voicebank, register, or phonetic context. The configuration is exposed through `AnalysisConfig` for future tuning.

## Harmonic partial analysis

For voiced frames, the analyzer searches around each expected harmonic `h * F0` for the strongest local FFT bin. The search radius is limited relative to the fundamental spacing to avoid crossing into the neighboring partial. A log-magnitude parabolic estimate refines the peak amplitude. The stored partial contains harmonic order, expected harmonic frequency, measured amplitude, and the complex-spectrum phase at the selected peak.

Amplitude is normalized by the Hann coherent sum. Phase is measured at the selected FFT bin; it is not yet unwrapped or tracked continuously between frames. The analysis therefore supplies local phase observations, not a phase-continuous oscillator trajectory.

## Spectral-envelope estimation

The envelope is estimated using real-cepstral liftering:

1. Take the log magnitude of the full complex spectrum.
2. Inverse FFT to obtain the real cepstrum.
3. Retain low-quefrency coefficients and remove higher-quefrency coefficients that primarily encode fine harmonic spacing.
4. Forward FFT and exponentiate the real part to recover a smooth magnitude envelope.

The default maximum retained quefrency is 1.5 ms. The full positive-frequency envelope is sampled into 256 logarithmically spaced bands for compact storage. This is a spectral-envelope approximation; it is not an LPC pole/formant tracker and does not yet perform formant shifting.

## Aperiodicity and noise estimate

The noise analyzer constructs a soft tapered mask around integer multiples of F0. Spectral power near expected harmonic partials contributes to the harmonic estimate; the remaining power contributes to the aperiodic estimate. The aperiodicity ratio is:

`aperiodic_energy / (harmonic_energy + aperiodic_energy)`

Unvoiced frames with measurable energy use no harmonic mask and are treated as entirely aperiodic in this spectral representation. Silent frames have no measured aperiodic energy and report an aperiodicity ratio of zero. The positive-frequency residual magnitude is sampled into 256 logarithmic bands.

This is a spectral-domain mask/residual estimate. It is **not** a perfect time-domain harmonic/noise separation: window leakage, vibrato, inharmonicity, consonant transients, and overlapping sources can affect the split. There is no inverse-STFT residual waveform, noise-phase model, or independent noise synthesizer in Phase 2.

## Spectral descriptors

Each frame also stores spectral centroid and spectral flatness, computed from the one-sided power spectrum. These descriptors are used by the conservative acoustic-region classifier alongside YIN periodicity and the aperiodicity ratio. They are not language-aware phoneme labels.

## JSON report

Run:

```text
ResynthUTAU --analyze input.wav [report.json]
```

Without a report path, the program prints a concise summary. With a report path, it writes JSON containing the summary plus per-frame measurements, harmonic partials, and the 256-band envelope and aperiodicity profiles. Reports can be large for long recordings because the per-frame feature arrays are included.

## Phase 3 harmonic resynthesis

The harmonic synthesizer allocates one oscillator state per observed partial order. At each output sample it interpolates the frame F0 and partial amplitudes, advances phase from the instantaneous F0, and sums the active partials. Oscillators continue advancing even while their amplitude is zero or their frequency is above Nyquist, preventing a silent interval from pausing and later restarting phase.

For each adjacent pair of frames with a measured partial in both frames, the synthesizer compares the wrapped observed phase difference with the phase advance predicted by the trapezoidal average of the source F0. The shortest wrapped residual is multiplied by a configurable strength and distributed across the sample interval. The correction is additive to the continuously integrated target-pitch oscillator; it never resets oscillator phase at a frame boundary. The default strength is 1.0: full phase following avoids leaving a systematic fraction of the measured phase error uncorrected. This assumes that the local FFT phase observations are reliable; noisy or weak partial observations remain an area for further confidence weighting and smoothing.

The target frequency of partial `h` is `h * F0 * pitch_ratio`. Partial amplitude is linearly interpolated between analysis frames. Frequencies at or above Nyquist are omitted from the sum but oscillator phase still advances. The first valid phase observation for each partial initializes its phase trajectory by integrating the F0 track back to sample zero.

## Phase 3 spectral-envelope compensation

When pitch ratio differs from 1.0 and envelope preservation is enabled, each partial is rescaled by the ratio of the smoothed spectral envelope at its target frequency to the envelope at its source frequency. The ratio is bounded by `maximum_envelope_compensation` to avoid extreme amplification in low-energy profile regions. At pitch ratio 1.0 no compensation is applied. This is a practical interpolation over the Phase 2 cepstral envelope profile, not a high-resolution formant tracker; it may blur narrow resonances or alter timbre when the profile is inaccurate.

## Phase 3 aperiodic/noise synthesis

For each analysis frame, the noise synthesizer samples the stored log-frequency aperiodic profile onto the positive bins of a configurable power-of-two FFT. A deterministic pseudo-random phase is assigned to each positive-frequency bin and the conjugate mirror is filled to produce a real inverse-FFT block. DC and Nyquist are zeroed. The block is normalized to the estimated aperiodic RMS, approximated from `frame.rms * sqrt(aperiodicity_ratio)`, then placed at the analysis frame center with a Hann window.

Overlapping blocks are combined using the square root of the accumulated window-power weights. This makes the expected noise power approximately stationary in overlap regions while allowing the profile and target energy to evolve between analysis frames. A configurable seed makes the generated result repeatable for the same build and inputs. The synthesis is stochastic and shaped by the estimated residual; it does not recover the exact original noise waveform or its phase.

## Phase 3 component merge and output guard

The harmonic and aperiodic paths are independently gain-controlled and summed sample by sample. A short raised-cosine edge fade reduces hard truncation clicks. A global offline peak guard applies one constant gain to the whole output when the configured ceiling is exceeded. This avoids clipping without adding sample-by-sample limiter distortion, but may reduce the overall level of a transient-heavy signal. Metrics report component RMS, mixed RMS, pre-guard peak, applied gain, and whether the guard engaged.

The diagnostic renderer can now write a time-mapped duration at the source sample rate. Phase 2 averages source channels before analysis; the conventional Phase 4 renderer blends mono reconstructed voiced content into each channel of the waveform-preserved signal.

## Phase 4 timing and phonetic integration (current renderer)

The conventional UTAU entry point calls `renderPhase5` (`renderPhase4` remains a compatibility wrapper). Offset and end-blank values are converted to sample positions before processing. The initial fixed region is separated from the tail and remains waveform material; at velocity 100 its samples pass through unchanged. If velocity changes its duration, that region is handled by the existing WSOLA implementation. The remaining tail is rendered to the exact requested frame count.

`PhoneticAnalyzer` classifies each analysis frame as silence, voiced, unvoiced, or transient. It requires more than a positive YIN result: a frame also needs sufficiently low aperiodicity and spectral flatness to be treated as locally periodic. Sharp RMS rises accompanied by high spectral flatness/centroid are marked transient. The voiced confidence is smoothed over a short triangular window; unvoiced and transient frames are held at zero so they remain on the waveform-preserved path. This is a conservative acoustic heuristic, not an actual phoneme recognizer.

The tail has one waveform-preserving fallback produced by WSOLA. Unvoiced, transient, and silent regions stay on that path. Stable voiced regions use time-domain pitch-synchronous overlap-add (TD-PSOLA): pitch marks are detected on a consistent waveform polarity, refined to sub-sample positions with a parabolic peak fit, and mapped to synthesis marks at the requested pitch period. The source position advances according to the separate duration-stretch map, so pitch and note length are independent. A smoothed voicing-confidence envelope makes a single complementary linear transition between the waveform fallback and the PSOLA reconstruction. The same WSOLA fallback is reused; it is not rendered a second time or mixed twice. This avoids adding independently phased oscillator harmonics to the recorded vowel and avoids equal-power gain buildup between correlated waveform paths. A short ramp softens the fixed-region join.

### Natural waveform-derived voiced resynthesis

The default neutral pitch path uses TD-PSOLA. Pitch marks are estimated from the dominant waveform polarity and then refined between samples using a three-point parabolic peak fit. The synthesis-mark spacing is `source_period / pitch_ratio`; duration stretch affects only the source-position mapping (`source_position = output_position / stretch_ratio`). This keeps F0 stable when a note is lengthened. Each source grain is centered on a source pitch mark, tapered with a raised-cosine window, and overlap-added with per-sample window normalization. Fractional source positions are sampled with linear interpolation. The WSOLA output is retained for unvoiced/transient material and blended once at voiced transitions using a linear complementary mix because the two signals are correlated.

Neutral same-duration processing returns the waveform unchanged (apart from file-format quantization at the WAV boundary). This is an explicit transparency invariant, not a promise that every pitch-shifted output is lossless.

### Time-mapped oscillator synthesis (expression path)

For an output duration ratio `r = output_frames / input_frames`, output sample `n` queries its analysis features at source position `n / r`. Harmonic oscillators still advance at the requested target frequency in output time. Measured phase deviations are distributed over the stretched analysis-frame interval. This decouples oscillator frequency from output duration: a sustained vowel can be longer without its F0 falling as it would under naive playback-rate modification. The aperiodic blocks are placed at frame centers scaled by `r`; their FFT support is scaled as well, within a bounded size, to maintain overlap during longer and shorter renders.

### Remaining gaps in the timing and natural synthesis paths

- The region classifier is not language-aware and cannot identify phoneme names or use `oto.ini` boundaries beyond the fixed-length value provided by the host.
- The transient detector can miss subtle stops or overprotect sharp vowel attacks.
- WSOLA can smear very short consonants at extreme duration changes.
- Pitch bends and vibrato are now expressed as interpolated cents curves. The source phase-observation residual is computed from the untouched analysis, while expressive cents values are applied to oscillator frequency during sample-wise phase integration. This prevents control points from resetting oscillator phase and avoids using altered F0 values to estimate source phase residuals.
- Formant compensation uses a compact cepstral envelope and is not a dedicated formant tracker.
- The stochastic aperiodic path does not recreate the exact original residual waveform.
- Full perceptual evaluation on varied real voicebanks and tests inside UTAU/OpenUTAU have not been performed.
- No exact Piapro NT internals are claimed or reproduced.


## Phase 5 expression processing

The renderer converts modulation, fine pitch, pitch-bend points, and vibrato to a cents curve indexed by the original analysis frames. The curve is passed independently to the harmonic synthesizer. The synthesizer uses the original F0 and measured phase observations for phase-residual estimation, then interpolates the cents curve per output sample and multiplies the oscillator frequency by `2^(cents/1200)`. This separates expressive pitch movement from source phase measurement.

Formant movement is applied by re-indexing the cepstral spectral-envelope profile independently of F0. `g` contributes a bounded gender/formant offset and `sf` adds an independent semitone shift. The partial amplitudes are compensated against the shifted envelope while the oscillator fundamental remains controlled by the pitch path. This is an envelope-based approximation rather than a dedicated formant tracker.

Harmonic Balance and Noise Level scale their respective components independently. Breathiness increases the aperiodic contribution and original-waveform texture. Voice Voltage scales harmonic excitation and applies a gentle order-dependent partial tilt. Spectral Brightness is a one-pole high-frequency shelf; H is a one-pole low-pass; Y reduces the synthesized aperiodic contribution and retained dry noise. Dynamics uses an attack/release-smoothed RMS contour with bounded gain changes. Attack Speed modifies the first acoustically detected voiced onset, Consonant Rate scales the fixed initial region, and Decay applies a raised-cosine ending fade.


## Phase 5.1 natural voiced refinement

The default voiced pitch path is waveform-derived TD-PSOLA. Candidate pitch marks are detected using a consistent waveform polarity and refined to fractional-sample positions with a parabolic peak fit. The requested pitch ratio sets the synthesis-mark period, while the output/source duration ratio controls source-position mapping independently. This separation is important: stretching a vowel does not itself lower its target pitch. Source grains are tapered and overlap-added with per-sample window normalization. At voiced/unvoiced boundaries, the PSOLA result and its WSOLA waveform fallback are correlated paths, so a complementary linear blend is used instead of an equal-power blend.

For neutral, same-duration processing, the renderer preserves the input samples exactly in the tested path. This transparency invariant is deliberately different from the Phase 3 diagnostic oscillator-bank reconstruction, which is an analysis-to-synthesis path and does not promise bit-exact reconstruction.

## Phase 6 WAV and analysis optimization

The WAV reader walks the RIFF container rather than assuming a fixed 44-byte header. It checks the declared container boundary, each chunk header and payload size, odd-byte chunk padding, the `fmt ` chunk, PCM/float encoding metadata, block alignment, and the final data-frame alignment. It accepts classic PCM16/24/32 and IEEE-float32/64 as well as WAVE_FORMAT_EXTENSIBLE when the subtype GUID identifies standard PCM or IEEE float. It accepts mono and stereo only. A single non-empty data chunk is read; RF64, compressed codecs, more than two channels, and input data chunks above 512 MiB are rejected.

One narrowly defined compatibility exception accepts a RIFF size field exactly eight bytes larger than the actual file size, because this specific legacy-writer error was observed in the supplied test sample. A larger overrun is rejected. This exception does not relax chunk-payload boundary checks.

The analysis path now reuses a `PitchWorkspace` per render. Frame samples, window coefficients, FFT spectrum, prefix-energy, YIN difference, and CMND buffers are reused between frame positions. Spectral window coefficients and their sums are precomputed for the selected window sizes. These changes target allocation and repeated trigonometric overhead; regression tests require the reusable pitch workspace to match the stateless estimator's F0, periodicity, and voiced decision. No global mutable workspace is shared between renders.

Phase 6 also adds `--version`, `--capabilities`, and an optional Python benchmark script. These are diagnostics and performance tools; they do not imply that the executable has been tested inside a host application.

## Host compatibility boundary

The positional parser accepts both the standalone profile and OpenUtau Classic's 13-argument external-resampler profile. The Classic adapter decodes signed 12-bit Base64/run-length pitch data, parses `!<tempo>`, and applies signed cutoff rules. Its musical pitch points are evaluated on the output timeline; the natural TD-PSOLA path varies synthesis-mark spacing from those cents offsets, while expression-controlled harmonic synthesis integrates the same curve into its oscillator contour. This is command-line integration, not a native OpenUtau renderer; real GUI behavior remains unverified.
