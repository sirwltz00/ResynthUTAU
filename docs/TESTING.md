# Testing — Phase 1

The test executable uses no external test framework. CTest runs the following checks:

1. AudioBuffer validity, frame count, and interleaved channel access.
2. PCM16 WAV writer/reader round-trip, sample rate, stereo layout, duration, and quantization tolerance.
3. UTAU positional argument parsing, including note-to-MIDI validation and timing/volume fields.
4. Phase 1 rendering with offset, end blank, fixed region, requested note length, stereo alignment, finite sample values, and volume attenuation.

## Numerical targets currently covered

- Required-duration error: zero frames after conversion to output frame count.
- PCM16 round-trip sample error: less than `0.0001` for the test signal.
- Output: no NaN/Inf values in the tested render path.
- Peak: samples are clamped to `[-1, 1]` before PCM16 writing.
- Stereo: channel count and frame alignment are preserved.

## Not yet measured

Pitch accuracy, pitch-bend continuity, formant stability, harmonic/noise isolation, consonant intelligibility, transient preservation, processing latency, peak memory, and objective perceptual quality are not Phase 1 test claims. Dedicated analysis and synthesis tests must be added when those features exist.

Run locally with:

```bash
ctest --test-dir build -C Release --output-on-failure
```
