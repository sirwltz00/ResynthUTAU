# UTAU-style command-line compatibility

## Compatibility summary

Version 0.6.1 implements the standalone UTAU-style positional profile and an adapter for OpenUtau Classic's external-resampler command line. The Classic adapter is covered by local tests, but no UTAU or OpenUtau GUI runtime test has been completed for this source snapshot. It is not a native OpenUtau renderer.

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

## OpenUtau Classic external-resampler adapter

OpenUtau Classic's current external `ExeResampler` call uses 13 positional arguments. ResynthUTAU detects this mode when the tempo argument begins with `!` and maps the values as follows:

| Position | Meaning | ResynthUTAU handling |
|---:|---|---|
| 1 | Input WAV | Read source WAV |
| 2 | Output WAV | Write PCM16 WAV, creating parent directories |
| 3 | Tone name | Note-name pitch target |
| 4 | Velocity | Velocity scaling |
| 5 | Flags | Standard-compatible and custom expressions |
| 6 | Offset | Source start in ms |
| 7 | Required duration | Requested output length in ms |
| 8 | Consonant | Fixed source-region length in ms |
| 9 | Cutoff | Signed right-blank convention (see below) |
| 10 | Volume | Output gain in percent |
| 11 | Modulation | Source pitch-contour retention for the harmonic route |
| 12 | `!tempo` | Tempo in BPM |
| 13 | Pitch curve | Base64/run-length signed 12-bit cents offsets |

The pitch curve uses two characters per signed 12-bit value. The encoder's alphabet is `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/`. Values with bit 11 set are converted to signed values by subtracting 4096. A marker `#N#` means repeat the previous value N additional times. Malformed pairs, digits, and repeat markers are rejected. Decoded curves are capped at one million points.

OpenUtau samples the curve at 5 ticks per point. At 480 ticks per quarter note this is 96 points per quarter. The `!tempo` argument sets the point spacing on the output timeline. The decoded curve is aligned across the fixed consonant region and applied to TD-PSOLA mark spacing on the natural path; when a separate expression requires harmonic synthesis, the same pitch points are added to that renderer's pitch contour.

### Signed cutoff

OpenUtau's cutoff is not the standalone `end_blank_ms` field:

- `cutoff >= 0`: trim that many milliseconds from the end of the WAV.
- `cutoff < 0`: set the usable source endpoint to `offset + abs(cutoff)` milliseconds from the beginning of the WAV.

The source endpoint is clipped to the file. An empty usable window is rejected.

The standalone profile described above is unchanged. It continues to use a nonnegative end-blank value and its own optional `Q<tempo>`/literal-cents convention.

**Host status:** argument decoding and cutoff handling are unit/integration tested locally. Real OpenUtau GUI launch, manifest discovery, process behavior, pitch alignment against real notes, cancellation, and cache handling remain unverified. See [OPENUTAU_INTEGRATION.md](OPENUTAU_INTEGRATION.md).

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

The capabilities report describes the standalone CLI, OpenUtau Classic external-resampler command-line adapter, and WAV formats. It explicitly reports that real host runtime testing has not been performed and that the native OpenUtau renderer API is not implemented.

## References

- [OpenUtau Classic external-resampler argument construction (`ExeResampler.cs`)](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Classic/ExeResampler.cs)
- [OpenUtau signed 12-bit Base64 encoding (`Base64.cs`)](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Util/Base64.cs)
- [OpenUtau resampler item and cutoff calculation (`ResamplerItem.cs`)](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Classic/ResamplerItem.cs)
- [OpenUtau Wiki: Resamplers and Wavtools](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools)
- [UTAU resampler parameter notes (community documentation)](https://shinta0806be.ldblog.jp/archives/8298940.html)
