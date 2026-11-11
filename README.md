# ResynthUTAU

**Phase 1 foundation — version 0.1.0**
Independent C++20 UTAU resampler foundation. The long-term direction is a harmonic-plus-aperiodic resynthesis engine inspired by publicly described vocal-synthesis principles associated with Piapro Studio NT. This project is not affiliated with or endorsed by Crypton Future Media or Yamaha, and it does not reproduce their proprietary software or undocumented algorithms.

> **Important status note:** This is a working Phase 1 foundation, not the completed resynthesis engine described in the development roadmap. It reads WAV samples, applies offset/end-blank trimming, a waveform-domain WSOLA-style duration adjustment, fixed-region/velocity handling, volume, and writes PCM16 WAV. It currently preserves the source pitch. Harmonic analysis, aperiodic analysis, pitch/formant reconstruction, pitch bends, and expression flags are not yet implemented. The program prints warnings for those unimplemented controls rather than pretending they work.

## Current capabilities

- C++20 and CMake; no Python, Node.js, MATLAB, or runtime dependency for the executable.
- Classic RIFF/WAVE input: PCM 16/24/32-bit and IEEE float 32/64-bit, mono or stereo.
- PCM16 WAV output at the source sample rate.
- Unicode Windows command-line argument conversion and filesystem path handling.
- UTAU-style positional argument parsing, note-name validation, and safe range checks.
- Offset and end-blank trimming in milliseconds.
- Required duration with sample-count-accurate output length.
- Fixed-region duration adjustment driven by velocity, followed by a WSOLA-style waveform time stretch for the remaining region.
- Volume percentage and finite-value/clipping protection.
- CTest unit tests and Windows/Linux GitHub Actions workflows.

## Synthesis architecture

The planned destination architecture is harmonic-plus-aperiodic resynthesis. Phase 1 intentionally implements only the WAV and command-line foundation plus waveform-domain duration rendering. It does **not** yet claim to analyze or reconstruct harmonic and noise components. See [ARCHITECTURE.md](docs/ARCHITECTURE.md), [DSP_ALGORITHMS.md](docs/DSP_ALGORITHMS.md) and [LIMITATIONS.md](docs/LIMITATIONS.md).

## Installation in UTAU / OpenUTAU Classic

1. Build or download the Windows x64 executable from a tagged GitHub Release once one has been published.
2. Place `ResynthUTAU.exe` in a stable folder.
3. In UTAU, set the resampler path in Project Properties. In OpenUTAU, select the Classic renderer and configure the resampler there.
4. Test with a copy of a voicebank and a short project first.

This Phase 1 build is useful for testing command-line invocation, WAV handling, output timing, and basic duration processing. Because pitch shifting is not implemented yet, it is **not ready for normal singing production**.

## Command-line usage

```text
ResynthUTAU <input.wav> <output.wav> <pitch_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

Example (the `-` placeholder keeps the optional flags slot empty while supplying timing values):

```text
ResynthUTAU input.wav output.wav C4 100 - 0 500 100 0 100 0
```

The pitch note is validated (for example `C4`, `F#3`, or `Bb4`) but is not applied in Phase 1. The `velocity` value is accepted from 0 to 200. The current fixed-region time multiplier is `2^((100 - velocity) / 100)`, so 100 leaves that region unchanged, 0 doubles its duration, and 200 halves it. This is an explicit project convention for the Phase 1 renderer, not a claim about Crypton's or every historical UTAU engine's internal scaling.

See [UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) for argument behavior and [FLAGS.md](docs/FLAGS.md) for the current flag status.

## Platforms

- **Windows 10/11 x64:** primary target; produces `ResynthUTAU.exe`.
- **Linux x64:** supported as a development/test build; package availability depends on the tagged release workflow.
- **macOS:** not yet configured or tested.

## Builds and downloads

Every push and pull request is configured to build and test on Windows and Ubuntu using GitHub Actions. Tagged versions (`v*`) trigger a release workflow that packages Windows and Linux builds and SHA-256 checksums. No binary is included in the source repository. Releases will appear on the repository's GitHub Releases page after the workflow is run successfully.

## Development status

The implementation is deliberately staged. A passing Phase 1 build does not mean that the full harmonic-plus-aperiodic engine is complete. The next engineering stages are analysis, harmonic/noise separation, continuous-phase resynthesis, formant processing, pitch automation, phonetic/transient handling, and compatibility testing in real UTAU/OpenUTAU Classic projects.
