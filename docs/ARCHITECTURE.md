# Software architecture — Phase 1

```text
UTAU / OpenUTAU Classic
        |
        | classic positional command line
        v
src/main.cpp
        |
        v
ParameterParser  ---> ResamplerParameters
        |
        v
UTAUInterface
        |              WavReader
        |                  |
        |                  v
        |              AudioBuffer (float, interleaved)
        |                  |
        v                  v
             ResynthesisEngine
             - trim source region
             - split fixed region / tail
             - WSOLA-style duration adjustment
             - volume / finite-value clamp
                      |
                      v
                  WavWriter
                      |
                      v
                output PCM16 WAV
```

## Module boundaries

- `AudioBuffer`: normalized floating-point interleaved samples, sample rate, and channel metadata.
- `WavReader`: RIFF/WAVE chunk parsing and conversion into `AudioBuffer`.
- `WavWriter`: PCM16 RIFF/WAVE serialization.
- `ParameterParser`: validation of the classic positional arguments and target note name.
- `UTAUInterface`: host-facing orchestration, path creation, WAV I/O, and render report.
- `ResynthesisEngine`: Phase 1 timing and waveform-domain duration processing.
- `main.cpp`: platform entry point; Windows uses a wide-character entry point and UTF-8 conversion.

The DSP layer is independent of the UTAU command-line layer, so later analysis/synthesis modules can be introduced without changing the host-facing invocation contract. No module is created as an empty placeholder.

## Planned architecture extensions

Subsequent phases can add independent analysis products (F0 tracks, harmonic tracks, spectral envelopes, aperiodicity maps, transient markers) and synthesis processors. The current `ResynthesisEngine` can then coordinate the harmonic and aperiodic render paths while retaining the same WAV and UTAU interface modules.
