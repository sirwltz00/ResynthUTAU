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

The `-` is the empty-flags placeholder. See [UTAU_COMPATIBILITY.md](UTAU_COMPATIBILITY.md) for both positional layouts. OpenUtau Classic's 13-argument external-resampler call is detected separately and includes its `!tempo` and encoded pitch-curve fields. Local adapter tests do not establish successful launch or rendering inside the OpenUtau GUI.

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

A workflow result is only considered successful after the GitHub Actions run itself reports success. Local tests do not imply that a remote workflow ran. The current source includes `ResynthUTAU.yaml` in release packages as the matching expression manifest. Manifest discovery and expression-menu behavior still require a check inside OpenUtau.


## Windows x64 package (MSVC)

On Windows 10/11 with Visual Studio 2022 (Desktop development with C++) and CMake installed, run the PowerShell packaging script from the repository root:

```powershell
.\scripts\build-windows-x64.ps1
```

The script configures the x64 MSVC generator, builds Release, runs CTest, packages `ResynthUTAU.exe`, `ResynthUTAU.yaml`, an OpenUtau install note, and the documentation, then writes a SHA-256 checksum. The CMake project uses the static MSVC runtime to avoid requiring the Visual C++ runtime DLL on the target machine.

The dedicated `.github/workflows/windows-x64-package.yml` workflow builds on the pinned `windows-2022` runner with the Visual Studio 2022 x64 generator. It runs CTest, checks the executable's version and capability output, packages the EXE with the matching YAML manifest and install notes, calculates a SHA-256 checksum, and uploads both files as a GitHub Actions artifact named `ResynthUTAU-windows-x64` (30-day retention).

### Run the Windows build on GitHub

1. Push this repository, including `.github/workflows/windows-x64-package.yml`, to GitHub.
2. To build automatically, push a commit that changes one of the workflow's watched source, test, manifest, script, or documentation paths. The workflow also runs for pull requests that change those paths.
3. To build manually, first merge the workflow file into the repository's default branch (GitHub requires that for `workflow_dispatch`). Then open the **Actions** tab, select **Windows x64 package**, choose the branch/ref to build, and select **Run workflow**. A push of the watched files can also trigger it on a non-default branch without manual dispatch.
4. Open the completed workflow run. Under **Artifacts**, download `ResynthUTAU-windows-x64`.
5. Extract the downloaded artifact ZIP. It contains `ResynthUTAU-windows-x64.zip` and `SHA256SUMS.txt`. The inner ZIP is the installable Windows x64 package.

The workflow uses `windows-2022`, `Visual Studio 17 2022`, and `-A x64`, so the output is built for 64-bit Windows with MSVC. The artifact is retained for 30 days; download it before it expires. The `.github/workflows/release.yml` workflow also builds Windows x64 for tagged/manual releases. A Windows PE executable must come from one of those Windows builds; a Linux build cannot be relabeled as a Windows executable.
