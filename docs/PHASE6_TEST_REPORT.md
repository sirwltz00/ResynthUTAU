# ResynthUTAU 0.6.0 — Phase 6 test report

## Build and automated tests

| Build | Result |
|---|---|
| GCC 14.2.0 Release | Passed — 3/3 CTest entries |
| Clang 17 Release | Passed — 3/3 CTest entries |
| GCC AddressSanitizer + UndefinedBehaviorSanitizer, optimized `-O1` | Passed — 3/3 CTest entries; leak detection disabled |

The main `resynth_tests` executable includes 26 test functions spanning the Phase 1–6 regression areas. The other CTest entries exercise `--version` and `--capabilities`.

Phase 6 coverage includes WAVE_FORMAT_EXTENSIBLE PCM24/float32, PCM32, IEEE float64, data-before-fmt chunk ordering, odd chunk padding, the narrowly tolerated eight-byte RIFF-size overstatement, malformed RIFF rejection, reusable YIN workspace equivalence, Unicode input/output paths, output parent-directory creation, and CLI metadata.

## Real-sample validation

Input: the user-provided `ka-ka-ki-ka-ku-ka-N-ka.wav` (mono, 44.1 kHz, PCM16, 3.11147 seconds). Its RIFF size field was eight bytes larger than the spec-correct value; Phase 6 accepted this known legacy error while still rejecting a larger overrun in a dedicated test.

Neutral output was sample-identical to the input. The Phase 5.1 and Phase 6 render outputs were also sample-identical for +1, +3, +5, -1, -3, and -5 semitone ratios. All six comparisons had maximum absolute difference 0, RMS difference 0, and correlation 1.0.

## Performance comparison

Same sample, same machine, same GCC Release configuration, same four-argument UTAU invocation, five fresh-process runs per build:

| Build | Median render time | Throughput |
|---|---:|---:|
| Phase 5.1 | 0.6170 s | 5.04× real time |
| Phase 6 | 0.5041 s | 6.17× real time |

This is an 18.3% reduction in median elapsed time on this test machine. It is not a universal speed guarantee. Peak RSS from one `/usr/bin/time` run was 7,416 KiB for Phase 5.1 and 7,540 KiB for Phase 6; the measurement does not establish a memory reduction.

## Environment

- Linux 6.18.57 x86_64
- Intel Xeon Platinum 8573C
- GCC 14.2.0 / Clang 17
- CMake Release and optimized sanitizer configurations

## Not verified here

- Launching the binary from UTAU on Windows.
- Launching it from OpenUTAU Classic.
- OpenUTAU's native renderer API (not implemented).
- Windows runtime behavior, including Unicode paths, on an actual Windows machine.
- Broad perceptual testing across real voicebanks and phonation styles.
- macOS.

The Windows/Linux GitHub Actions workflows are included, but no remote Actions run or GitHub Release was performed from this local build.
