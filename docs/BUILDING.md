# Building ResynthUTAU

## Requirements

- CMake 3.20 or newer.
- A C++20 compiler (MSVC 2022, GCC 11+, or Clang 14+ recommended).
- Git for cloning the repository.
- Python 3 only if you want to run the optional benchmark script.

The executable has no third-party runtime DSP or audio-library dependency. FFT, analysis, harmonic/aperiodic synthesis, phonetic classification, TD-PSOLA, and WSOLA are implemented in this repository.

## Configure, build, and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRESYNTHUTAU_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

On Windows, run these commands in a Visual Studio 2022 Developer PowerShell or another shell where CMake can locate MSVC. The executable is normally at `build/Release/ResynthUTAU.exe`. On single-configuration Linux builds, it is normally at `build/ResynthUTAU`.

## Run the positional renderer

```bash
./build/ResynthUTAU input.wav output.wav A#3 100 - 0 650 80 0 100 0
# A numeric third argument is a relative frequency percentage:
./build/ResynthUTAU input.wav output.wav 120 100 - 0 650 80 0 100 0
```

The `-` is the empty-flags placeholder. See [UTAU_COMPATIBILITY.md](UTAU_COMPATIBILITY.md) for the complete argument layout. This is a UTAU-style CLI subset, not a completed OpenUtau argument adapter; do not infer host compatibility from a successful command-line render.

## Diagnostic commands

```bash
./build/ResynthUTAU --version
./build/ResynthUTAU --capabilities
./build/ResynthUTAU --analyze path/to/sample.wav
./build/ResynthUTAU --analyze path/to/sample.wav analysis.json
./build/ResynthUTAU --resynthesize path/to/sample.wav reconstructed.wav
./build/ResynthUTAU --resynthesize path/to/sample.wav one_semitone_up.wav 1.059463094
```

On Windows, use `build/Release/ResynthUTAU.exe` in place of `./build/ResynthUTAU`.

## Optional performance benchmark

```bash
python scripts/benchmark.py ./build/ResynthUTAU voice.wav --runs 5 --json benchmark.json
```

See [PERFORMANCE.md](PERFORMANCE.md) for the benchmark method and the recorded Phase 5.1-to-Phase 6 comparison.

## GitHub Actions

`.github/workflows/ci.yml` runs on pushes, pull requests, and manual dispatch. It configures CMake, compiles, runs CTest, and uploads build artifacts on Windows and Ubuntu. `.github/workflows/release.yml` runs for `v*` tags or manual dispatch, builds/tests Windows and Linux, packages each platform, calculates SHA-256 hashes, and creates or updates a GitHub Release using `github.token` through the GitHub CLI.

A workflow result is only considered successful after the GitHub Actions run itself reports success. Local tests do not imply that a remote workflow ran. The current source includes `ResynthUTAU.yaml` in release packages as an experimental expression-menu manifest; it does not imply full OpenUtau compatibility.
