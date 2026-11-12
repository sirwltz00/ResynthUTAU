# Phase 6 — performance and resource behavior

## Reusable analysis memory

The analysis stage now owns a `PitchWorkspace` per render. Its frame, Hann window, FFT spectrum, prefix-energy, YIN difference, and CMND buffers are reused for every analysis hop. The workspace is not global or static: simultaneous invocations receive independent buffers and do not share mutable DSP state.

The spectral stage precomputes Hann coefficients and their sums for the power-of-two window sizes used by the selected analysis configuration. A single complex spectrum buffer is reused between frames. This removes repeated trigonometric window generation and repeated FFT-buffer allocation from the per-hop loop.

These changes do not change the analysis frame hop, window sizes, F0 thresholds, harmonic extractor, spectral envelope, or resynthesis algorithms. A regression compares the reusable pitch workspace against the stateless API at multiple centers; F0, periodicity, and voiced decisions must match.

## Benchmarking

Use the included dependency-free benchmark script:

```bash
python scripts/benchmark.py ./build/ResynthUTAU voice.wav --runs 5 --json benchmark.json
```

On Windows:

```powershell
python scripts/benchmark.py .\build\Release\ResynthUTAU.exe .\voice.wav --runs 5 --json benchmark.json
```

The script uses the conventional four-argument UTAU invocation with neutral pitch and velocity, records wall-clock duration for each fresh process, and reports median time plus real-time factor. It intentionally uses a temporary output path and does not alter the input. For comparable measurements, use the same input file, machine power mode, executable build type, and run count. Close other CPU-heavy applications and compare medians, not single runs.

The benchmark does not claim a universal speed target. CPU, compiler, optimization settings, operating system, and sample content all affect the result. The sample must be representative of the user's actual voicebank material.

## Concurrency

The DSP workspaces are per invocation and no process-global analysis buffers or mutable caches are used. Multiple independent resampler processes can run concurrently without sharing render state. Internal multithreading is deliberately not enabled in this version: the current per-frame synthesis pipeline has phase/time dependencies, and parallelizing it without a dedicated thread-safe frame scheduler risks nondeterministic output or quality regressions.

## Memory and file safety

- Input RIFF chunks are checked against the declared container boundary before reading or seeking. A narrowly defined legacy exception accepts files whose RIFF size field mistakenly includes the eight-byte RIFF header; larger overruns are rejected.
- A single `data` chunk is limited to 512 MiB to prevent unreasonable allocation requests.
- Classic RIFF/WAVE files are supported; RF64 is explicitly rejected with a clear message.
- PCM16/24/32 and IEEE float32/64 are decoded; WAVE_FORMAT_EXTENSIBLE is accepted for PCM and IEEE-float subtype GUIDs.
- Mono and stereo are supported. More than two channels, compressed codecs, and RF64 remain unsupported.
- Each rendering process uses independent work buffers. No cross-process temporary cache is used.

## Phase 6 verification boundary

The local C++20 build and tests verify file parsing, DSP workspace equivalence, and command-line metadata. They do not verify audio-engine installation inside UTAU or OpenUTAU Classic. That requires running the Windows executable through the actual host applications with representative banks and checking the generated timing and WAV outputs.
