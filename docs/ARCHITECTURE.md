# Software architecture — Phase 2

```text
                         ResynthUTAU
                              |
              +---------------+----------------+
              |                                |
       UTAU render path                   Analysis path
              |                                |
      ParameterParser                  WAV reader -> AudioBuffer
              |                                |
         UTAUInterface                  Frame scheduler / mono fold-down
              |                                |
      ResynthesisEngine                  +-----+-------------------+
      (Phase 1 timing)                    |                         |
              |                      FFT autocorrelation       Windowed FFT
       WAV reader/writer                    |                         |
              |                         YIN F0 track          +------+------+
         output WAV                          |                 |             |
                                      Voiced/unvoiced      Harmonic      Cepstral
                                           labels          partials      envelope
                                                              |             |
                                                              +------+------+
                                                                     |
                                                           Aperiodic residual
                                                                     |
                                                      256-band log-frequency profiles
                                                                     |
                                                         summary / JSON report
```

## Module boundaries

- `AudioBuffer`: normalized floating-point interleaved samples and channel metadata.
- `WavReader` / `WavWriter`: RIFF/WAVE I/O.
- `ParameterParser` / `UTAUInterface`: classic UTAU command-line contract and I/O orchestration.
- `ResynthesisEngine`: Phase 1 waveform-domain timing renderer. It is still the UTAU invocation's render path in Phase 2.
- `FFT`: in-place radix-2 Cooley–Tukey transform and inverse transform.
- `PitchAnalyzer`: FFT-autocorrelation YIN cumulative-mean normalized difference function, parabolic lag refinement, RMS and periodicity confidence.
- `HarmonicAnalyzer`: local spectral peak measurements around integer multiples of the tracked F0, including amplitude and phase estimates.
- `SpectralEnvelope`: real-cepstral liftering of the log-magnitude spectrum.
- `NoiseAnalyzer`: soft harmonic-bin mask and remaining spectral energy as an aperiodicity estimate.
- `AnalysisEngine`: adaptive frame/window scheduling, spectral metrics, compact log-frequency feature profiles, summary aggregation, and JSON serialization.
- `main.cpp`: UTAU renderer entry point plus the independent `--analyze` diagnostic mode.

## Phase 2 data model

Each `AnalysisFrame` records its sample center and spectral window size, RMS, F0, YIN periodicity/confidence, voiced/unvoiced label, harmonic partials, harmonic and aperiodic energy, aperiodicity ratio, spectral centroid, spectral flatness, and two 256-band log-frequency profiles. The profile representation avoids storing two full-resolution spectra for every frame and is intended to be a compact handoff format for later synthesis work.

## Separation of concerns

The `--analyze` mode is deliberately independent from UTAU note rendering. It reads a WAV, runs the analysis pipeline, and optionally writes a detailed JSON report. Phase 2 does not yet feed its estimates into the output renderer. Phase 3 will connect analyzed features to a continuous-phase harmonic synthesizer and a noise/aperiodic synthesizer.
