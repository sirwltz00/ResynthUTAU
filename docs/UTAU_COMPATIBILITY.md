# UTAU compatibility — Phase 1

## Classic positional interface

```text
ResynthUTAU <input.wav> <output.wav> <pitch_note> <velocity> [flags [offset_ms length_required_ms [fixed_length_ms [end_blank_ms [volume_percent [modulation_percent [pitch_bend...]]]]]]]
```

The parser follows the classic trailing-optional positional layout. Because later positional values cannot be supplied while omitting an earlier slot, use `-` as the empty flags placeholder when timing arguments follow.

| Position | Value | Phase 1 behavior |
|---:|---|---|
| 1 | Input WAV path | Read and validate. |
| 2 | Output WAV path | Create parent folders and write PCM16 WAV. |
| 3 | Pitch note | Validate note spelling and MIDI range; pitch shift is not implemented. |
| 4 | Velocity | 0–200; scales the fixed region's duration. |
| 5 | Flags | Parse as a string; report that effects are not active yet. |
| 6 | Offset (ms) | Trim this many frames from the start. |
| 7 | Required length (ms) | Set exact output duration in frames. |
| 8 | Fixed length (ms) | Mark the initial region affected by velocity. |
| 9 | End blank (ms) | Trim this many frames from the end. |
| 10 | Volume (%) | Apply linear gain after duration rendering. |
| 11 | Modulation (%) | Parse and validate; not implemented yet. |
| 12+ | Pitch-bend tokens | Preserve as tokens and warn; not implemented yet. |

The argument names and general ordering follow the UTAU resampler usage string. UTAU's note parameter is a note name such as `G#4`, not a literal frequency percentage. Historical descriptions indicate that `velocity` is consonant speed and that offset/required length/fixed length/end blank are millisecond timing fields. The exact behavior of every UTAU build and renderer is not assumed to be identical; see the implementation notes below.

## WAV compatibility

Supported input: PCM16/24/32 and IEEE float32/64, mono/stereo, arbitrary sample rates accepted by the WAV header. Output: PCM16, same sample rate and channel count. WAVE_FORMAT_EXTENSIBLE, RF64, compressed WAV codecs, and more than two channels are not supported in Phase 1.

## Voicebank formats

The resampler does not inspect reclists, `oto.ini`, or phonemizer data. It processes only the WAV path supplied by the host, so the same entry point can be called for CV, VCV, CVVC, CV+VV, CVC, and other WAV-based banks. Actual rendering quality across those types still requires future consonant-aware and pitch-aware processing.

## OpenUTAU

The program targets OpenUTAU's Classic renderer only when its invocation matches this positional interface. It does not implement OpenUTAU's native renderer API.

## Windows paths

The Windows executable uses a wide-character entry point and converts arguments to UTF-8 before passing them through the application. Filesystem paths are opened with `std::filesystem`.

## References used for the Phase 1 interface

These are community/developer documentation, not a formal normative API specification. The Japanese technical note for UTAU 0.4.16 explicitly cautions that some details are the author's investigation and may be imperfect:

- [UTAU resampler parameter notes (2014)](https://shinta0806be.ldblog.jp/archives/8298940.html)
- [UTAU developer technical notes: resampler.exe](https://utau2008.blog47.fc2.com/?cat=3)
- [OpenUTAU Wiki: Resamplers and Wavtools](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools)
