# OpenUtau Classic integration

## Status in version 0.6.1

**The external Classic command-line adapter is implemented and covered by local automated tests. Actual OpenUtau GUI/runtime integration is not yet verified.** ResynthUTAU is an external `.exe` resampler for the Classic renderer. It does not implement OpenUtau's native renderer API and does not replace the wavtool.

OpenUtau's documented setup is to place the `.exe` in its `Resamplers` directory (or install it through the UI), then choose the **CLASSIC** renderer and select the resampler. The matching manifest must use the executable's base name, so `ResynthUTAU.exe` is paired with `ResynthUTAU.yaml`. The wavtool remains OpenUtau's separate `simple` or `convergence` component. See the [OpenUtau resampler installation guide](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools).

## Implemented host invocation

The adapter detects the 13-argument pattern used by OpenUtau's `ExeResampler`:

| Position | OpenUtau value | ResynthUTAU mapping |
|---:|---|---|
| 1 | Input temp WAV | UTF-8 input path |
| 2 | Output temp WAV | UTF-8 output path; parent directory is created when needed |
| 3 | Tone name | Equal-tempered note target, e.g. `A3` |
| 4 | Velocity | Fixed/consonant-region duration scaling |
| 5 | Flags string | Manifest-filtered flag string parsed by the expression parser |
| 6 | Offset | Source start in milliseconds |
| 7 | Required duration | Requested output duration in milliseconds |
| 8 | Consonant | Fixed source region in milliseconds |
| 9 | Signed cutoff | Nonnegative value trims from the WAV end; negative value sets the source endpoint to `offset + abs(cutoff)` |
| 10 | Volume | Output gain percentage |
| 11 | Modulation | Source-contour retention control on the harmonic expression path |
| 12 | `!tempo` | Tempo in BPM; selects the host pitch grid |
| 13 | Base64 pitch curve | Signed 12-bit pitch points plus run-length repeat markers |

The invocation shape is checked using the `!tempo` field. An argument beginning with `!` but containing a malformed or missing tempo value is treated as an OpenUtau invocation and rejected with a parse error rather than silently falling back to standalone mode. The ordinary standalone positional profile, including numeric cents tokens and its `Q<tempo>` marker, remains a separate mode.

## Pitch curve decoding and timing

The current OpenUtau encoder uses the alphabet `A-Z`, `a-z`, `0-9`, `+`, `/`. Each point is represented by two 6-bit digits forming one 12-bit signed integer. Values with bit 11 set are sign-extended by subtracting 4096. A run marker `#N#` repeats the previous decoded value N additional times. The decoder rejects malformed markers, incomplete pairs, invalid digits, zero-length runs, and curves above its configured point limit.

OpenUtau samples pitch points at 5-tick intervals. With the host's 480-tick quarter-note grid, this is 96 points per quarter note. The `!tempo` value sets the point spacing to `sample_rate * 60 / (tempo * 96)` frames on the output timeline. The curve is aligned from the input window and carried across the fixed consonant region; the tail renderer queries it using the fixed output-region length as its origin.

When the OpenUtau pitch curve is neutral and no other expression requires spectral/oscillator processing, the renderer keeps the waveform-derived TD-PSOLA route. Non-neutral OpenUtau pitch points alter the natural TD-PSOLA synthesis-mark spacing. If a control selects the harmonic/aperiodic expression route, the decoded curve is integrated into the harmonic pitch contour instead.

## Cutoff semantics

OpenUtau's signed cutoff is handled separately from the standalone mode's nonnegative `end_blank_ms` field:

- `cutoff >= 0`: trim `cutoff` milliseconds from the end of the WAV.
- `cutoff < 0`: set the usable endpoint to `offset + abs(cutoff)` milliseconds from the beginning of the WAV.

The usable source window is clipped to the file bounds. An empty or inverted source window is rejected. This matches the signed right-blank convention used by OpenUtau Classic's external-resampler inputs; it is not treated as a generic positive/negative end-blank duration.

## Manifest and flags

`ResynthUTAU.yaml` is paired with the executable name and enables the manifest expression filter. It lists ResynthUTAU's project-specific `g`, `t`, `B`, `H`, `Y`, and custom controls. Those controls use the ranges documented in [FLAGS.md](FLAGS.md); they do not promise identical parameter curves or sound to other resamplers.

## Install instructions for Windows

1. Extract the Windows x64 package.
2. Copy `ResynthUTAU.exe` and `ResynthUTAU.yaml` into OpenUtau's `Resamplers` folder, or use **Tools > Install Wavtool/Resampler (.exe)** and choose **Install as resampler**.
3. Set the track renderer to **CLASSIC**.
4. Open the Classic renderer's gear/settings menu and select `ResynthUTAU` as the resampler.
5. Keep the wavtool set separately to `simple` or `convergence`.

## Not verified yet

See [OPENUTAU_ADAPTER_TEST_REPORT.md](OPENUTAU_ADAPTER_TEST_REPORT.md) for the exact local test coverage and compiler results.

The automated tests cover argument mapping, signed 12-bit decode, run-length markers, malformed-payload rejection, and negative-cutoff rendering. They do not launch OpenUtau. The following still need user or CI validation inside the real application:

- Whether OpenUtau discovers and loads the manifest as intended.
- Windows executable launch, process exit codes, file quoting, and Unicode temp paths.
- Repeated note renders, simultaneous render requests, cancellation, and cache behavior.
- Pitch-bend alignment and expressive flags against real voicebanks and musical phrases.
- Output behavior with the user's chosen wavtool and OpenUtau version.

A successful command-line test does not establish perfect end-to-end host compatibility. Report any logs or failed notes from your OpenUtau test and the adapter can be refined against those results.

## Upstream references

- [OpenUtau external-resampler invocation (`ExeResampler.cs`)](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/Classic/ExeResampler.cs)
- [OpenUtau 12-bit pitch curve encoder (`Base64.cs`)](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/Util/Base64.cs)
- [OpenUtau pitch-curve preparation (`ResamplerItem.cs`)](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/Classic/ResamplerItem.cs)
- [OpenUtau resampler and wavtool installation](https://github.com/openutau/OpenUtau/wiki/Resamplers-and-Wavtools)
