# OpenUtau Classic adapter test report — 0.6.1

## Scope

This report covers the implementation of the OpenUtau Classic external-resampler command-line adapter. It does not claim an OpenUtau GUI/runtime test.

## Covered behaviors

- Recognizes the 13-argument Classic command shape when argument 12 begins with `!`.
- Parses a finite positive `!tempo` value and uses the host's 96-points-per-quarter-note pitch spacing.
- Decodes two-character signed 12-bit Base64 values using OpenUtau's alphabet.
- Expands `#N#` run-length markers by repeating the preceding decoded value N additional times.
- Rejects malformed tempo tokens, invalid alphabet characters, incomplete value pairs, malformed/empty repeat markers, and payloads exceeding the configured point limit.
- Keeps the standalone UTAU-style interface and `Q<tempo>`/literal-cents syntax separate.
- Applies OpenUtau's signed cutoff mapping independently from standalone `end_blank_ms`.
- Applies decoded pitch automation to the natural TD-PSOLA pitch-mark spacing; the harmonic expression path also receives the decoded contour when selected.

## Local verification

The test suite includes adapter argument mapping, signed 12-bit values, run-length expansion, invalid payload rejection, tempo-grid configuration, and signed-cutoff output-duration coverage.

Recorded local Release test runs for this source snapshot:

- GCC 14: 3/3 CTest entries passed.
- Clang 17: 3/3 CTest entries passed.

The three CTest entries are `resynth_tests`, `cli_version`, and `cli_capabilities`. The adapter-specific checks are inside `resynth_tests`.

## Not covered

- Building the Windows executable in this Linux environment.
- Launching the executable from OpenUtau on Windows.
- Manifest loading and expression menu behavior inside OpenUtau.
- Windows path quoting, Unicode temporary paths, cancellation, parallel render requests, and cache invalidation in the host.
- Audible musical accuracy on real voicebanks and real projects.

A successful command-line adapter test is useful evidence, but it is not proof of end-to-end host compatibility.
