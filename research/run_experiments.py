#!/usr/bin/env python3
"""Reproducible native benchmark launcher for milestone 3.1.2.

Research runs are never silently executed inside CTest or GitHub CI.
All generated observations are stored under git-ignored research/output/.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import os
import platform
import subprocess
import sys
import time

import psutil
from datetime import datetime, timezone
from pathlib import Path

from memory_profile import profile


def git_revision() -> str | None:
    result = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                            text=True, check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def git_dirty() -> bool | None:
    result = subprocess.run(["git", "status", "--porcelain"], capture_output=True,
                            text=True, check=False)
    return bool(result.stdout.strip()) if result.returncode == 0 else None


def compiler_version(cmake: dict[str, str]) -> str | None:
    compiler = cmake.get("CMAKE_C_COMPILER")
    if not compiler:
        return None
    try:
        result = subprocess.run([compiler, "--version"], capture_output=True,
                                text=True, check=False, timeout=15)
        return result.stdout.splitlines()[0] if result.returncode == 0 and result.stdout else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cmake_configuration(native_dir: Path) -> dict[str, str]:
    cache = native_dir / "CMakeCache.txt"
    if not cache.is_file():
        return {"warning": "no CMakeCache.txt; compiler and flags not recorded"}
    values = {}
    relevant = {"CMAKE_BUILD_TYPE", "CMAKE_C_COMPILER", "CMAKE_C_COMPILER_ID",
                "CMAKE_C_FLAGS", "CMAKE_C_FLAGS_RELEASE", "CMAKE_C_FLAGS_DEBUG",
                "LEADERBOARD_ENABLE_SANITIZERS", "LEADERBOARD_WARNINGS_AS_ERRORS",
                "LEADERBOARD_BUILD_BENCHMARKS"}
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line or ":" not in line:
            continue
        left, value = line.split("=", 1)
        key, _kind = left.split(":", 1)
        if key in relevant:
            values[key] = value
    return values


def execute(command: list[str], stdout_file: Path) -> int | None:
    """Sample process RSS; peak is a lower bound, not exact allocator peak."""
    stdout_file.parent.mkdir(parents=True, exist_ok=True)
    with stdout_file.open("w", encoding="utf-8", newline="") as stream:
        proc = subprocess.Popen(command, stdout=stream)
        try:
            monitor = psutil.Process(proc.pid)
        except psutil.Error:
            monitor = None  # Very short smoke checks can exit before inspection.
        peak = 0
        while proc.poll() is None:
            if monitor is not None:
                try:
                    peak = max(peak, monitor.memory_info().rss)
                except psutil.Error:
                    pass
            time.sleep(0.01)
        result = proc.wait()
        if result != 0:
            raise subprocess.CalledProcessError(result, command)
        return peak if peak else None


def run(config_path: Path, directory: Path, native_dir: Path, smoke: bool,
        profile_memory: bool = False) -> dict:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    ranking = native_dir / ("leaderboard_ranking_compare.exe" if os.name == "nt"
                            else "leaderboard_ranking_compare")
    season = native_dir / ("leaderboard_season_experiment.exe" if os.name == "nt"
                           else "leaderboard_season_experiment")
    memory_runner = native_dir / ("leaderboard_memory_compare.exe" if os.name == "nt"
                                   else "leaderboard_memory_compare")
    if not ranking.is_file() or not season.is_file():
        raise FileNotFoundError("build with -DLEADERBOARD_BUILD_BENCHMARKS=ON first")
    if profile_memory and not memory_runner.is_file():
        raise FileNotFoundError("build leaderboard_memory_compare for isolated Massif ranking trials")
    directory.mkdir(parents=True, exist_ok=True)
    files = []
    memory = {}
    repetitions = 1 if smoke else int(config["ranking_repetitions"])
    samples = 3 if smoke else int(config["samples_per_seed"])
    seed_count = 1 if smoke else int(config["independent_seeds"])
    counts = [8] if smoke else config["ranking_sizes"]
    seasons = [8] if smoke else config["league_sizes"]
    for n in counts:
        operation_count = min(int(n), int(config["incremental_operations"]))
        path = directory / f"ranking-{n}.csv"
        memory[path.name] = execute([str(ranking), str(n), str(repetitions),
                                     str(config["base_seed"]), str(operation_count)], path)
        files.append(path)
    # Pair both backends on exactly identical seed and competition input.
    for n in seasons:
        for i in range(seed_count):
            seed = int(config["base_seed"]) + i
            if seed > 0xFFFFFFFF:
                raise ValueError("seed exceeds uint32 range")
            for backend in (("heap", "avl") if i % 2 == 0 else ("avl", "heap")):
                root = directory / f"league-{n}-seed-{seed}-{backend}"
                obs = root.with_suffix(".observations.csv")
                summ = root.with_suffix(".summary.csv")
                stages = root.with_suffix(".stages.csv")
                memory[stages.name] = execute([str(season), str(n), str(seed), str(samples), backend,
                                               str(config["calendar_method"]), str(config["sampling_mode"]),
                                               str(obs), str(summ)], stages)
                files.extend((obs, summ, stages))
            left = (directory / f"league-{n}-seed-{seed}-heap.observations.csv").read_bytes()
            right = (directory / f"league-{n}-seed-{seed}-avl.observations.csv").read_bytes()
            if left != right:
                raise ValueError(f"backend mismatch for n={n}, seed={seed}")
    massif_profiles = {}
    if profile_memory:
        # Separate slow, instrumented *whole-process* memory trials.
        # Profiled CPU/wall times are never used as performance evidence.
        for n in config.get("memory_profile_ranking_sizes", []):
            n = int(n)
            if n not in counts:
                raise ValueError("memory ranking size not in configured workloads")
            for backend in ("heap", "avl"):
                name = f"ranking-{n}-{backend}"
                destination = directory / "memory" / name
                command = [str(memory_runner.resolve()), str(n), backend, str(config["base_seed"])]
                massif_profiles[name] = profile(command, destination)
                files.extend(destination / x for x in
                             ("massif.out", "stdout.csv", "valgrind.log", "memory.json"))
            # Both independent processes must resolve *identical* sorted data.
            heap_output = (directory / "memory" / f"ranking-{n}-heap" / "stdout.csv")
            avl_output = (directory / "memory" / f"ranking-{n}-avl" / "stdout.csv")
            def checksum(path: Path) -> str:
                with path.open(newline="", encoding="utf-8") as inp:
                    rows = list(csv.DictReader(inp))
                if len(rows) != 1 or not rows[0].get("checksum"):
                    raise ValueError(f"invalid memory ranking checksum: {path}")
                return rows[0]["checksum"]
            if checksum(heap_output) != checksum(avl_output):
                raise ValueError(f"Massif memory ranking backends disagree for n={n}")
        memory_samples = int(config.get("memory_profile_samples", 100))
        if memory_samples <= 0:
            raise ValueError("memory profiling requires positive sample count")
        for n in config.get("memory_profile_league_sizes", []):
            n = int(n)
            if n not in seasons:
                raise ValueError("memory league size not in configured workloads")
            for backend in ("heap", "avl"):
                name = f"league-{n}-{backend}"
                destination = directory / "memory" / name
                destination.mkdir(parents=True, exist_ok=True)
                command = [str(season.resolve()), str(n), str(config["base_seed"]),
                           str(memory_samples), backend,
                           str(config["calendar_method"]), str(config["sampling_mode"]),
                           str(destination / "observations.csv"),
                           str(destination / "summary.csv")]
                massif_profiles[name] = profile(command, destination)
                files.extend(destination / x for x in
                             ("massif.out", "stdout.csv", "valgrind.log", "memory.json",
                              "observations.csv", "summary.csv"))
            heap_obs = (directory / "memory" / f"league-{n}-heap" / "observations.csv")
            avl_obs = (directory / "memory" / f"league-{n}-avl" / "observations.csv")
            if heap_obs.read_bytes() != avl_obs.read_bytes():
                raise ValueError(f"Massif season backends disagree for n={n}")
    meta = {
        "schema": "leaderboardsystem-3.1.2-run-manifest-v1",
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "git_revision": git_revision(),
        "git_dirty": git_dirty(),
        "native_executables": {"ranking_sha256": sha256(ranking),
                               "season_sha256": sha256(season),
                               "memory_runner_sha256": sha256(memory_runner) if memory_runner.is_file() else None},
        "python": sys.version,
        "platform": platform.platform(),
        "processor": platform.processor(),
        "cpu_logical_count": psutil.cpu_count(logical=True),
        "memory_total_bytes": psutil.virtual_memory().total,
        "cmake_configuration": cmake_configuration(native_dir),
        "compiler_version": compiler_version(cmake_configuration(native_dir)),
        "packages": {"numpy": __import__("numpy").__version__,
                     "pandas": __import__("pandas").__version__,
                     "scipy": __import__("scipy").__version__,
                     "matplotlib": __import__("matplotlib").__version__,
                     "psutil": psutil.__version__},
        "config": config,
        "smoke": smoke,
        "commands_maintained_in": "research/run_experiments.py",
        "artifacts": {p.relative_to(directory).as_posix(): sha256(p) for p in files},
        "massif_profiles": massif_profiles,
        "sampled_peak_rss_bytes_lower_bound": memory,
        "limitations": ["Wall time and CPU time are machine dependent.",
                        "No performance threshold is enforced.",
                        "Monte Carlo samples are not assumed uniformly distributed.",
                        "RSS sampling at 10 ms intervals can miss short-lived peaks.",
                        "Massif whole-process heap peaks are available only when --profile-memory is used and Valgrind succeeds.",
                        "Massif heap peaks are not per-stage; heap profiling changes runtime and is never used for timing comparisons.",
                        "Collected stage timings are isolated public API calls,"
                        " not additive components of a timed Monte Carlo run."],
    }
    (directory / "manifest.json").write_text(json.dumps(meta, indent=2) + "\n",
                                               encoding="utf-8")
    return meta


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path,
                        default=Path("research/config/default.json"))
    parser.add_argument("--output", type=Path, default=Path("research/output/latest"))
    parser.add_argument("--native-dir", type=Path, default=Path("build-release"))
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--profile-memory", action="store_true",
                        help="Run separate full-heap Valgrind Massif trials (Linux)")
    args = parser.parse_args()
    result = run(args.config, args.output, args.native_dir, args.smoke,
                 args.profile_memory)
    print(json.dumps({"git_revision": result["git_revision"],
                      "artifacts": len(result["artifacts"]),
                      "manifest": str(args.output / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
