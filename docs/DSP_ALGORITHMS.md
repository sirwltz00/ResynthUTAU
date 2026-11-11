# DSP algorithms and implementation status

## Scope

Phase 2 adds an offline, frame-based vocal-analysis path. It does not yet resynthesize audio from the extracted data. The design uses independently implemented, conventional DSP techniques; it does not claim to reproduce Piapro Studio NT's undocumented implementation.

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

## Not implemented yet

- Harmonic waveform reconstruction and continuous oscillator phase tracking.
- Time-domain aperiodic/noise reconstruction.
- Pitch transposition, pitch bends, vibrato, portamento, and phase-locked spectral synthesis.
- Formant tracking/shift controls or Super Formant Shifter-like processing.
- Consonant/vowel segmentation, transient detection, attack/decay modeling, and vowel-sustain extension.
- UTAU note renderer integration of Phase 2 analysis products.
