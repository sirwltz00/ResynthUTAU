# Third-party notices

## Runtime dependencies

ResynthUTAU 0.6.1 has no third-party runtime DSP, FFT, or audio-library dependency. The executable is implemented using the C++ standard library and repository-owned code. The FFT, pitch analysis, harmonic analysis, spectral-envelope estimation, aperiodicity estimation, synthesis, and WAV I/O code are maintained in this repository.

CMake is required to configure and build the project, but it is a build-system dependency rather than a linked runtime library. Python 3 is optional and is used only by `scripts/benchmark.py`.

## GitHub Actions

The GitHub Actions workflows use the following actions, pinned to immutable commit references:

- `actions/checkout` — MIT License; pinned to `11bd71901bbe5b1630ceea73d27597364c9af683` (v4.2.2).
- `actions/upload-artifact` — MIT License; pinned to `ea165f8d65b6e75b540449e92b4886f43607fa02` (v4.6.2).
- `actions/download-artifact` — MIT License; pinned to `fa0a91b85d4f404e444e00e005971372dc801d16` (v4.1.8).

These actions execute in CI; they are not included in the ResynthUTAU runtime binary.

## Assets and test material

No proprietary voicebank data, encrypted voicebank assets, commercial voice recordings, or third-party sample library is included in the source package. Automated tests generate synthetic audio in memory or temporary files. A short user-supplied sample was used for local comparison measurements but is not distributed in the source archive.
