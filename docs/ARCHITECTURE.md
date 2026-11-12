# Software architecture — version 0.6.0

```text
                              ResynthUTAU
                                   |
             +---------------------+----------------------+
             |                                            |
    Positional render CLI                          Diagnostic CLI
             |                                      --analyze
      ParameterParser                             --resynthesize
             |
       UTAUInterface
             |
        renderPhase5
             |
       +-----+---------------------------+
       |                                 |
  WAV crop / timing                 AnalysisEngine
  fixed initial region              FFT / YIN / descriptors
       |                                 |
  WSOLA waveform path              PhoneticAnalyzer
       |                          voiced / unvoiced /
       |                          transient / silence
       |                                 |
       +----------------+----------------+
                        |
                 voiced confidence
                        |
              +---------+----------+
              |                    |
       Natural pitch path    Expression path
           TD-PSOLA         Harmonic + aperiodic
              |                    |
              +----------+---------+
                         |
                transition / mix
                         |
             expression / gain / guard
                         |
                      WAV output
```

## Module boundaries

- `AudioBuffer`: normalized floating-point interleaved samples and channel metadata.
- `WavReader` / `WavWriter`: RIFF/WAVE decoding and PCM16 output.
- `ParameterParser`: detects the standalone UTAU-style positional profile or OpenUtau Classic's 13-argument profile. The Classic adapter decodes signed 12-bit Base64 pitch points and repeat markers, and maps `!tempo` and signed cutoff fields.
- `UTAUInterface`: file-level entry point from parsed arguments to WAV output.
- `ResynthesisEngine`: Phase 1 comparison renderer, timing-aware `renderPhase5` path, natural TD-PSOLA path, expression synthesis path, and offline analysis-to-resynthesis orchestration. `renderPhase4` remains a source-compatibility wrapper.
- `PitchWorkspace`: per-render reusable pitch-analysis buffers; no shared mutable global DSP workspace.
- `AnalysisEngine`: frame extraction, analysis orchestration, summary metrics, and JSON output.
- `PhoneticAnalyzer`: conservative acoustic classification and smoothed voiced-region confidence; not a phoneme recognizer.
- `FFT`: in-place radix-2 Cooley–Tukey transform and normalized inverse transform.
- `PitchAnalyzer`: FFT-autocorrelation YIN cumulative-mean normalized difference function, parabolic lag refinement, RMS and periodicity confidence.
- `HarmonicAnalyzer`: local spectral peak measurements around expected harmonic frequencies, amplitudes, and centered phase observations.
- `SpectralEnvelope`: cepstral smoothing and log-frequency profile interpolation.
- `NoiseAnalyzer`: spectral-mask aperiodicity estimation.
- `HarmonicSynthesizer`: continuous-phase harmonic oscillator bank with interpolated pitch curves and phase-observation correction.
- `NoiseSynthesizer`: deterministic seeded stochastic aperiodic synthesis shaped by the estimated residual profile.
- `scripts/benchmark.py`: optional fresh-process command-line performance measurement.

## Render-path selection

The conventional positional renderer first handles offset/end trimming and the fixed initial region. It then analyzes the remaining tail and constructs a waveform-preserving WSOLA result.

- With neutral expression settings, voiced regions are pitch-shifted with waveform-derived TD-PSOLA. Unvoiced, transient, silent, and uncertain regions remain on the WSOLA waveform path.
- When controls requiring spectral or oscillator processing are active, voiced regions use the harmonic/aperiodic synthesis path. A smoothed voiced-confidence mask controls the blend back to waveform-preserved material.
- Both paths are followed by note-level processing and a peak guard. The requested duration is enforced before WAV output.

## Phase 6 additions

Phase 6 does not add a new synthesis algorithm. It hardens WAV parsing and reduces repeated analysis allocations:

- `WavReader` validates RIFF container boundaries, chunk payload sizes, padding, format metadata, block alignment, frame alignment, supported subtype GUIDs, and a 512 MiB data-chunk ceiling.
- `PitchWorkspace` reuses the frame buffer, window, FFT spectrum, prefix-energy, YIN difference, and CMND buffers within one render.
- Spectral analysis reuses a complex spectrum buffer and precomputed Hann-window coefficients.
- `--version` and `--capabilities` expose build and interface metadata.
- The benchmark script measures fresh-process render time and real-time factor.

## Compatibility boundary

The executable implements both the standalone UTAU-style positional profile and OpenUtau Classic's external-resampler command-line profile. It does not implement a native OpenUtau renderer API, and no real OpenUtau GUI session has been run. See [UTAU_COMPATIBILITY.md](UTAU_COMPATIBILITY.md) and [OPENUTAU_INTEGRATION.md](OPENUTAU_INTEGRATION.md).
