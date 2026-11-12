# Phase 6 benchmark report

## Test setup

- Input: supplied `ka-ka-ki-ka-ku-ka-N-ka.wav` sample (not distributed in this source archive)
- Duration: approximately 3.11147 seconds
- Format: mono, 44,100 Hz, PCM16
- Host: Linux x86_64 container, Intel Xeon Platinum 8573C
- Compiler: GCC 14.2.0
- Build: CMake Release
- Invocation: `<ResynthUTAU> <input.wav> <output.wav> 100 100`
- Runs: five fresh-process measurements per build

## Results

| Build | Median elapsed | Audio throughput |
|---|---:|---:|
| Phase 5.1 | 0.6170 s | 5.04× real time |
| Phase 6 | 0.5041 s | 6.17× real time |

Phase 6 used approximately **18.3% less wall-clock time** on this input and machine. This is a local comparative result, not a universal performance guarantee. A single `/usr/bin/time` run reported peak resident memory of 7,416 KiB for Phase 5.1 and 7,540 KiB for Phase 6; this does not establish a memory reduction.

## Audio output equivalence

The supplied sample was rendered by Phase 5.1 and Phase 6 using the same positional invocation at neutral pitch. The WAV outputs had the same shape and were sample-for-sample identical: maximum absolute difference 0, RMS difference 0, and correlation 1.0.

The same comparison was repeated for +1, +3, +5, -1, -3, and -5 semitone ratios. All five shifted Phase 6 WAV outputs were sample-for-sample identical to their Phase 5.1 counterparts. This verifies that the Phase 6 optimization did not alter these tested outputs.

## Limits

These measurements cover one short mono sample, one Linux CPU, and a GCC Release build. They do not establish performance on Windows, on longer files, on stereo inputs, or across a large collection of voicebanks. Actual UTAU/OpenUtau host integration and Windows runtime verification remain outstanding.
