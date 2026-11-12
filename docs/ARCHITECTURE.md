# Software architecture — Phase 5

```text
                              ResynthUTAU
                                   |
             +---------------------+---------------------+
             |                                           |
      UTAU render path                            Diagnostic paths
             |                                           |
    ParameterParser                       --analyze / --resynthesize
             |
      UTAUInterface
             |
        renderPhase5
             |
      +------+----------------------+
      |                             |
  WAV timing crop               AnalysisEngine
  fixed region                  FFT / YIN / spectral descriptors
      |                             |
  WSOLA waveform path          PhoneticAnalyzer
      |                       voiced / unvoiced / transient / silence
      |                             |
      +----------------+------------+
                       |
               voicing-confidence mask
                       |
          time-mapped ResynthesisEngine
                  /           \
        HarmonicSynthesizer  NoiseSynthesizer
                  \           /
                   component mix
                       |
             dry/resynth transition blend
                       |
             volume / peak guard / WAV
```

## Module boundaries

- `AudioBuffer`: normalized floating-point interleaved samples and channel metadata.
- `WavReader` / `WavWriter`: RIFF/WAVE I/O.
- `ParameterParser` / `UTAUInterface`: classic UTAU positional command-line contract and file-level renderer entry point.
- `ResynthesisEngine`: Phase 1 comparison renderer, Phase 4 compatibility renderer, Phase 5 expressive UTAU renderer, and offline analysis-to-resynthesis orchestration.
- `PhoneticAnalyzer`: conservative acoustic classification and smoothed voiced-region confidence. It does not perform language-aware phoneme recognition.
- `FFT`: in-place radix-2 Cooley–Tukey transform and normalized inverse transform.
- `PitchAnalyzer`: FFT-autocorrelation YIN cumulative-mean normalized difference function, parabolic lag refinement, RMS and periodicity confidence.
- `HarmonicAnalyzer`: local spectral peak measurements around integer multiples of tracked F0, including amplitude and phase estimates.
- `SpectralEnvelope`: real-cepstral liftering of the log-magnitude spectrum.
- `NoiseAnalyzer`: soft harmonic-bin mask and residual spectral energy/magnitude estimates.
- `AnalysisEngine`: adaptive frame/window scheduling, spectral metrics, compact log-frequency profiles, summary aggregation, and JSON serialization.
- `HarmonicSynthesizer`: sample-wise oscillator bank, amplitude/F0 interpolation, bounded phase-observation corrections, and duration-aware sample mapping.
- `NoiseSynthesizer`: seeded random-phase spectral blocks, inverse FFT, window-power-normalized overlap-add, and duration-aware frame placement.
- `main.cpp`: classic UTAU invocation, `--analyze`, and `--resynthesize` diagnostic command modes.

## Phase 5 expression processing flow

The UTAU path first trims offset and end blank. It then separates the initial fixed region from the tail. The fixed region is preserved or WSOLA-adjusted according to velocity and Consonant Rate. The tail is analyzed and one waveform-preserving WSOLA fallback is created. Stable voiced frames are reconstructed with waveform-derived TD-PSOLA using fractional pitch marks; unvoiced/transient/silent frames remain on the WSOLA path. A single linear transition blends correlated waveform paths at phonetic boundaries. Pitch-contour retention, pitch bends, vibrato, and fine tuning use the expressive harmonic path when requested. The natural neutral path does not mix an independently phased oscillator into the source vowel. Formant shifting on the expressive path reindexes the cepstral envelope independently from F0. The joined result receives dynamics, brightness, filtering, decay, note gain, and a final peak guard.

## Time-mapped resynthesis

The synthesis frame trajectories remain in source-time coordinates. For each output sample, the synthesizer maps to a source analysis position using `source_position = output_position / time_stretch_ratio`. Harmonic phase is integrated at the target F0 in output time, while measured phase deviations are distributed across the stretched frame interval. This allows the vowel's duration to change without scaling the oscillator frequency by the inverse stretch ratio. Noise analysis frames are placed at correspondingly time-mapped centers and the synthesis window is adjusted to maintain overlap.

## Important boundaries

- The classifier uses acoustic heuristics, not `oto.ini` or explicit phoneme labels.
- The fixed region is inferred from the host's fixed-length argument; the resampler does not inspect the source OTO.
- Stereo analysis is downmixed; the resynthesized mono voiced path is blended into each original channel's waveform path.
- Phase 5 expression controls are applied in the conventional positional rendering path. `renderPhase4` remains as a source-compatibility wrapper around `renderPhase5`.
- `--resynthesize` remains a diagnostic path; the conventional positional invocation calls the full Phase 5 timing, phonetics, and expression pipeline.
