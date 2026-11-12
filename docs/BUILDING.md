# Building ResynthUTAU

## Requirements

- CMake 3.20 or newer.
- A C++20 compiler (MSVC 2022, GCC 11+, or Clang 14+ recommended).
- Git for cloning the repository.

The runtime executable has no external library dependency. FFT, analysis, harmonic synthesis, stochastic aperiodic synthesis, phonetic classification, and WSOLA timing are implemented in the project source.

## Local build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

On Windows, run these commands in a Visual Studio 2022 Developer PowerShell or another shell where CMake can locate MSVC. The executable is normally at `build/Release/ResynthUTAU.exe`. On single-configuration Linux builds, it is normally at `build/ResynthUTAU`.

## UTAU, analysis, and diagnostic resynthesis commands

After building, the conventional positional invocation executes the Phase 4 timing/phonetics renderer. For example:

```bash
./build/ResynthUTAU input.wav output.wav A#3 100 - 0 650 80 0 100 0
# Alternatively, 120 applies a 1.2x frequency ratio
./build/ResynthUTAU input.wav output.wav 120 100 - 0 650 80 0 100 0
```

Analysis and standalone diagnostic commands:

```bash
./build/ResynthUTAU --analyze path/to/sample.wav
./build/ResynthUTAU --analyze path/to/sample.wav analysis.json
./build/ResynthUTAU --resynthesize path/to/sample.wav reconstructed.wav
./build/ResynthUTAU --resynthesize path/to/sample.wav one_semitone_up.wav 1.059463094
```

On Windows, use `build/Release/ResynthUTAU.exe` in place of `./build/ResynthUTAU`.

## GitHub Actions

`.github/workflows/ci.yml` runs on pushes, pull requests, and manual dispatch. It configures CMake, compiles, runs CTest, and uploads build artifacts on Windows and Ubuntu. `.github/workflows/release.yml` runs for `v*` tags or manual dispatch, builds/tests Windows and Linux, packages each platform, calculates SHA-256 hashes, and creates or updates a GitHub Release using `github.token` through the GitHub CLI.

The workflow files are part of the source package. A CI result is only considered successful after the GitHub Actions run itself reports success; a local build does not imply a remote workflow succeeded.
