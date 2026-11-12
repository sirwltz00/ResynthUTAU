# UTAU compatibility — Phase 6

## Classic positional interface

```text
ResynthUTAU <input.wav> <output.wav> <pitch_percent_or_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

The parser follows the classic trailing-optional positional layout. Because later positional values cannot be supplied while omitting an earlier slot, use `-` as the empty flags placeholder when timing arguments follow.

| Position | Value | Renderer behavior |
|---:|---|---|
| 1 | Input WAV path | Read and validate. |
| 2 | Output WAV path | Create parent folders and write PCM16 WAV. |
| 3 | Pitch percent or target note | Accept a numeric percentage from 1–400 (100 = unchanged; 120 = 1.2× source F0), or a note such as `A#3`; note mode targets the note frequency using median voiced source F0. |
| 4 | Velocity | 0–200; scales the duration of the initial fixed region using the existing project mapping. |
| 5 | Flags | Apply the documented standard-compatible and custom expression controls in [FLAGS.md](FLAGS.md). Unknown flags are ignored with warnings. |
| 6 | Offset (ms) | Trim this many frames from the start before analyzing/rendering. |
| 7 | Required length (ms) | Set the exact output duration in frames. |
| 8 | Fixed length (ms) | Treat the initial part of the trimmed input as the fixed consonant region. Consonant Rate additionally scales its output duration. |
| 9 | End blank (ms) | Trim this many frames from the end before rendering. |
| 10 | Volume (%) | Apply linear gain after timing and resynthesis; combined multiplicatively with custom Note Gain. A final peak guard prevents clipping. |
| 11 | Modulation (%) | 0–100; controls how much source F0 contour is retained around median source F0. 0 flattens it; 100 retains it. |
| 12+ | Pitch-bend tokens | Cents offsets, interpolated continuously. Optional leading `Q<tempo>` selects a 96-points-per-quarter-note grid; otherwise points are 256 source samples apart, mapped through the stretch ratio. |

## Timing model

1. Convert offset and end-blank milliseconds to integer sample frames and crop the input.
2. The first `fixed_length_ms` of the cropped sample is the fixed region. At velocity 100 and Consonant Rate 100 it is copied without time modification. Other settings scale this region with the project velocity mapping and consonant-rate multiplier using WSOLA.
3. If required length is supplied, the fixed output region is capped to that total. The remaining requested frames are assigned to the tail. Without required length, the tail keeps its source frame count.
4. The tail is analyzed. A waveform-preserved WSOLA version is generated for consonants, unvoiced sections, and transient regions. Voiced sections are blended with time-mapped harmonic resynthesis.
5. Continuous pitch automation is integrated into the harmonic oscillator phase. It does not reset oscillator phase at control points.
6. Note-level dynamics, brightness/filtering, decay, note gain, and peak protection are applied before the output is written.
7. The output frame count is checked against the requested number before writing.

The velocity mapping is a documented project convention carried forward from Phase 1, not a claim to duplicate every historical UTAU or resampler implementation. The precise semantics can vary between hosts and resamplers.

## Phonetic and transient handling

The resampler does not receive the full `oto.ini`, phoneme label, or reclist. The host supplies the selected WAV and timing values. The renderer therefore uses acoustic heuristics rather than language-aware phoneme recognition:

- Voiced frames require an F0 estimate, periodicity confidence, and locally periodic spectral evidence.
- Unvoiced frames are retained from the waveform-preserving path.
- Transients are conservatively flagged using a sharp RMS increase together with spectral flatness and high-frequency centroid evidence.
- A smoothed voicing-confidence mask blends the oscillator path into and out of waveform-preserved material.
- The fixed initial region is kept separately so the principal consonant timing landmark is not stretched as part of the vowel tail.
- Attack Speed shapes the first voiced onset detected in the tail; it does not attempt to recognize a named phoneme.

## Pitch and expression

The third positional argument supports two modes. A numeric percentage between 1 and 400 is converted directly to a ratio (`pitch_ratio = pitch_percent / 100`), so `100` preserves detected pitch and `120` raises it by 20%. A note name is converted to equal-tempered frequency using A4 = 440 Hz. The median F0 across reliably voiced frames in the tail is used as the source reference.

The positional modulation value controls retention of the measured source pitch contour: 0 produces a flat target contour and 100 retains the full analyzed variation. It is separate from the custom `vib` vibrato control. `t` adds a fine-pitch offset in 10-cent steps per flag unit.

The trailing pitch-bend points are interpreted as cents. They are linearly interpolated at analysis-frame positions and continuously integrated at the oscillator. Without `Q`, the grid spacing is 256 source samples multiplied by the time-stretch ratio. With `Q<tempo>`, the grid has 96 points per quarter note. Point values are constrained to ±2400 cents; invalid values are ignored with a warning.

`g` and `sf` shift the estimated spectral envelope independently from F0. `B`/`br` affect the aperiodic component and original texture. `nh` controls aperiodic energy, `hb` controls harmonic energy, and `vv` adds a separate upper-partial tilt. `dy`, `as`, `cr`, `ng`, `sb`, `H`, `Y`, and `dc` apply their documented envelope, timing, gain, or filtering behavior. See [FLAGS.md](FLAGS.md) for exact project-defined ranges.

## WAV compatibility

Input: RIFF/WAVE PCM16/24/32 and IEEE float32/64, mono or stereo. WAVE_FORMAT_EXTENSIBLE is supported when its subtype GUID is standard PCM or IEEE float. The valid-bits field is range-checked; integer sample containers are decoded according to their PCM container width. Output: PCM16, same sample rate and channel count. RF64, compressed WAV codecs, and more than two channels are not supported. Phase 2 analysis averages stereo channels to mono; the synthesized voiced signal is blended into each channel's waveform-preserved path.

The reader validates the declared RIFF container length, chunk payload boundaries, odd-byte padding, block alignment, channel count, encoding, and data-frame alignment before decoding. It tolerates one narrowly defined legacy writer error where the RIFF size field contains the total file length (exactly eight bytes too large); actual chunk payloads must still fit inside the file. Larger container overruns are rejected. Input data chunks above 512 MiB are rejected with a clear error. RF64 files are rejected explicitly rather than misread as RIFF.

## Voicebank formats

The resampler does not inspect reclists, `oto.ini`, or phonemizer data. It processes only the WAV path supplied by the host, so the entry point is format-agnostic for CV, VCV, CVVC, CV+VV, CVC, and other WAV-based banks. Consonant/vowel treatment is based on acoustic features, not alias names.

## OpenUTAU

The program targets OpenUTAU's Classic renderer only when its invocation matches this positional interface. It does not implement OpenUTAU's native renderer API. Actual host integration has not yet been verified. A successful local CLI test is not proof of compatibility with a particular UTAU/OpenUTAU host version.

## Windows paths

The Windows executable uses a wide-character entry point and converts arguments to UTF-8 before passing them through the application. Filesystem paths are opened with `std::filesystem`. The Windows Unicode-path behavior is covered by the wide-character entry-point implementation, but still requires an actual Windows runtime test before host compatibility can be declared.

## Capability query

`ResynthUTAU --version` prints the build version. `ResynthUTAU --capabilities` prints a small JSON document describing the implemented command-line interface and WAV formats. The capabilities output explicitly states that OpenUTAU Classic host integration is unverified and that the native OpenUTAU renderer is not implemented.

## References for the positional interface

These are community/developer documentation, not a formal normative API specification. The Japanese technical note for UTAU 0.4.16 explicitly cautions that some details are the author's investigation and may be imperfect:

- [UTAU resampler parameter notes (2014)](https://shinta0806be.ldblog.jp/archives/8298940.html)
- [UTAU developer technical notes: resampler.exe](https://utau2008.blog47.fc2.com/?cat=3)
- [OpenUTAU Wiki: Resamplers and Wavtools](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools)
