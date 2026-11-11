# Software architecture — Phase 3

```text
                              ResynthUTAU
                                   |
             +---------------------+---------------------+
             |                                           |
      UTAU render path                            Offline path
             |                                           |
    ParameterParser                         WAV reader -> AudioBuffer
             |                                           |
      UTAUInterface                                AnalysisEngine
             |                                           |
    Phase 1 timing renderer                    +----------+----------+
             |                                 |                     |
      WAV writer                         YIN / FFT analysis     spectral features
                                                   |                     |
                                           harmonic partials      envelope / residual
                                                   |                     |
                                           HarmonicSynthesizer   NoiseSynthesizer
                                                   |                     |
                                                   +----------+----------+
                                                              |
                                                   component merge / peak guard
                                                              |
                                                         WAV writer

       --analyze selects analysis/reporting; --resynthesize selects offline synthesis.
```

## Module boundaries

- `AudioBuffer`: normalized floating-point interleaved samples and channel metadata.
- `WavReader` / `WavWriter`: RIFF/WAVE I/O.
- `ParameterParser` / `UTAUInterface`: classic UTAU positional command-line contract and Phase 1 timing renderer.
- `ResynthesisEngine`: Phase 1 `renderPhase1` plus Phase 3 offline `resynthesize` orchestration and component merging.
- `FFT`: in-place radix-2 Cooley–Tukey transform and normalized inverse transform.
- `PitchAnalyzer`: FFT-autocorrelation YIN cumulative-mean normalized difference function, parabolic lag refinement, RMS and periodicity confidence.
- `HarmonicAnalyzer`: local spectral peak measurements around integer multiples of tracked F0, including amplitude and phase estimates.
- `SpectralEnvelope`: real-cepstral liftering of the log-magnitude spectrum.
- `NoiseAnalyzer`: soft harmonic-bin mask and residual spectral energy/magnitude estimates.
- `AnalysisEngine`: adaptive frame/window scheduling, spectral metrics, compact log-frequency profiles, summary aggregation, and JSON serialization.
- `HarmonicSynthesizer`: sample-wise oscillator bank, amplitude/F0 interpolation, and bounded phase-observation corrections.
- `NoiseSynthesizer`: seeded random-phase spectral blocks, inverse FFT, and window-power-normalized overlap-add.
- `main.cpp`: classic UTAU invocation, `--analyze`, and `--resynthesize` diagnostic command modes.

## Phase 2/3 handoff

Each `AnalysisFrame` carries its center sample, window size, RMS, F0, periodicity, voiced/unvoiced label, harmonic partials, harmonic/aperiodic energy, aperiodicity ratio, spectral centroid/flatness, and compact 256-band spectral-envelope and aperiodicity profiles. Phase 3 consumes this `AnalysisResult` directly. It does not need to reopen or re-read the original WAV after analysis.

## Current integration boundary

The analysis-to-resynthesis path is offline and separate from the traditional UTAU positional renderer. The latter continues to call `renderPhase1` to preserve existing CLI behavior. Phase 4 is responsible for connecting host timing parameters and phonetic regions to the new synthesis engine without sacrificing consonant attacks or vowel timing.
