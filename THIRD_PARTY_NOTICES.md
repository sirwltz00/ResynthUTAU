# Third-party notices

ResynthUTAU Phase 1 currently uses only the C++ standard library and the CMake build system. No third-party DSP or audio library is linked into the executable.

The GitHub Actions workflows use the following maintained official actions, pinned to immutable commit references:

- `actions/checkout` — MIT License; pinned to `11bd71901bbe5b1630ceea73d27597364c9af683` (v4.2.2).
- `actions/upload-artifact` — MIT License; pinned to `ea165f8d65b6e75b540449e92b4886f43607fa02` (v4.6.2).
- `actions/download-artifact` — MIT License; pinned to `fa0a91b85d4f404e444e00e005971372dc801d16` (v4.1.8).

No proprietary voicebank data, encrypted voicebank assets, or commercial test recordings are included.
