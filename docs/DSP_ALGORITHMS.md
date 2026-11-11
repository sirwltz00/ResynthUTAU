# DSP algorithms and current implementation

## Intended destination

The long-term engine is designed around independent periodic (harmonic) and aperiodic components, a time-varying spectral envelope, and controlled reconstruction. This architecture is inspired by publicly described harmonic-plus-noise vocal-synthesis principles; it is not a reverse-engineering claim about Piapro Studio NT.

## Phase 1 implementation

### Input decoding

`WavReader` parses RIFF/WAVE chunks, reads `fmt ` and `data`, validates frame alignment, and converts supported sample formats into normalized floating-point interleaved samples. Current supported encodings are PCM16/24/32 and IEEE float32/64. Mono and stereo are supported. Non-finite floating-point input is replaced with zero.

### Time-region handling

The renderer converts offset, fixed length, end blank, and requested length from milliseconds into frames using the input sample rate. The `offset` removes leading source frames; `end_blank` removes trailing frames. If a required duration is supplied, the output frame count is rounded once from that duration and is exact thereafter.

### Fixed-region velocity

The initial fixed region is rendered to a velocity-adjusted duration using the Phase 1 convention:

`fixed_time_multiplier = 2 ^ ((100 - velocity) / 100)`

The remainder is adjusted to fill the requested note duration. If the requested duration is shorter than the velocity-adjusted fixed section, the renderer reports a warning and truncates the fixed section to fit.

### WSOLA-style waveform time stretching

For sufficiently long regions, the initial renderer uses waveform-similarity overlap-add. It divides the output into overlapping windows, searches near the expected source position, chooses a candidate with high normalized cross-correlation against the preceding overlap, and crossfades the overlap. The search uses a mono correlation estimate while applying the selected time positions identically to all channels, preserving stereo alignment.

For very short segments that do not contain enough samples for a stable correlation search, a linear interpolation fallback is used. It is intentionally limited to those small regions; it is not the pitch-shifting architecture.

### Output

`WavWriter` writes valid little-endian PCM16 RIFF/WAVE data at the input sample rate and channel count. Output samples are checked for finite values and clamped to the representable range.

## Not implemented yet

There is no F0 tracker, FFT/STFT, harmonic partial tracker, spectral-envelope estimator, LPC/cepstral formant model, aperiodicity estimator, harmonic oscillator bank, noise synthesizer, or phase-locked resynthesis in Phase 1. The pitch note is parsed and range-checked but source pitch is retained. These are subsequent project stages, not hidden behind the current renderer.
