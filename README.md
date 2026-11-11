# ResynthUTAU

**Phase 4 — timing-aware phonetic resynthesis, version 0.4.0**

ResynthUTAU is an independent C++20 UTAU resampler and vocal-analysis/resynthesis project. Its architecture combines harmonic-plus-aperiodic resynthesis with waveform-preserving processing for consonants, unvoiced material, and transients. It takes the input WAV supplied by UTAU and reconstructs voiced regions from analyzed acoustic features. The design is inspired by publicly described vocal-synthesis principles associated with Piapro Studio NT; it is not affiliated with Crypton Future Media or Yamaha and does not claim to duplicate either product's internal algorithms.

> **Current status:** Phases 1–3 provide WAV I/O, UTAU argument handling, FFT/YIN analysis, harmonic and aperiodic feature extraction, and offline resynthesis. Phase 4 connects timing and phonetic-region handling to the conventional UTAU positional invocation. Fixed material and unvoiced/transient regions are waveform-preserved, while voiced material is reconstructed with time-mapped harmonic synthesis and a low-level aperiodic component. Synthetic integration tests pass. Real-world UTAU/OpenUTAU project testing and perceptual evaluation across recorded voicebanks are still required.

## What's new in Phase 4

- The standard UTAU invocation now calls `renderPhase4`; it no longer falls back to the Phase 1 renderer.
- Offset and end-blank trimming, required output duration, fixed-region timing, velocity scaling, and volume are applied in one sample-accurate path.
- The initial fixed region is kept as recorded material (or WSOLA-adjusted when velocity changes its duration).
- The remaining sample is analyzed into voiced, unvoiced, transient, and silence regions. Classification is conservative and uses YIN periodicity together with local aperiodicity, spectral flatness, spectral centroid, and energy-rise cues.
- Unvoiced and transient material stays on a waveform-preserving WSOLA path to protect consonant articulation.
- Voiced material is blended with harmonic resynthesis. A small amount of the original waveform and a low-level aperiodic component retain some natural breath and texture.
- The harmonic and noise synthesizers now support time-mapped output durations. Vowel sustain can be lengthened without lowering the pitch as naive playback-rate stretching would.
- Short interpolation and boundary ramps reduce abrupt joins. A final peak guard prevents clipping.
- The standard renderer estimates the median source F0 and applies either the requested UTAU target note or numeric pitch percentage. Unvoiced-only samples are not assigned an artificial pitch.

The automatic phonetic classifier is a conservative frame-level guide, not a complete phoneme recognizer. It does not inspect `oto.ini` or infer phoneme names; the host still decides which sample WAV to send to the resampler.

## Phase 3.1 resynthesis-quality update

- Harmonic partial phases are referenced to the center sample of each analysis frame, rather than the start of the FFT frame.
- Full measured phase following (`phase_observation_strength = 1.0`) is the default.
- Regression tests cover a clean 220 Hz sine and phase-reference consistency across 1024-, 2048-, and 4096-sample windows.

## Synthesis architecture

1. **Analyze:** centered FFT frames, YIN F0, harmonic partials, cepstral spectral envelope, aperiodicity profiles, RMS, spectral centroid, and flatness.
2. **Classify:** identify stable voiced regions and conservatively protect unvoiced, transient, and silent regions.
3. **Preserve timing landmarks:** leave the fixed initial region intact, then use WSOLA for the waveform-preserved path.
4. **Resynthesize voiced material:** continuously integrate harmonic oscillator phase, retain spectral-envelope compensation, and time-map harmonic/noise frame trajectories across the requested output duration.
5. **Blend:** use a smoothed voicing confidence mask to mix the resynthesized vowel path with waveform-preserved material, then apply volume and a peak guard.

This is a clean-room DSP implementation using conventional signal-processing methods. It is not a reverse-engineering claim about Piapro Studio NT.

## UTAU command-line usage

```text
ResynthUTAU.exe <input.wav> <output.wav> <pitch_percent_or_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

Example:

```text
ResynthUTAU.exe voice.wav rendered.wav A#3 100 - 0 650 80 0 100 0
# Or use a relative pitch percentage: 120 = 1.2x the detected source F0
ResynthUTAU.exe voice.wav rendered.wav 120 100 - 0 650 80 0 100 0
```

Because optional arguments are trailing-only, use `-` for an empty flags slot when later timing arguments follow. Timing values are in milliseconds. See [UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) for the implemented behavior and caveats.

The third argument accepts either a musical note (for example, `A#3`) or a numeric pitch percentage from 1 to 400 (`100` leaves the detected source F0 unchanged; `120` multiplies it by 1.2). Note targeting compares the equal-tempered target frequency with the median measured F0 of the voiced tail. This is a practical source-pitch estimate, not an explicit recorded-root-note metadata system. Pitch-bend automation and modulation remain Phase 5 work.

## Analysis and diagnostic resynthesis

```text
ResynthUTAU --analyze voice_sample.wav [report.json]
ResynthUTAU --resynthesize voice_sample.wav reconstructed.wav [pitch_ratio]
```

`--analyze` prints a concise summary and can export detailed JSON. `--resynthesize` is the standalone Phase 3 analysis-to-waveform diagnostic; its optional pitch ratio is a frequency multiplier (`1.0` unchanged, `2.0` one octave up). The conventional UTAU invocation uses the Phase 4 timing/phonetics path instead.

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler (MSVC 2022, GCC 11+, or Clang 14+ recommended). The executable has no Python, Node.js, MATLAB, or external FFT runtime dependency.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

See [BUILDING.md](docs/BUILDING.md) and [TESTING.md](docs/TESTING.md). GitHub Actions workflows are included for Windows and Linux builds, but a local build does not imply that a remote workflow ran.

## Platforms

- **Windows 10/11 x64:** primary target; the CI/release workflow builds `ResynthUTAU.exe`.
- **Linux x64:** supported development and test target.
- **macOS:** not configured or verified.

## Known limitations

- The phonetic classifier detects acoustic region types; it does not perform language-aware phoneme recognition.
- The transient detector is heuristic and may miss subtle plosives or overprotect sharp vowel attacks.
- WSOLA can smear very short consonants or unusual transient patterns. Very short regions use a linear interpolation fallback.
- The F0 estimate used for note transposition can be unreliable on very breathy, noisy, polyphonic, or strongly varying recordings.
- The spectral envelope is cepstrally smoothed, not a dedicated high-resolution formant model.
- Aperiodicity is estimated from a spectral mask and resynthesized stochastically; it is not an exact recovery of the original residual waveform.
- The standard UTAU renderer still does not apply pitch bends, modulation, or Phase 5 expression flags.
- UTAU/OpenUTAU Classic compatibility has not yet been validated inside the actual host applications.

See [LIMITATIONS.md](docs/LIMITATIONS.md) for what is implemented, approximated, and still outstanding.

## Project roadmap

1. **Phase 1 — functional foundation:** implemented.
2. **Phase 2 — audio analysis:** implemented.
3. **Phase 3 — harmonic/aperiodic resynthesis:** implemented as an offline reconstruction path.
4. **Phase 4 — timing and phonetics:** implemented in the conventional renderer, with heuristic acoustic region classification and synthetic regression coverage.
5. **Phase 5 — expression controls:** next; dynamics, breathiness, formant shifting, consonant rate, attack speed, voice-voltage-inspired control, vibrato, and pitch automation.
6. **Phase 6/7 — compatibility, optimization, and release:** actual UTAU/OpenUTAU testing, quality tuning, performance work, and verified release packaging.
