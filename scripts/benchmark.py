#!/usr/bin/env python3
"""Measure the conventional UTAU render path without third-party packages."""

from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import tempfile
import time
import wave
from pathlib import Path


def wav_duration(path: Path) -> float:
    with wave.open(str(path), "rb") as handle:
        frames = handle.getnframes()
        rate = handle.getframerate()
    if rate <= 0:
        raise RuntimeError(f"Invalid sample rate in {path}")
    return frames / rate


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="ResynthUTAU or ResynthUTAU.exe")
    parser.add_argument("input", type=Path, help="Input PCM WAV sample")
    parser.add_argument("--runs", type=int, default=3, help="Measured repetitions (default: 3)")
    parser.add_argument("--json", type=Path, help="Optional path to write the measurements")
    args = parser.parse_args()

    executable = args.executable.resolve()
    source = args.input.resolve()
    if not executable.is_file():
        parser.error(f"Executable not found: {executable}")
    if not source.is_file():
        parser.error(f"Input WAV not found: {source}")
    if args.runs < 1 or args.runs > 50:
        parser.error("--runs must be between 1 and 50")

    duration = wav_duration(source)
    elapsed_seconds: list[float] = []
    with tempfile.TemporaryDirectory(prefix="resynthutau-bench-") as temp_dir:
        output = Path(temp_dir) / "benchmark-output.wav"
        command = [str(executable), str(source), str(output), "100", "100"]
        for run_index in range(args.runs):
            start = time.perf_counter()
            completed = subprocess.run(command, capture_output=True, text=True, check=False)
            elapsed = time.perf_counter() - start
            if completed.returncode != 0:
                raise RuntimeError(
                    f"Render {run_index + 1} failed with exit code {completed.returncode}:\n"
                    f"{completed.stderr}\n{completed.stdout}"
                )
            if not output.is_file():
                raise RuntimeError("Renderer returned success but did not create its WAV output")
            elapsed_seconds.append(elapsed)

    median = statistics.median(elapsed_seconds)
    result = {
        "input": str(source),
        "audio_duration_seconds": duration,
        "runs": len(elapsed_seconds),
        "elapsed_seconds": elapsed_seconds,
        "median_elapsed_seconds": median,
        "real_time_factor": median / duration if duration > 0.0 else None,
        "speed_multiple_of_realtime": duration / median if median > 0.0 else None,
        "invocation": "<executable> <input.wav> <output.wav> 100 100",
    }
    print(json.dumps(result, indent=2))
    if args.json:
        args.json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
