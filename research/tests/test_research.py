"""Deterministic no-network scientific analysis and pseudonymization regression."""
from __future__ import annotations
import csv
import json
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from analyze import analyze, bootstrap_seed_means, load_batch, kendall_tie_aware
from import_league import normalize
from import_workbook import convert
from summarize_benchmarks import summarize


def make_batch(directory: Path, seed: int = 5489) -> tuple[Path, Path]:
    directory.mkdir(parents=True, exist_ok=True)
    observation = directory / "observations.csv"
    summary = directory / "summary.csv"
    obs = []
    for sample, ranks in [(1, (1, 1)), (2, (2, 1)), (3, (1, 2))]:
        for i, rank in enumerate(ranks):
            obs.append({"sample": sample, "attempt": sample,
                        "evaluated_directed_fingerprint": f"{sample:016x}",
                        "team_uuid": f"u-{i}", "sporting_rank": rank,
                        "championship_points": 10 + sample - i,
                        "fantasy_points_tenths": 6600,
                        "goal_difference": sample - i})
    pd.DataFrame(obs).to_csv(observation, index=False)
    pd.DataFrame([{ "team_uuid": f"u-{i}", "seed": seed,
                    "baseline_rank": i + 1, "baseline_points": 10 - i,
                    "accepted": 3, "attempts": 3, "sampling_mode": 0,
                    "uniqueness_tracked": 1, "duplicate_attempts": 0,
                    "unique_calendars": 3, "completion": 0}
                  for i in range(2)]).to_csv(summary, index=False)
    return observation, summary


def test_summary_and_confidence_intervals(tmp_path: Path) -> None:
    pairs = [make_batch(tmp_path / "a", 5489),
             make_batch(tmp_path / "b", 5490)]
    result = analyze(pairs, tmp_path / "output", 61)
    assert result["seed_replicates"] == 2
    assert (tmp_path / "output" / "rank_displacement.png").is_file()
    assert result["metrics"]["observed_fingerprint_diversity"]["mean"] == pytest.approx(1)
    assert result["metrics"]["mean_absolute_rank_displacement"]["ci_low"] is not None
    assert json.loads((tmp_path / "output" / "summary.json").read_text()) == result
    with pytest.raises(ValueError, match="repeated seed"):
        analyze([pairs[0], pairs[0]], tmp_path / "duplicate", 61)


def test_incomplete_or_duplicate_observation(tmp_path: Path) -> None:
    observation, summary = make_batch(tmp_path)
    frame = pd.read_csv(observation)
    frame = frame.iloc[:-1]
    frame.to_csv(observation, index=False)
    with pytest.raises(ValueError, match="incomplete"):
        load_batch(observation, summary)


def test_tie_aware_kendall_and_bootstrap() -> None:
    assert kendall_tie_aware(np.array([1, 1, 3]), np.array([1, 1, 3])) == pytest.approx(1)
    assert kendall_tie_aware(np.array([1, 1]), np.array([1, 1])) is None
    first = bootstrap_seed_means(np.array([1.0, 2.0, 3.0]), 543)
    assert first == bootstrap_seed_means(np.array([1.0, 2.0, 3.0]), 543)
    assert first["ci_low"] <= first["mean"] <= first["ci_high"]
    assert bootstrap_seed_means(np.array([1.0]), 7)["ci_low"] is None


def test_ranking_summary(tmp_path: Path) -> None:
    raw = tmp_path / "results.csv"
    pd.DataFrame([
        dict(experiment="ranking", dataset="synthetic", backend="avl",
             phase="construct", count=8, operations=8, replication=i,
             seed=5489, wall_seconds=t, cpu_seconds=t)
        for i, t in enumerate((0.0008, 0.0012))]).to_csv(raw, index=False)
    frame = summarize([raw], tmp_path / "summarized.csv")
    assert frame.iloc[0]["replications"] == 2
    assert frame.iloc[0]["median_ns_op"] == pytest.approx(125000)


def test_import_pseudonymization(tmp_path: Path) -> None:
    source = tmp_path / "raw"
    source.mkdir()
    (source / "teams.csv").write_text("team_id,name\nalice,Private Alice\nbob,Private Bob\n")
    (source / "fixtures.csv").write_text("round,matchday,home_id,away_id\n1,1,alice,bob\n")
    (source / "scores.csv").write_text("matchday,team_id,points_tenths,status\n"
                                       "1,alice,660,FINAL\n1,bob,700,FINAL\n")
    target = tmp_path / "private"
    result = normalize(source, target, b"test-key" * 8)
    data = (target / "competition.tsv").read_text()
    assert result["team_count"] == 2
    assert result["round_count"] == pytest.approx(1)
    assert "alice" not in data and "Private" not in data and "bob" not in data
    assert data.count("FIXTURE") == 1 and data.count("SCORE") == 2
    assert normalize(source, target, b"test-key" * 8) == result
    (source / "scores.csv").write_text("matchday,team_id,points_tenths,status\n1,alice,660,FINAL\n")
    with pytest.raises(ValueError, match="missing score"):
        normalize(source, target, b"test-key" * 8)


def test_existing_workbook_adapter(tmp_path: Path) -> None:
    fixture = tmp_path / "workbook.tsv"
    fixture.write_text(
        "# 3.1.0 example header\n"
        "1\t4\tFirst private name\tSecond private name\t660\t700\tF\n")
    output = tmp_path / "private"
    report = convert(fixture, output, b"research-key" * 4)
    assert report["team_count"] == 2 and report["final_scores"] == 2
    data = (output / "competition.tsv").read_text()
    assert "First private name" not in data
    assert data.count("FIXTURE") == 1 and data.count("SCORE") == 2


def test_documented_example(tmp_path: Path) -> None:
    root = Path(__file__).resolve().parents[1] / "examples"
    obs, summ = load_batch(root / "observations.csv", root / "summary.csv")
    assert len(obs) == 6 and len(summ) == 2


def test_massif_parser_reports_whole_process_heap_not_rss(tmp_path: Path) -> None:
    from memory_profile import parse_massif
    massif = tmp_path / "massif.out"
    massif.write_text("""desc: massif output
snapshot=0
time=10
mem_heap_B=300
mem_heap_extra_B=20
mem_stacks_B=0
heap_tree=empty
snapshot=1
time=50
mem_heap_B=900
mem_heap_extra_B=60
mem_stacks_B=0
heap_tree=peak
snapshot=2
time=75
mem_heap_B=100
mem_heap_extra_B=8
mem_stacks_B=0
heap_tree=empty
""")
    parsed = parse_massif(massif)
    assert parsed["peak_heap_requested_bytes"] == 900
    assert parsed["peak_heap_including_allocator_overhead_bytes"] == 960
    assert parsed["peak_heap_snapshot"] == 1
    assert parsed["peak_heap_snapshot_marked_by_massif"]
    assert parsed["snapshot_count"] == 3
    assert "whole-process" in parsed["scope"]
    massif.write_text("snapshot=0\nmem_heap_B=100\n")
    with pytest.raises(ValueError, match="incomplete"):
        parse_massif(massif)


def test_verification_rejects_smoke_missing_or_dirty_evidence(tmp_path: Path) -> None:
    from verify_evidence import verify
    (tmp_path / "manifest.json").write_text(json.dumps({
        "schema": "leaderboardsystem-3.1.2-run-manifest-v1",
        "smoke": True,
        "git_revision": "deadbeef",
        "git_dirty": False,
    }))
    with pytest.raises(ValueError, match="smoke"):
        verify(tmp_path)
    (tmp_path / "manifest.json").write_text(json.dumps({
        "schema": "leaderboardsystem-3.1.2-run-manifest-v1",
        "smoke": False,
        "git_revision": "deadbeef",
        "git_dirty": True,
    }))
    with pytest.raises(ValueError, match="clean"):
        verify(tmp_path)


def test_strict_validation_requires_complete_research_profile(tmp_path: Path) -> None:
    from verify_evidence import verify
    (tmp_path / "manifest.json").write_text(json.dumps({
        "schema": "leaderboardsystem-3.1.2-run-manifest-v1",
        "smoke": False,
        "git_revision": "a" * 40,
        "git_dirty": False,
        "native_executables": {
            "ranking_sha256": "a" * 64,
            "season_sha256": "b" * 64,
            "memory_runner_sha256": "c" * 64,
        },
        "config": {"ranking_repetitions": 1, "independent_seeds": 1,
                   "samples_per_seed": 10},
        "artifacts": {"fake.csv": "0" * 64},
    }))
    with pytest.raises(ValueError, match="full protocol"):
        verify(tmp_path)


def test_memory_profiler_fails_loudly_without_valgrind(tmp_path: Path, monkeypatch) -> None:
    import memory_profile
    monkeypatch.setattr(memory_profile.shutil, "which", lambda name: None)
    with pytest.raises(RuntimeError, match="Valgrind Massif is required"):
        memory_profile.profile(["/path/to/benchmark", "8", "heap", "5489"], tmp_path)
