# OpenUtau integration status

## Status in version 0.6.0

**Experimental only — full OpenUtau Classic compatibility is not implemented or host-tested.** The repository includes an external-resampler expression manifest (`ResynthUTAU.yaml`) so the project can be evaluated as a development integration, but the executable does not yet decode every argument convention used by OpenUtau Classic.

Do not use this build as a validated production resampler for OpenUtau projects that rely on pitch automation or cutoff behavior.

## What is present

- A standalone executable with a UTAU-style positional command-line interface.
- WAV file input/output and the timing, pitch, volume, and expression controls described in [UTAU_COMPATIBILITY.md](UTAU_COMPATIBILITY.md).
- A YAML expression manifest with project-defined controls for a Classic-style external-resampler UI.
- `--version` and `--capabilities` for diagnostics.

## Blocking compatibility gaps

OpenUtau Classic's external resampler invocation currently includes the ordinary input/output/tone/velocity/flags/timing/volume/modulation parameters, followed by a tempo token and a Base64-encoded pitch curve. The pitch curve encodes signed 12-bit integer values and uses run-length markers. See OpenUtau's [`ExeResampler.cs`](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Classic/ExeResampler.cs), [`Base64.cs`](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Util/Base64.cs), and [`ResamplerItem.cs`](https://github.com/openutau/OpenUtau/blob/2005b9670ac312aaf0bbf2b42767fb70fe9185dd/OpenUtau.Core/Classic/ResamplerItem.cs).

This version has the following gaps:

1. **Encoded pitch curve:** `ParameterParser` stores arguments after modulation as literal strings. It does not decode OpenUtau's Base64 alphabet, signed 12-bit values, or `#count#` run-length segments.
2. **Tempo argument:** OpenUtau passes a token such as `!120`. This version's pitch-curve parser recognizes its own optional `Q120` convention, not OpenUtau's `!120` token.
3. **Cutoff semantics:** this version's ninth positional field is a non-negative `end_blank_ms` duration. OpenUtau passes a signed cutoff value; negative values have from-end semantics. The two meanings are not interchangeable.
4. **Host process behavior:** no real OpenUtau process has launched this executable in the verification environment. Path quoting, cancellation, repeated render calls, and cache behavior still need host tests.

The current `ResynthUTAU.yaml` exposes the project's expression controls, but the manifest alone does not provide the missing argument decoder or make the executable fully compatible.

## Safe next integration milestones

1. Add a dedicated OpenUtau argument adapter without changing the existing standalone positional syntax.
2. Decode the exact signed 12-bit Base64/run-length representation and test it against OpenUtau's reference vectors.
3. Parse the `!tempo` token and map the decoded pitch values onto the correct tempo grid.
4. Implement OpenUtau's signed cutoff rules separately from the project's end-blank duration.
5. Add command-line fixtures copied from OpenUtau's current `ExeResampler` argument construction and verify the output path and duration.
6. Launch OpenUtau Classic with the packaged binary on Windows and Linux, then validate several voicebanks, pitch curves, note transitions, and cancellation/re-render behavior.

## Manifest note

`ResynthUTAU.yaml` is included as an experimental expression-menu manifest. The listed controls follow ResynthUTAU's own documented scales; they do not claim to reproduce proprietary or other resampler flag scales. Remove or disable the manifest for builds intended only for standalone CLI use.
