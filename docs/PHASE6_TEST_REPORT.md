# ResynthUTAU 0.6.0 — Phase 6 test report

## Build and automated tests

The checked source archive was configured and built locally with GCC 14.2.0 in Release mode. CTest reported all three registered tests passing:

| CTest entry | Result |
|---|---|
| `resynth_tests` | Passed — 26 test functions |
| `cli_version` | Passed |
| `cli_capabilities` | Passed |

The documentation refresh independently rebuilt and reran the suite with both GCC 14.2.0 Release and Clang 17 Release; both passed all three CTest entries. An optimized AddressSanitizer/UndefinedBehaviorSanitizer build also compiled, but its test run exceeded the execution time limit during this refresh, so this refresh does not claim a new sanitizer pass. An earlier Phase 6 validation log recorded a passing optimized sanitizer run with leak detection disabled.

## Phase 6 test coverage

- WAVE_FORMAT_EXTENSIBLE PCM24, including `data` before `fmt ` and odd-sized chunk padding.
- WAVE_FORMAT_EXTENSIBLE IEEE float32.
- Standard PCM32 and IEEE float64 decoding.
- Tolerance of the narrowly defined eight-byte RIFF-size overstatement.
- Rejection of malformed RIFF container sizes.
- Reusable YIN pitch workspace equivalence with the stateless API across multiple frame centers.
- UTF-8 path handling and automatic output parent-directory creation.
- `--version` and `--capabilities` metadata endpoints.

The full test executable includes 26 test functions covering buffer and WAV I/O, positional parsing, WSOLA, FFT/YIN analysis, harmonic/noise analysis, Phase 3 synthesis, Phase 4 timing and phonetic handling, Phase 5 controls, the conventional file-level render path, and Phase 6 WAV/workspace behavior.

## Supplied real-sample comparison

A short user-supplied sample (`ka-ka-ki-ka-ku-ka-N-ka.wav`, mono, 44.1 kHz, PCM16, approximately 3.11147 seconds) was used during the Phase 6 optimization comparison. It is not included in this source archive.

- Neutral Phase 6 output was sample-identical to the input.
- Phase 6 output was sample-identical to Phase 5.1 for the tested +1, +3, +5, -1, and -5 semitone cases.
- For those comparisons, maximum absolute difference and RMS difference were 0, with correlation 1.0.

This confirms output equivalence for the tested file and settings; it does not establish broad perceptual quality.

## Performance comparison

Same sample, machine, GCC Release configuration, four-argument positional invocation, and five fresh-process runs per build:

| Build | Median render time | Throughput |
|---|---:|---:|
| Phase 5.1 | 0.6170 s | 5.04× real time |
| Phase 6 | 0.5041 s | 6.17× real time |

This is an 18.3% reduction in median elapsed time on this machine and sample. It is not a universal speed guarantee. Peak RSS from one `/usr/bin/time` run was 7,416 KiB for Phase 5.1 and 7,540 KiB for Phase 6; this does not demonstrate a memory reduction.

## Environment recorded for the Phase 6 validation

- Linux x86_64, kernel 6.18.57
- Intel Xeon Platinum 8573C
- GCC 14.2.0 and Clang 17
- CMake Release and optimized sanitizer configurations

## Not verified

- Launching the executable from UTAU on Windows.
- Launching the executable from OpenUtau Classic on Windows or Linux.
- OpenUtau's Base64/run-length encoded 12-bit pitch curve and signed cutoff argument handling; these are not implemented in this source snapshot.
- OpenUtau's native renderer API; not implemented.
- Windows runtime behavior on an actual Windows machine.
- macOS or mobile builds.
- Broad perceptual testing across real voicebanks, languages, and phonation styles.

The Windows/Linux GitHub Actions workflows are included, but no remote Actions run or GitHub Release was performed as part of this documentation refresh.
