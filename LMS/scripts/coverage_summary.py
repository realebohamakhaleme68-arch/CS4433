#!/usr/bin/env python3
"""Summarise GCC gcov line coverage for the LMS service/domain layer."""
from __future__ import annotations

import glob
import os
import re
import subprocess
import sys
from pathlib import Path

project = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else project / "build-coverage"
pattern = str(build / "CMakeFiles" / "lms_core.dir" / "**" / "*.gcno")
gcno_files = glob.glob(pattern, recursive=True)
if not gcno_files:
    raise SystemExit(f"No GCC coverage files found under {build}; configure with -DLMS_ENABLE_COVERAGE=ON and run tests first.")

run = subprocess.run(
    ["gcov", "-b", "-c", *gcno_files], cwd=build,
    check=True, capture_output=True, text=True,
)
blocks: dict[str, tuple[int, int]] = {}
current: str | None = None
for line in run.stdout.splitlines():
    match = re.match(r"File '(.+)'", line)
    if match:
        current = match.group(1)
        continue
    match = re.match(r"Lines executed:([0-9.]+)% of (\d+)", line)
    if match and current:
        if current.endswith(".cpp"):
            total = int(match.group(2))
            covered = round(total * float(match.group(1)) / 100)
            blocks[current] = (covered, total)
        current = None

scopes = [
    ("Business logic — service + domain", ("/src/service/", "/src/domain/")),
    ("Utilities — reported separately", ("/src/util/",)),
]
output = []
for label, markers in scopes:
    files = {name: values for name, values in blocks.items() if any(marker in name for marker in markers)}
    covered = sum(values[0] for values in files.values())
    total = sum(values[1] for values in files.values())
    if total == 0:
        raise SystemExit(f"No source coverage data found for scope: {label}")
    output.append(f"{label}: {covered}/{total} = {covered / total * 100:.2f}%")
    for name, (hit, count) in sorted(files.items()):
        output.append(f"  {name.split('/src/')[-1]}: {hit}/{count} = {hit / count * 100:.2f}%")

result = "\n".join(output) + "\n"
result_path = project / "test-results" / "coverage-summary.txt"
result_path.parent.mkdir(parents=True, exist_ok=True)
result_path.write_text(result, encoding="utf-8")
print(result, end="")
