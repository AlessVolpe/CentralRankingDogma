#!/usr/bin/env python3
"""3.1.2 descriptive analysis. No uniform-calendar inference is implied.

Inputs are the exact native Monte Carlo observation and summary CSV schemas.
Independently seeded *batches* are the replication units for interval estimates.
The same historical baseline must occur in every batch.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy import stats

OBS_REQUIRED = {"sample", "attempt", "evaluated_directed_fingerprint", "team_uuid",
                "sporting_rank", "championship_points", "fantasy_points_tenths",
                "goal_difference"}
SUMMARY_REQUIRED = {"team_uuid", "seed", "baseline_rank", "baseline_points",
                    "accepted", "attempts", "sampling_mode", "uniqueness_tracked",
                    "duplicate_attempts", "unique_calendars", "completion"}


def load_batch(observation: Path, summary: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    obs = pd.read_csv(observation, dtype={"team_uuid": "string",
                                         "evaluated_directed_fingerprint": "string"})
    info = pd.read_csv(summary, dtype={"team_uuid": "string"})
    if not OBS_REQUIRED <= set(obs) or not SUMMARY_REQUIRED <= set(info):
        raise ValueError("CSV schema does not match native 3.1.1 output")
    if info.empty or info.team_uuid.duplicated().any():
        raise ValueError("missing/duplicated team summary")
    for col in ("accepted", "seed", "attempts", "sampling_mode", "completion"):
        if info[col].nunique(dropna=False) != 1:
            raise ValueError(f"inconsistent metadata within summary: {col}")
    if (info.completion != 0).any() or info.accepted.nunique() != 1:
        raise ValueError("incomplete or inconsistent Monte Carlo batch")
    count = int(info.accepted.iloc[0])
    if count < 1 or len(obs) != len(info) * count:
        raise ValueError("incomplete observations or zero accepted samples")
    if obs.duplicated(["sample", "team_uuid"]).any() or obs["sample"].nunique() != count:
        raise ValueError("missing or duplicate sample/team records")
    if sorted(obs["sample"].unique().tolist()) != list(range(1, count + 1)):
        raise ValueError("non-contiguous sample indices")
    if set(obs.team_uuid) != set(info.team_uuid):
        raise ValueError("team identities differ between observation and summary")
    if (obs.groupby("sample").team_uuid.nunique() != len(info)).any():
        raise ValueError("incomplete team set in a sample")
    if (obs.groupby("sample").evaluated_directed_fingerprint.nunique() != 1).any():
        raise ValueError("mixed calendar fingerprint within sample")
    if ((obs.sporting_rank < 1) | (obs.sporting_rank > len(info))).any():
        raise ValueError("invalid sporting rank")
    rank = {str(x.team_uuid): int(x.baseline_rank) for x in info.itertuples(index=False)}
    point = {str(x.team_uuid): int(x.baseline_points) for x in info.itertuples(index=False)}
    obs["baseline_rank"] = obs.team_uuid.map(rank)
    obs["baseline_points"] = obs.team_uuid.map(point)
    return obs, info


def kendall_tie_aware(original: np.ndarray, sample: np.ndarray) -> float | None:
    """Kendall tau-b handles shared sporting ranks; undefined constant arrays => null."""
    val = stats.kendalltau(original, sample, variant="b").statistic
    return float(val) if np.isfinite(val) else None


def lag_one(x: np.ndarray) -> float | None:
    if x.size < 3 or np.std(x[:-1]) == 0 or np.std(x[1:]) == 0:
        return None
    result = np.corrcoef(x[:-1], x[1:])[0, 1]
    return float(result) if np.isfinite(result) else None


def batch_metrics(obs: pd.DataFrame, info: pd.DataFrame) -> tuple[dict, pd.DataFrame]:
    teams = info.team_uuid.tolist()
    original = info.set_index("team_uuid").loc[teams, "baseline_rank"].to_numpy(dtype=int)
    grouped = obs.groupby("sample", sort=True)
    ties = []
    changes = []
    rank_displacements = []
    winner_run = []
    for _, frame in grouped:
        frame = frame.set_index("team_uuid").loc[teams]
        sample = frame.sporting_rank.to_numpy(dtype=int)
        ties.append(kendall_tie_aware(original, sample))
        changes.append(float((frame.championship_points - frame.baseline_points).abs().mean()))
        rank_displacements.append(float(np.mean(np.abs(sample - original))))
        # First team uses UUID-sorted identity as a stable observable for chain diagnostics.
        winner_run.append(float(sample[0] == 1))
    calendar_fingerprints = obs.groupby("sample", sort=True).evaluated_directed_fingerprint.first()
    # Fingerprints are 64-bit diagnostics, not collision-proof identity certificates.
    unique_count = int(calendar_fingerprints.nunique())
    accepted = int(info.accepted.iloc[0]); attempts = int(info.attempts.iloc[0])
    stats_out = {
        "seed": int(info.seed.iloc[0]), "sampling_mode": int(info.sampling_mode.iloc[0]),
        "accepted": accepted, "attempts": attempts,
        "acceptance_fraction": accepted / attempts if attempts else None,
        "observed_fingerprint_diversity": unique_count / accepted,
        "mean_absolute_points_change": float(np.mean(changes)),
        "mean_absolute_rank_displacement": float(np.mean(rank_displacements)),
        "mean_kendall_tau_b_defined": (float(np.mean([t for t in ties if t is not None]))
                                      if any(t is not None for t in ties) else None),
        "undefined_kendall_samples": sum(t is None for t in ties),
        "lag1_first_team_first_place_indicator": lag_one(np.asarray(winner_run)),
        "reported_exact_duplicate_attempts": (None if pd.isna(info.duplicate_attempts.iloc[0])
                                               else int(info.duplicate_attempts.iloc[0])),
    }
    team_metrics = obs.groupby("team_uuid", sort=True).agg(
        first_place_frequency=("sporting_rank", lambda x: (x == 1).mean()),
        mean_rank=("sporting_rank", "mean"),
        mean_points=("championship_points", "mean"),
        mean_goal_difference=("goal_difference", "mean"),
    ).reset_index()
    rank_distribution = (obs.groupby(["team_uuid", "sporting_rank"], sort=True)
                         .size().unstack(fill_value=0))
    for rank in range(1, len(info) + 1):
        team_metrics[f"rank_{rank}_frequency"] = (team_metrics.team_uuid.map(
            rank_distribution.get(rank, pd.Series(dtype=float))) / accepted).fillna(0.0)
    team_metrics["seed"] = stats_out["seed"]
    return stats_out, team_metrics


def bootstrap_seed_means(values: np.ndarray, seed: int, resamples: int = 5000) -> dict:
    if values.size < 2:
        return {"mean": float(np.mean(values)), "ci_low": None, "ci_high": None,
                "unit": "independent_seed", "replications": int(values.size)}
    rng = np.random.default_rng(seed)
    draws = rng.choice(values, size=(resamples, len(values)), replace=True).mean(axis=1)
    return {"mean": float(values.mean()),
            "ci_low": float(np.quantile(draws, .025)),
            "ci_high": float(np.quantile(draws, .975)),
            "unit": "independent_seed", "replications": int(values.size)}


def analyze(pairs: list[tuple[Path, Path]], output: Path, bootstrap_seed: int) -> dict:
    if not pairs:
        raise ValueError("at least one (observations, summary) pair is required")
    batch_rows, per_team = [], []
    reference = None
    seen = set()
    for observations, summary in pairs:
        obs, meta = load_batch(observations, summary)
        baseline = dict(zip(meta.team_uuid.astype(str),
                            zip(meta.baseline_rank, meta.baseline_points)))
        if reference is not None and baseline != reference:
            raise ValueError("different historical baselines cannot be pooled")
        reference = baseline
        batch, team = batch_metrics(obs, meta)
        if batch["seed"] in seen:
            raise ValueError("repeated seed is not an independent replication")
        seen.add(batch["seed"])
        batch_rows.append(batch)
        per_team.append(team)
    mode_set = {x["sampling_mode"] for x in batch_rows}
    if len(mode_set) != 1:
        raise ValueError("do not pool independent and chain sampling modes")
    frame = pd.DataFrame(batch_rows)
    team_frame = pd.concat(per_team, ignore_index=True)
    output.mkdir(parents=True, exist_ok=True)
    frame.to_csv(output / "replications.csv", index=False)
    team_frame.to_csv(output / "team_replications.csv", index=False)
    team_pooled = []
    for team_id, frame_by_id in team_frame.groupby("team_uuid", sort=True):
        item = {"team_uuid": str(team_id)}
        for column in ("first_place_frequency", "mean_rank", "mean_points"):
            item[column] = bootstrap_seed_means(frame_by_id[column].to_numpy(dtype=float),
                                                bootstrap_seed)
        team_pooled.append(item)
    (output / "team_interval_summary.json").write_text(
        json.dumps(team_pooled, indent=2) + "\n", encoding="utf-8")
    summary = {
        "scope": "descriptive sampling from configured generator, not uniform calendar probabilities",
        "sampling_mode": next(iter(mode_set)),
        "seed_replicates": len(batch_rows),
        "bootstrap_seed": bootstrap_seed,
        "metrics": {},
        "warnings": ["Draws within chain-mode runs may be autocorrelated.",
                     "Independent seeds do not imply uniform samples.",
                     "Fingerprints are diagnostics, not collision-proof identities.",
                     "Reported intervals are seed-level bootstrap intervals, not finite-calendar-population CIs."],
    }
    for key in ("acceptance_fraction", "observed_fingerprint_diversity",
                "mean_absolute_points_change", "mean_absolute_rank_displacement"):
        summary["metrics"][key] = bootstrap_seed_means(frame[key].to_numpy(dtype=float),
                                                        bootstrap_seed)
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.hist(frame["mean_absolute_rank_displacement"], bins=min(12, max(1, len(frame))))
    ax.set(xlabel="Mean absolute rank displacement per seed",
           ylabel="Seed replications", title="Calendar-sampling sensitivity")
    fig.tight_layout(); fig.savefig(output / "rank_displacement.png", dpi=160)
    plt.close(fig)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pair", nargs=2, action="append", required=True,
                        metavar=("OBS_CSV", "SUMMARY_CSV"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bootstrap-seed", type=int, default=5489)
    args = parser.parse_args()
    result = analyze([(Path(a), Path(b)) for a, b in args.pair], args.output,
                     args.bootstrap_seed)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
