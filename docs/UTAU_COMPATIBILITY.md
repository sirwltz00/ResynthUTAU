# UTAU-style command-line compatibility

## Compatibility summary

Version 0.6.0 implements a **UTAU-style positional subset** for standalone WAV rendering. It is not yet a complete adapter for every UTAU-derived host. In particular, OpenUtau Classic's Base64/run-length pitch curve and signed cutoff argument are not implemented. No UTAU or OpenUtau GUI runtime test has been completed for this source snapshot.

## Positional interface implemented by this project

```text
ResynthUTAU <input.wav> <output.wav> <pitch_percent_or_note> <velocity> [flags [offset_ms [length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend_tokens...]]]]]]]]
```

The arguments after velocity are trailing optional values. When a later positional value is needed but flags are empty, pass `-` as the flags placeholder.

| Position | Value | Implemented meaning |
|---:|---|---|
| 1 | Input WAV path | Read and validate the WAV file. |
| 2 | Output WAV path | Create parent directories when necessary and write PCM16 WAV. |
| 3 | Pitch percentage or note | Numeric 1–400 gives a ratio (`100` = unchanged; `120` = 1.2×). A note such as `A#3` targets equal-tempered note frequency using median reliably voiced source F0. |
| 4 | Velocity | Range 0–200. Scales the fixed-region duration with the project mapping `2^((100 - velocity) / 100)`. |
| 5 | Flags | ResynthUTAU's documented standard-compatible and custom flags; see [FLAGS.md](FLAGS.md). `-` means no flags. |
| 6 | Offset (ms) | Crop this duration from the beginning before analysis. Must be non-negative. |
| 7 | Required length (ms) | Requested total output duration. When supplied, the output frame count is computed from this duration. Must be positive. |
| 8 | Fixed length (ms) | Source duration of the initial fixed/consonant region. Must be non-negative. |
| 9 | End blank (ms) | Crop this duration from the end of the source. Must be non-negative. |
| 10 | Volume (%) | Linear output gain; accepted range 0–1000. A final peak guard prevents clipping. |
| 11 | Modulation (%) | Range 0–100; controls how much analyzed source F0 contour is retained on the harmonic expression route. |
| 12+ | Literal pitch-bend tokens | Numeric cents points, optionally preceded by the project's `Q<tempo>` grid marker. These are not Base64 data. |

## Timing and synthesis behavior

1. Offset and end-blank durations are converted to sample frames and crop the input.
2. The first `fixed_length_ms` of the cropped source becomes the fixed region. At velocity 100 and Consonant Rate 100 its samples retain their original timing and waveform. Other settings use WSOLA to adjust its duration.
3. When required length is present, the fixed output is capped to that total and the remaining duration is assigned to the tail. The final frame count is checked before writing.
4. The tail is analyzed. WSOLA supplies the waveform-preserving path for unvoiced, transient, and silence regions.
5. The default voiced pitch-shifting path uses TD-PSOLA with fractional pitch-mark refinement. The source-position map controls duration independently from target pitch.
6. Controls that require oscillator or spectral processing route voiced material through the harmonic/aperiodic renderer. A smoothed acoustic voicing mask controls the transition.
7. Expression shaping and final peak protection are applied before PCM16 output.

The acoustic classifier does not receive `oto.ini`, aliases, reclist entries, or phonemizer labels. It estimates region type from the audio itself.

## Pitch and expression semantics

- A numeric third argument is interpreted as a ratio percentage, not a MIDI note.
- A note-name third argument is converted to equal-tempered frequency (A4 = 440 Hz) and uses median reliably voiced source F0 as the source reference.
- Modulation controls source-contour retention on the harmonic expression path. It is distinct from the custom `vib` control.
- `t` adds 10 cents per flag unit.
- Literal numeric trailing tokens are parsed as cents, limited to ±2400 cents. Invalid tokens are ignored with warnings.
- Without `Q`, the project's pitch-point spacing is 256 source samples, mapped through the output stretch ratio.
- `Q<tempo>` selects 96 points per quarter note, where the quarter-note duration is `60 / tempo` seconds.

This is the ResynthUTAU pitch-point convention. It is not a claim that other resamplers or hosts encode their pitch curves the same way.

## OpenUtau Classic is not fully supported yet

OpenUtau Classic currently constructs a 13-argument external-resampler invocation. In addition to the positional values above, it appends `!<tempo>` and a Base64/run-length-encoded signed 12-bit pitch array. It also uses signed cutoff semantics. This project version does not decode that payload or implement those cutoff rules.

Consequently:

- A Base64 pitch payload is not interpreted as a pitch curve by this build.
- `!<tempo>` is not the project's `Q<tempo>` token.
- The ninth OpenUtau argument is not safely interchangeable with this project's non-negative `end_blank_ms` field.
- The bundled expression manifest is experimental and does not resolve these gaps.

See [OPENUTAU_INTEGRATION.md](OPENUTAU_INTEGRATION.md) for the implementation plan and upstream source references.

## WAV support

Input: RIFF/WAVE PCM16/24/32 and IEEE float32/64, mono or stereo. WAVE_FORMAT_EXTENSIBLE is accepted when its subtype GUID is standard PCM or IEEE float and its metadata passes validation. Output: PCM16, preserving input sample rate and channel count.

The reader checks RIFF boundaries, chunk payloads, odd-byte padding, block alignment, data-frame alignment, and format metadata. It tolerates a narrowly defined RIFF-size overstatement of exactly eight bytes. Larger overruns are rejected. A data chunk above 512 MiB, RF64, compressed WAV codecs, and more than two channels are unsupported.

Analysis is performed on a mono average for stereo sources. Resynthesized voiced content is combined with the waveform-preserved content for each output channel; independent stereo phase is not reconstructed.

## Voicebank formats

The executable processes the WAV path passed to it. It does not parse `oto.ini`, phonemizer databases, reclists, or aliases. Its file-level input is therefore independent of whether a voicebank stores CV, VCV, CVVC, CV+VV, or another aliasing scheme, but phonetic timing decisions remain acoustic heuristics rather than voicebank-aware rules.

## Windows path handling

On Windows, `wmain` converts the wide command-line arguments to UTF-8, and filesystem paths are constructed from UTF-8. The code includes this handling, but actual Windows runtime behavior still needs to be tested.

## Diagnostic commands

```text
ResynthUTAU --version
ResynthUTAU --capabilities
```

The capabilities report describes the implemented CLI and WAV formats. It explicitly reports that complete OpenUtau Classic integration and the native OpenUtau renderer are not implemented.

## References

- [OpenUtau Classic external-resampler argument construction (`ExeResampler.cs`)](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Classic/ExeResampler.cs)
- [OpenUtau signed 12-bit Base64 encoding (`Base64.cs`)](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Util/Base64.cs)
- [OpenUtau resampler item and cutoff calculation (`ResamplerItem.cs`)](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Classic/ResamplerItem.cs)
- [OpenUtau Wiki: Resamplers and Wavtools](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools)
- [UTAU resampler parameter notes (community documentation)](https://shinta0806be.ldblog.jp/archives/8298940.html)
