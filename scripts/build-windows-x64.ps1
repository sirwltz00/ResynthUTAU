$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

$buildDir = Join-Path $repoRoot 'build-windows-x64'
$stageDir = Join-Path $repoRoot 'dist/ResynthUTAU-windows-x64'
$zipPath = Join-Path $repoRoot 'dist/ResynthUTAU-windows-x64.zip'
$checksumPath = Join-Path $repoRoot 'dist/SHA256SUMS.txt'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake was not found. Install CMake and Visual Studio 2022 with Desktop development with C++.'
}

Invoke-Checked 'cmake' @(
    '-S', '.', '-B', $buildDir,
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DRESYNTHUTAU_BUILD_TESTS=ON',
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded'
)
Invoke-Checked 'cmake' @('--build', $buildDir, '--config', 'Release', '--parallel')
Invoke-Checked 'ctest' @('--test-dir', $buildDir, '-C', 'Release', '--output-on-failure')

$exePath = Join-Path $buildDir 'Release/ResynthUTAU.exe'
if (-not (Test-Path $exePath -PathType Leaf)) {
    throw "Expected x64 executable was not produced: $exePath"
}

$version = & $exePath --version
if ($LASTEXITCODE -ne 0 -or (($version -join ' ') -notmatch 'ResynthUTAU 0\.6\.1')) {
    throw "Executable version check failed: $version"
}
$capabilities = & $exePath --capabilities
if ($LASTEXITCODE -ne 0 -or (($capabilities -join "`n") -notmatch 'openutau_classic')) {
    throw 'Executable capabilities check failed or OpenUtau Classic metadata is missing.'
}

if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $zipPath) | Out-Null

Copy-Item $exePath, 'ResynthUTAU.yaml', 'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md' -Destination $stageDir
Get-ChildItem 'docs' -Filter '*.md' -File | Copy-Item -Destination $stageDir

@'
ResynthUTAU 0.6.1 — Windows x64 / OpenUtau Classic

INSTALLATION
1. Extract this ZIP.
2. In OpenUtau, select the CLASSIC renderer.
3. Install ResynthUTAU.exe through Tools > Install Wavtool/Resampler (.exe),
   choosing the resampler option; or copy ResynthUTAU.exe and ResynthUTAU.yaml
   into the Resamplers folder.
4. Select ResynthUTAU as the Classic renderer's resampler.
5. Configure the wavtool separately (for example, simple or convergence).

This is an external Classic command-line resampler, not a native OpenUtau renderer.
Automated tests cover the adapter protocol but do not launch OpenUtau; real host
compatibility still needs a test in the target OpenUtau installation.
'@ | Set-Content -Encoding UTF8 (Join-Path $stageDir 'OPENUTAU-INSTALL.txt')

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $stageDir '*') -DestinationPath $zipPath -CompressionLevel Optimal
$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash
"$hash  ResynthUTAU-windows-x64.zip" | Set-Content -Encoding ASCII $checksumPath

Write-Host "Windows x64 package: $zipPath"
Write-Host "SHA-256: $hash"
