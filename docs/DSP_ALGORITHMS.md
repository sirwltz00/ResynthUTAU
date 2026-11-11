# DSP algorithms and implementation status — Phase 3

## Scope

Phases 2 and 3 provide an offline, frame-based vocal-analysis and resynthesis path. Phase 2 extracts F0, harmonic, envelope, and aperiodicity features; Phase 3 reconstructs a new waveform from those features. The design uses independently implemented, conventional DSP techniques and does not claim to reproduce Piapro Studio NT's undocumented implementation.

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

Each frame also stores spectral centroid and spectral flatness, computed from the one-sided power spectrum. These descriptors can help later distinguish bright/noisy regions from strongly periodic vowel-like regions, but Phase 2 does not use them to make phoneme labels.

## JSON report

Run:

```text
ResynthUTAU --analyze input.wav [report.json]
```

Without a report path, the program prints a concise summary. With a report path, it writes JSON containing the summary plus per-frame measurements, harmonic partials, and the 256-band envelope and aperiodicity profiles. Reports can be large for long recordings because the per-frame feature arrays are included.

## Phase 3 harmonic resynthesis

The harmonic synthesizer allocates one oscillator state per observed partial order. At each output sample it interpolates the frame F0 and partial amplitudes, advances phase from the instantaneous F0, and sums the active partials. Oscillators continue advancing even while their amplitude is zero or their frequency is above Nyquist, preventing a silent interval from pausing and later restarting phase.

For each adjacent pair of frames with a measured partial in both frames, the synthesizer compares the wrapped observed phase difference with the phase advance predicted by the trapezoidal average of the source F0. The shortest wrapped residual is multiplied by a configurable strength and distributed across the sample interval. The correction is additive to the continuously integrated target-pitch oscillator; it never resets oscillator phase at a frame boundary. At strength 1.0 and pitch ratio 1.0, this causes the oscillator to meet the measured phase observations modulo 2π, subject to the accuracy of the analyzer's local FFT phase estimates.

The target frequency of partial `h` is `h * F0 * pitch_ratio`. Partial amplitude is linearly interpolated between analysis frames. Frequencies at or above Nyquist are omitted from the sum but oscillator phase still advances. The first valid phase observation for each partial initializes its phase trajectory by integrating the F0 track back to sample zero.

## Phase 3 spectral-envelope compensation

When pitch ratio differs from 1.0 and envelope preservation is enabled, each partial is rescaled by the ratio of the smoothed spectral envelope at its target frequency to the envelope at its source frequency. The ratio is bounded by `maximum_envelope_compensation` to avoid extreme amplification in low-energy profile regions. At pitch ratio 1.0 no compensation is applied. This is a practical interpolation over the Phase 2 cepstral envelope profile, not a high-resolution formant tracker; it may blur narrow resonances or alter timbre when the profile is inaccurate.

## Phase 3 aperiodic/noise synthesis

For each analysis frame, the noise synthesizer samples the stored log-frequency aperiodic profile onto the positive bins of a configurable power-of-two FFT. A deterministic pseudo-random phase is assigned to each positive-frequency bin and the conjugate mirror is filled to produce a real inverse-FFT block. DC and Nyquist are zeroed. The block is normalized to the estimated aperiodic RMS, approximated from `frame.rms * sqrt(aperiodicity_ratio)`, then placed at the analysis frame center with a Hann window.

Overlapping blocks are combined using the square root of the accumulated window-power weights. This makes the expected noise power approximately stationary in overlap regions while allowing the profile and target energy to evolve between analysis frames. A configurable seed makes the generated result repeatable for the same build and inputs. The synthesis is stochastic and shaped by the estimated residual; it does not recover the exact original noise waveform or its phase.

## Phase 3 component merge and output guard

The harmonic and aperiodic paths are independently gain-controlled and summed sample by sample. A short raised-cosine edge fade reduces hard truncation clicks. A global offline peak guard applies one constant gain to the whole output when the configured ceiling is exceeded. This avoids clipping without adding sample-by-sample limiter distortion, but may reduce the overall level of a transient-heavy signal. Metrics report component RMS, mixed RMS, pre-guard peak, applied gain, and whether the guard engaged.

The current diagnostic renderer writes the analyzed duration at the source sample rate. Phase 2 averages source channels before analysis; mono is therefore the default output, and requested stereo output duplicates the mono reconstruction.

## Current gaps after Phase 3

- UTAU note timing, fixed-consonant handling, and the conventional CLI are not yet routed through Phase 3 resynthesis.
- Pitch bends, vibrato, portamento, and time-varying expression curves are not implemented.
- No phoneme segmentation, consonant-specific waveform preservation, transient detector, or vowel-sustain loop model exists yet.
- The spectral aperiodicity estimate is not exact harmonic/noise waveform decomposition; the stochastic noise path is an approximation.
- Formant compensation uses a compact cepstral envelope and is not a dedicated formant tracker.
- Full perceptual evaluation on varied real voicebanks has not been performed.
- No exact Piapro NT internals are claimed or reproduced.
