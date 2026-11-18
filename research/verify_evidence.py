#!/usr/bin/env python3
"""Strict acceptance audit for native 3.1.2 experiments.

Does not decide which backend is faster. Validates *that* complete experiments
were run on actual executables, without replacing missing data with examples.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise ValueError(f"no header in {path}")
        return list(reader)


def verify(directory: Path, require_memory: bool = True) -> dict:
    manifest_file = directory / "manifest.json"
    manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
    if manifest.get("schema") != "leaderboardsystem-3.1.2-run-manifest-v1":
        raise ValueError("unsupported run manifest schema")
    if manifest.get("smoke"):
        raise ValueError("CI smoke results cannot establish research conclusions")
    if not manifest.get("git_revision") or manifest.get("git_dirty") is not False:
        raise ValueError("evidence must be from a clean, identified git revision")
    for executable, digest in manifest.get("native_executables", {}).items():
        if not isinstance(digest, str) or len(digest) != 64:
            raise ValueError("missing native binary hash: " + executable)
    if not all(manifest.get("native_executables", {}).get(k) for k in
               ("ranking_sha256", "season_sha256")):
        raise ValueError("ranking and season executable hashes required")
    config = manifest["config"]
    if int(config["ranking_repetitions"]) < 30 or int(config["independent_seeds"]) < 10 or \
       int(config["samples_per_seed"]) < 1000:
        raise ValueError("full protocol requires >=30 ranking repetitions, >=10 seeds, >=1000 samples")
    artifact_hashes = manifest.get("artifacts", {})
    if not artifact_hashes:
        raise ValueError("no archived artifacts")
    for name, expected in artifact_hashes.items():
        if Path(name).is_absolute() or ".." in Path(name).parts:
            raise ValueError("unsafe artifact path")
        observed = directory / name
        if not observed.is_file() or _sha256(observed) != expected:
            raise ValueError("missing or modified experiment artifact: " + name)

    nrows = 0
    ranking_sizes = [int(n) for n in config["ranking_sizes"]]
    league_sizes = [int(n) for n in config["league_sizes"]]
    if not {8, 10, 12, 1000, 10000}.issubset(ranking_sizes) or not {8, 10, 12}.issubset(league_sizes):
        raise ValueError("complete protocol requires realistic league sizes and synthetic scaling")
    if not (manifest.get("cmake_configuration") or {}).get("CMAKE_BUILD_TYPE", "").lower() == "release":
        raise ValueError("full scientific measurements must use a recorded Release build")
    for n in ranking_sizes:
        name = f"ranking-{n}.csv"
        if name not in artifact_hashes:
            raise ValueError("ranking workload missing: " + name)
        rows = _rows(directory / name)
        if not rows:
            raise ValueError("empty ranking benchmark: " + name)
        seen: dict[tuple, set[int]] = defaultdict(set)
        for row in rows:
            phase = row["phase"]
            backend = row["backend"]
            if int(row["count"]) != n or int(row["operations"]) <= 0:
                raise ValueError("invalid ranking dimension")
            if not all(math.isfinite(float(row[x])) and float(row[x]) >= 0
                       for x in ("wall_seconds", "cpu_seconds")):
                raise ValueError("invalid ranking duration")
            seen[(backend, phase)].add(int(row["replication"]))
        expected_reps = set(range(int(config["ranking_repetitions"])))
        for required in (("heapsort", "sort"), ("avl", "construct"),
                         ("heapsort", "get_and_select"), ("avl", "get_and_select"),
                         ("sorted_array", "update"), ("avl", "update"),
                         ("sorted_array", "delete"), ("avl", "delete"),
                         ("sorted_array", "insert"), ("avl", "insert")):
            if seen.get(required) != expected_reps:
                raise ValueError(f"missing ranking repetitions for {n} {required}")
        nrows += len(rows)

    pairs = 0
    for n in league_sizes:
        for i in range(int(config["independent_seeds"])):
            seed = int(config["base_seed"]) + i
            names = {backend: f"league-{n}-seed-{seed}-{backend}" for backend in ("heap", "avl")}
            observations = {}
            for backend, root in names.items():
                obs = f"{root}.observations.csv"
                summary = f"{root}.summary.csv"
                stages = f"{root}.stages.csv"
                if any(key not in artifact_hashes for key in (obs, summary, stages)):
                    raise ValueError("missing league results: " + root)
                obs_rows = _rows(directory / obs)
                summary_rows = _rows(directory / summary)
                stage_rows = _rows(directory / stages)
                if not obs_rows or not summary_rows or not stage_rows:
                    raise ValueError("empty league evidence: " + root)
                team_ids = {row["team_uuid"] for row in summary_rows}
                if len(team_ids) != int(n):
                    raise ValueError("team count mismatch: " + root)
                sample_ids = {int(row["sample"]) for row in obs_rows}
                samples = int(config["samples_per_seed"])
                if sample_ids != set(range(1, samples + 1)) or len(obs_rows) != samples * int(n):
                    raise ValueError("missing sampled teams or observations: " + root)
                by_sample: dict[int, set[str]] = defaultdict(set)
                for row in obs_rows:
                    sid = int(row["sample"])
                    team_id = row["team_uuid"]
                    if team_id not in team_ids or team_id in by_sample[sid]:
                        raise ValueError("duplicate or unknown observation: " + root)
                    by_sample[sid].add(team_id)
                if any(len(ids) != int(n) for ids in by_sample.values()):
                    raise ValueError("incomplete sample: " + root)
                for row in summary_rows:
                    if int(row["accepted"]) != samples or int(row["completion"]) != 0 or \
                       int(row["seed"]) != seed:
                        raise ValueError("incomplete or mismatched Monte Carlo run: " + root)
                required_phases = {"monte_full", "generation", "resolution_including_ranking", "rank_access"}
                if not required_phases <= {row["phase"] for row in stage_rows}:
                    raise ValueError("missing stage timings: " + root)
                observations[backend] = (directory / obs).read_bytes()
            if observations["heap"] != observations["avl"]:
                raise ValueError(f"AVL/heapsort results differ for n={n}, seed={seed}")
            pairs += 1
    if require_memory:
        records = manifest.get("massif_profiles", {})
        requested = set()
        for n in config.get("memory_profile_ranking_sizes", []):
            for backend in ("heap", "avl"):
                requested.add(f"ranking-{int(n)}-{backend}")
        for n in config.get("memory_profile_league_sizes", []):
            for backend in ("heap", "avl"):
                requested.add(f"league-{int(n)}-{backend}")
        if not requested or not {"ranking-8-heap", "ranking-8-avl",
                                      "ranking-1000-heap", "ranking-1000-avl",
                                      "league-8-heap", "league-8-avl"} <= requested or not requested <= set(records):
            raise ValueError("complete Massif evidence is required for declared memory workloads")
        for name in requested:
            m = records[name]
            if m.get("profiler") != "valgrind-massif" or \
               m.get("peak_heap_including_allocator_overhead_bytes", -1) < 0 or \
               m.get("snapshot_count", 0) <= 0:
                raise ValueError("invalid Massif memory result: " + name)
            for entry in (f"memory/{name}/massif.out", f"memory/{name}/stdout.csv",
                          f"memory/{name}/valgrind.log", f"memory/{name}/memory.json"):
                if entry not in artifact_hashes:
                    raise ValueError("missing archived memory evidence: " + entry)
        for n in config.get("memory_profile_ranking_sizes", []):
            names = [directory / f"memory/ranking-{n}-{backend}/stdout.csv" for backend in ("heap", "avl")]
            values = [_rows(path) for path in names]
            if any(len(v) != 1 for v in values) or values[0][0]["checksum"] != values[1][0]["checksum"]:
                raise ValueError("memory ranking backends disagree")
        for n in config.get("memory_profile_league_sizes", []):
            left = directory / f"memory/league-{n}-heap/observations.csv"
            right = directory / f"memory/league-{n}-avl/observations.csv"
            if left.read_bytes() != right.read_bytes():
                raise ValueError("memory season backends disagree")
    return {"ranking_sizes": ranking_sizes, "ranking_rows": nrows,
            "independent_seed_pairs": pairs,
            "memory_workloads_verified": len(manifest.get("massif_profiles", {})) if require_memory else None,
            "status": "evidence_integrity_verified_not_a_performance_conclusion"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--allow-missing-memory", action="store_true",
                        help="For development only; NOT full PR B acceptance")
    args = parser.parse_args()
    report = verify(args.directory, not args.allow_missing_memory)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
