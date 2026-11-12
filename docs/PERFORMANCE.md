# Performance and resource behavior — Phase 6

## Reusable analysis memory

The analysis stage owns a `PitchWorkspace` per render. Its frame, Hann window, FFT spectrum, prefix-energy, YIN difference, and CMND buffers are reused for each analysis hop. The workspace is not global or static: simultaneous invocations receive independent buffers and do not share mutable DSP state.

The spectral stage precomputes Hann coefficients and their sums for the power-of-two window sizes used by the selected analysis configuration. A complex spectrum buffer is reused between frames. This removes repeated trigonometric window generation and repeated FFT-buffer allocation from the per-hop loop.

These changes are intended to preserve analysis results. A regression test compares the reusable pitch workspace against the stateless API at multiple frame centers; F0, periodicity, and voiced decisions must match.

## Recorded Phase 5.1 vs Phase 6 benchmark

The checked Phase 6 report records five fresh-process runs per version using the same 3.11147-second, mono 44.1 kHz user-supplied WAV, GCC Release build, machine, and four-argument positional invocation.

| Build | Median render time | Throughput |
|---|---:|---:|
| Phase 5.1 | 0.6170 s | 5.04× real time |
| Phase 6 | 0.5041 s | 6.17× real time |

The Phase 6 median was 18.3% lower on that test. This is a local comparison, not a universal speed guarantee. A single peak-RSS run reported 7,416 KiB for Phase 5.1 and 7,540 KiB for Phase 6; it does not show a memory reduction.

Full details are in [PERFORMANCE_BENCHMARK.md](PERFORMANCE_BENCHMARK.md) and [PHASE6_TEST_REPORT.md](PHASE6_TEST_REPORT.md).

## Reproduce a benchmark

```bash
python scripts/benchmark.py ./build/ResynthUTAU voice.wav --runs 5 --json benchmark.json
```

On Windows:

```powershell
python scripts/benchmark.py .\build\Release\ResynthUTAU.exe .\voice.wav --runs 5 --json benchmark.json
```

The script expects a PCM WAV sample that Python's standard-library `wave` module can open. It uses the project's four-argument positional invocation with numeric pitch 100 and velocity 100, writes to a temporary output path, measures each fresh process, and reports median elapsed time and real-time factor. The `--runs` value is limited to 1–50 (default 3). It does not alter the input file. Python 3 is needed only for this script, not for the executable.

For meaningful comparisons, use the same input, executable build type, machine power mode, and run count. Compare medians rather than individual runs.

## Concurrency

The DSP workspaces are per invocation and no process-global mutable analysis buffers or caches are used. Multiple independent resampler processes do not share render state. Internal multithreading is not enabled in this version.

## Memory and file safety

- RIFF chunks are checked against the declared container boundary before reading or seeking.
- A narrow compatibility exception accepts a RIFF size field exactly eight bytes larger than the actual file; larger overruns are rejected.
- A single input data chunk is limited to 512 MiB.
- Classic RIFF/WAVE is supported; RF64 is explicitly rejected.
- PCM16/24/32 and IEEE float32/64 are decoded; WAVE_FORMAT_EXTENSIBLE is accepted for standard PCM and IEEE-float subtype GUIDs.
- Mono and stereo are supported. More than two channels and compressed WAV codecs are unsupported.
- Output is PCM16 at the input sample rate and channel count.

## Verification boundary

Local C++20 builds and tests cover file parsing, workspace equivalence, and command-line metadata. They do not verify installation or rendering inside UTAU/OpenUtau. The current source also lacks OpenUtau's Base64/run-length pitch-curve decoding and signed cutoff handling. Those require a dedicated adapter and real host tests.
