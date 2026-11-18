#!/usr/bin/env python3
"""Isolated Valgrind Massif runs for reproducible *whole-process* heap peaks.

Massif measures allocations through its instrumented allocator, including
third-party C containers, unlike LeaderboardSystem's internal wrapper counters.
It is Linux-only for the supported research protocol. Do not use the profiled
execution's wall/CPU timings to compare backends.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path


_SNAPSHOT_START = re.compile(r"^snapshot=(\d+)$")
_INTEGER = re.compile(r"^(mem_heap_B|mem_heap_extra_B|mem_stacks_B)=(\d+)$")


def parse_massif(path: Path) -> dict:
    """Read snapshot heap, allocator overhead and stack counters verbatim.

    Values describe Massif-instrumented process memory at the recorded peaks,
    not peak RSS or total virtual memory. The maximum is over snapshots,
    including peak-labelled snapshots, and excludes untracked memory mappings.
    """
    if not path.is_file():
        raise ValueError(f"missing Massif output: {path}")
    records: list[dict] = []
    current: dict | None = None
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        match = _SNAPSHOT_START.fullmatch(line)
        if match:
            if current is not None:
                records.append(current)
            current = {"snapshot": int(match.group(1))}
            continue
        if current is None:
            continue
        match = _INTEGER.fullmatch(line)
        if match:
            current[match.group(1)] = int(match.group(2))
        elif line.startswith("heap_tree="):
            current["peak_label"] = line.split("=", 1)[1] == "peak"
    if current is not None:
        records.append(current)
    required = ("mem_heap_B", "mem_heap_extra_B", "mem_stacks_B")
    if not records or any(any(field not in rec for field in required) for rec in records):
        raise ValueError("invalid or incomplete Massif snapshots")
    max_heap = max(records, key=lambda row: row["mem_heap_B"] + row["mem_heap_extra_B"])
    max_total = max(records, key=lambda row: sum(row[field] for field in required))
    return {
        "profiler": "valgrind-massif",
        "snapshot_count": len(records),
        "peak_heap_requested_bytes": max_heap["mem_heap_B"],
        "peak_heap_allocator_overhead_bytes": max_heap["mem_heap_extra_B"],
        "peak_heap_including_allocator_overhead_bytes": (
            max_heap["mem_heap_B"] + max_heap["mem_heap_extra_B"]
        ),
        "peak_heap_snapshot": max_heap["snapshot"],
        "peak_heap_snapshot_marked_by_massif": bool(max_heap.get("peak_label")),
        "peak_including_reported_stacks_bytes": sum(max_total[field] for field in required),
        "peak_total_snapshot": max_total["snapshot"],
        "scope": "whole-process Massif allocator events, not per-stage or total RSS",
        "limitations": [
            "Massif default heap tracking excludes uninstrumented memory mappings",
            "Allocator overhead is Massif-estimated; results include executable/runtime allocations",
            "Profiled wall and CPU times must never be used for backend timing comparisons",
        ],
    }


def profile(command: list[str], destination: Path) -> dict:
    """Run the unmodified executable under Massif, persist complete evidence."""
    executable = shutil.which("valgrind")
    if executable is None:
        raise RuntimeError("Valgrind Massif is required for full heap profiling; "
                           "run the research memory protocol on Linux with valgrind installed")
    destination.mkdir(parents=True, exist_ok=True)
    massif = destination / "massif.out"
    stdout = destination / "stdout.csv"
    stderr = destination / "valgrind.log"
    arguments = [executable, "--tool=massif", "--stacks=no", "--time-unit=B",
                 "--max-snapshots=2000", "--detailed-freq=1",
                 "--massif-out-file=" + str(massif.resolve()), *command]
    with stdout.open("wb") as captured, stderr.open("wb") as diagnostic:
        result = subprocess.run(arguments, stdout=captured, stderr=diagnostic, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Massif run failed (exit {result.returncode}); see {stderr}")
    parsed = parse_massif(massif)
    version = subprocess.run([executable, "--version"], capture_output=True,
                             text=True, check=True).stdout.strip()
    parsed.update({"valgrind_version": version, "command": command,
                   "massif_path": str(massif), "stdout_path": str(stdout),
                   "stderr_path": str(stderr)})
    (destination / "memory.json").write_text(json.dumps(parsed, indent=2) + "\n",
                                               encoding="utf-8")
    return parsed


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER,
                        help="-- /path/to/benchmark arg ...")
    args = parser.parse_args()
    command = args.command[1:] if args.command and args.command[0] == "--" else args.command
    if not command:
        parser.error("a benchmark executable and its arguments are required")
    result = profile(command, args.output)
    print(json.dumps({"peak_heap_bytes": result["peak_heap_including_allocator_overhead_bytes"],
                      "profile": result["massif_path"]}, indent=2))


if __name__ == "__main__":
    main()
