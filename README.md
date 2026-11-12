# ResynthUTAU

**Phase 5 — expressive vocal resynthesis, version 0.5.1**

ResynthUTAU is an independent C++20 UTAU resampler and vocal-analysis/resynthesis project. Its default neutral pitch path is waveform-derived: TD-PSOLA handles stable voiced material, while WSOLA preserves consonants, unvoiced sounds, and transients. A separate phase-continuous harmonic-plus-aperiodic path provides the oscillator-based expression controls. The design is inspired by publicly described vocal-synthesis principles associated with Piapro Studio NT; it is not affiliated with Crypton Future Media or Yamaha and does not claim to duplicate either product's internal algorithms.

> **Current status:** Phases 1–5 and the Phase 5.1 natural-path refinement are implemented locally and covered by synthetic regression tests plus the supplied real-sample pitch-shift check. Phase 5 adds pitch automation, modulation, vibrato, formant movement, breathiness, harmonic/noise balance, dynamics, note gain, consonant-rate timing, attack shaping, brightness, filtering, and decay. Real-world perceptual tuning across more voicebanks and runtime testing in UTAU/OpenUTAU Classic remain necessary before a production release.

## What's new in Phase 5

- Added a case-sensitive standard-flag profile for `g`, `t`, `B`, `H`, and `Y`, with documented bounded ranges.
- Added custom multi-character expression flags: `br`, `sf`, `vv`, `cr`, `as`, `ng`, `nh`, `hb`, `dy`, `vib`, `vr`, `sb`, and `dc`/`dec`.
- Implemented independent harmonic and aperiodic gains, breathiness, noise suppression, voice-voltage-inspired upper-partial shaping, and harmonic balance.
- Added independent spectral-envelope shifting. `g` and `sf` move formant structure without simply changing the fundamental frequency.
- Added continuous cents-based pitch automation and vibrato. Pitch curves are interpolated during oscillator phase integration, preserving phase continuity instead of restarting oscillators at automation points.
- Implemented UTAU modulation as the retained proportion of the source F0 contour (0 = flattened contour, 100 = full source contour), plus the fine-pitch `t` adjustment.
- Added consonant-rate timing, a voiced attack-speed envelope, dynamic contour shaping, spectral brightness, low-pass filtering, note gain, and a smooth terminal decay.
- Unknown or unsupported flags are ignored and reported rather than causing a crash.
- Added regression tests for expression parsing, pitch bend, vibrato, gain scaling, consonant-rate timing, decay, and finite output.

The automatic phonetic classifier remains a conservative acoustic guide, not a complete phoneme recognizer. It does not inspect `oto.ini` or infer phoneme names; the host still decides which sample WAV to send to the resampler.

## Synthesis architecture

1. **Analyze:** centered FFT frames, YIN F0, harmonic partials, cepstral spectral envelope, aperiodicity profiles, RMS, spectral centroid, and flatness.
2. **Classify:** identify stable voiced regions and protect unvoiced, transient, and silent regions.
3. **Preserve timing landmarks:** leave the fixed initial region as recorded where possible; use WSOLA for unvoiced/transient material and pitch-synchronous overlap-add (TD-PSOLA) for stable voiced regions.
4. **Resynthesize voiced material:** the neutral pitch path repositions waveform-synchronous grains, retaining the recorded vowel spectrum and source phase structure. The separate expression path uses continuous harmonic oscillators and an aperiodic component when oscillator-based pitch automation or timbre controls are requested.
5. **Apply expression:** interpolate pitch curves at the oscillator when that path is active, shift spectral-envelope resonances independently, and adjust harmonic/aperiodic balance and dynamic contours.
6. **Blend and finish:** use one WSOLA fallback and one linear complementary transition at phonetic boundaries, then apply note gain, brightness/filtering, decay, and a peak guard.

This is a clean-room DSP implementation using conventional signal-processing methods. It is not a reverse-engineering claim about Piapro Studio NT.

## UTAU command-line usage

```text
ResynthUTAU.exe <input.wav> <output.wav> <pitch_percent_or_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

Example with named controls:

```text
ResynthUTAU.exe voice.wav rendered.wav A#3 100 "br35,sf+2,vv110,cr100,as110,ng100,nh100,hb100,dy100,vib35,vr5.5,sb105,dc10" 0 650 80 0 100 0
```

Example with compact standard flags and custom flags:

```text
ResynthUTAU.exe voice.wav rendered.wav A#3 100 "g+10t-2B30H10Y15br40sf+3" 0 650 80 0 100 0
```

Example pitch bend (two points, 0 cents to +300 cents):

```text
ResynthUTAU.exe voice.wav rendered.wav 100 100 - 0 650 80 0 100 0 0 300
```

The third argument accepts either a musical note (for example, `A#3`) or a numeric pitch percentage from 1 to 400 (`100` leaves detected F0 unchanged; `120` multiplies detected F0 by 1.2). `g`, `t`, `B`, `H`, and `Y` are case-sensitive. Custom flag names are lowercase. See [FLAGS.md](docs/FLAGS.md) and [UTAU_COMPATIBILITY.md](docs/UTAU_COMPATIBILITY.md) for ranges and behavior.

The pitch-bend point stream is documented by this project as cents values. Without a tempo marker it uses a 256-source-sample grid mapped through the stretch ratio; a leading `Q<tempo>` uses 96 points per quarter note. It is interpolated continuously and does not reset oscillator phase.

## Analysis and diagnostic resynthesis

```text
ResynthUTAU --analyze voice_sample.wav [report.json]
ResynthUTAU --resynthesize voice_sample.wav reconstructed.wav [pitch_ratio]
```

`--analyze` prints a concise summary and can export detailed JSON. `--resynthesize` is the standalone Phase 3 analysis-to-waveform diagnostic; the conventional UTAU invocation uses the timing, phonetics, and expression path.

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler (MSVC 2022, GCC 11+, or Clang 14+ recommended). The executable has no Python, Node.js, MATLAB, or external FFT runtime dependency.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

See [BUILDING.md](docs/BUILDING.md), [TESTING.md](docs/TESTING.md), and [FLAGS.md](docs/FLAGS.md). GitHub Actions workflows are included for Windows and Linux builds, but a local build does not imply that a remote workflow ran.

## Platforms

- **Windows 10/11 x64:** primary target; the CI/release workflow builds `ResynthUTAU.exe`.
- **Linux x64:** supported development and test target.
- **macOS:** not configured or verified.

## Known limitations

- The phonetic classifier detects acoustic region types; it does not perform language-aware phoneme recognition.
- The transient detector is heuristic and may miss subtle plosives or overprotect sharp vowel attacks.
- WSOLA can smear very short consonants or unusual transient patterns. Very short regions use a linear interpolation fallback.
- The natural voiced path uses waveform-derived TD-PSOLA with fractional pitch marks and linear complementary transitions. This avoids summing an independently phased harmonic reconstruction with the source vowel waveform. It is still heuristic and needs listening tests across voice types and phonation styles.
- The F0 estimate used for note transposition can be unreliable on very breathy, noisy, polyphonic, or strongly varying recordings.
- The spectral envelope is cepstrally smoothed, not a dedicated high-resolution formant model. Formant movement is an envelope-reindexing approximation and may change vowel color unnaturally at extreme settings.
- Aperiodicity is estimated from a spectral mask and resynthesized stochastically; it is not an exact recovery of the original residual waveform.
- Dynamics, breathiness, attack, decay, and voice-voltage controls are DSP-inspired approximations, not physically measured laryngeal controls or proprietary parameter replicas.
- The UTAU compatibility layer is based on the conventional positional interface. Host-specific pitch bend streams and flag extensions can vary between engines; verify the documented ResynthUTAU profile before using existing projects.
- UTAU/OpenUTAU Classic compatibility has not yet been validated inside the actual host applications.

See [LIMITATIONS.md](docs/LIMITATIONS.md) for what is implemented, approximated, and still outstanding.

## Project roadmap

1. **Phase 1 — functional foundation:** implemented.
2. **Phase 2 — audio analysis:** implemented.
3. **Phase 3 — harmonic/aperiodic resynthesis:** implemented as an offline reconstruction path.
4. **Phase 4 — timing and phonetics:** implemented in the conventional renderer, with heuristic acoustic region classification and synthetic regression coverage.
5. **Phase 5 — expression controls:** implemented as documented clean-room controls with synthetic regression coverage.
6. **Phase 6 — compatibility and optimization:** actual UTAU/OpenUTAU testing, real-voicebank quality tuning, performance, and memory work.
7. **Phase 7 — release:** documentation, platform builds, and verified packaging.
