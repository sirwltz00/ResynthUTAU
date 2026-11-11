# UTAU compatibility — Phase 4

## Classic positional interface

```text
ResynthUTAU <input.wav> <output.wav> <pitch_percent_or_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

The parser follows the classic trailing-optional positional layout. Because later positional values cannot be supplied while omitting an earlier slot, use `-` as the empty flags placeholder when timing arguments follow.

| Position | Value | Phase 4 behavior |
|---:|---|---|
| 1 | Input WAV path | Read and validate. |
| 2 | Output WAV path | Create parent folders and write PCM16 WAV. |
| 3 | Pitch percent or target note | Accept a numeric percentage from 1–400 (100 = unchanged; 120 = 1.2× source F0), or a note such as `A#3`; note mode targets the note frequency using median voiced source F0. |
| 4 | Velocity | 0–200; scales the duration of the initial fixed region using the existing project mapping. |
| 5 | Flags | Parse as a string; expression processing is Phase 5 and not yet active. |
| 6 | Offset (ms) | Trim this many frames from the start before analyzing/rendering. |
| 7 | Required length (ms) | Set the exact output duration in frames. |
| 8 | Fixed length (ms) | Treat the initial part of the trimmed input as the fixed consonant region. |
| 9 | End blank (ms) | Trim this many frames from the end before rendering. |
| 10 | Volume (%) | Apply linear gain after timing and resynthesis; a final peak guard prevents clipping. |
| 11 | Modulation (%) | Parse and validate; not implemented until Phase 5. |
| 12+ | Pitch-bend tokens | Preserve as tokens and warn; pitch automation is Phase 5. |

## Timing model

1. Convert offset and end-blank milliseconds to integer sample frames and crop the input.
2. The first `fixed_length_ms` of the cropped sample is the fixed region. At velocity 100 it is copied without time modification. Other velocity values time-scale this region with the project mapping `2^((100 - velocity) / 100)` using WSOLA.
3. If required length is supplied, the fixed output region is capped to that total. The remaining requested frames are assigned to the tail. Without required length, the tail keeps its source frame count.
4. The tail is analyzed. A waveform-preserved WSOLA version is generated for consonants, unvoiced sections, and transient regions. Voiced sections are blended with time-mapped harmonic resynthesis.
5. The output frame count is checked against the requested number before writing.

The velocity mapping is a documented project convention carried forward from Phase 1, not a claim to duplicate every historical UTAU or resampler implementation. The precise semantics can vary between hosts and resamplers.

## Phonetic and transient handling

The resampler does not receive the full `oto.ini`, phoneme label, or reclist. The host supplies the selected WAV and timing values. Phase 4 therefore uses acoustic heuristics rather than language-aware phoneme recognition:

- Voiced frames require an F0 estimate, periodicity confidence, and locally periodic spectral evidence.
- Unvoiced frames are retained from the waveform-preserving path.
- Transients are conservatively flagged using a sharp RMS increase together with spectral flatness and high-frequency centroid evidence.
- A smoothed voicing-confidence mask blends the oscillator path into and out of waveform-preserved material.
- The fixed initial region is kept separately so the principal consonant timing landmark is not stretched as part of the vowel tail.

This is intended to preserve recognizable consonant attacks while allowing vowel sustain to be reconstructed without simply repeating the same waveform cycle. The detector is not a phoneme recognizer and may need tuning for specific voicebanks.

## Pitch targeting

The third positional argument supports two modes. A numeric percentage between 1 and 400 is converted directly to a ratio (`pitch_ratio = pitch_percent / 100`), so `100` preserves the detected pitch and `120` raises it by 20%. A note name is converted to equal-tempered frequency using A4 = 440 Hz. The median F0 across reliably voiced frames in the tail is used as the source reference, giving `pitch_ratio = target_note_frequency / median_source_F0`. The resulting ratio is applied to the continuous harmonic oscillator path. The spectral envelope remains independently estimated and is used for the existing bounded envelope compensation. When there is no stable voiced material, no artificial F0 or pitch ratio is invented; the tail uses waveform-preserving time stretching.

## WAV compatibility

Input: RIFF/WAVE PCM16/24/32 and IEEE float32/64, mono or stereo. Output: PCM16, same sample rate and channel count. WAVE_FORMAT_EXTENSIBLE, RF64, compressed WAV codecs, and more than two channels are not supported. Phase 2 analysis averages stereo channels to mono; the synthesized voiced signal is blended into each channel's waveform-preserved path.

## Voicebank formats

The resampler does not inspect reclists, `oto.ini`, or phonemizer data. It processes only the WAV path supplied by the host, so the entry point is format-agnostic for CV, VCV, CVVC, CV+VV, CVC, and other WAV-based banks. Consonant/vowel treatment is based on acoustic features, not alias names.

## OpenUTAU

The program targets OpenUTAU's Classic renderer only when its invocation matches this positional interface. It does not implement OpenUTAU's native renderer API. Actual host integration has not yet been verified.

## Windows paths

The Windows executable uses a wide-character entry point and converts arguments to UTF-8 before passing them through the application. Filesystem paths are opened with `std::filesystem`.

## References for the positional interface

These are community/developer documentation, not a formal normative API specification. The Japanese technical note for UTAU 0.4.16 explicitly cautions that some details are the author's investigation and may be imperfect:

- [UTAU resampler parameter notes (2014)](https://shinta0806be.ldblog.jp/archives/8298940.html)
- [UTAU developer technical notes: resampler.exe](https://utau2008.blog47.fc2.com/?cat=3)
- [OpenUTAU Wiki: Resamplers and Wavtools](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools)
