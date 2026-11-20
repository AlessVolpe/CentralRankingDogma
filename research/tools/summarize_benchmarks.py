#!/usr/bin/env python3
"""Summarize native benchmark CSV without pretending sub-microsecond accuracy.

Results are descriptive per-replication measures, not significance claims.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd

REQUIRED = {"experiment", "dataset", "backend", "phase", "count", "operations",
            "replication", "seed", "wall_seconds", "cpu_seconds"}


def summarize(inputs: list[Path], output: Path) -> pd.DataFrame:
    tables = [pd.read_csv(p) for p in inputs]
    if any(not REQUIRED <= set(table) for table in tables):
        raise ValueError("unsupported benchmark CSV schema")
    data = pd.concat(tables, ignore_index=True)
    if data.empty or (data.operations <= 0).any() or (data.wall_seconds < 0).any():
        raise ValueError("empty/invalid benchmark data")
    data["ns_per_operation"] = data.wall_seconds * 1e9 / data.operations
    keys = ["experiment", "dataset", "backend", "phase", "count", "operations"]
    table = data.groupby(keys, dropna=False).agg(
        replications=("ns_per_operation", "size"),
        median_ns_op=("ns_per_operation", "median"),
        mean_ns_op=("ns_per_operation", "mean"),
        std_ns_op=("ns_per_operation", "std"),
        min_ns_op=("ns_per_operation", "min"),
        max_ns_op=("ns_per_operation", "max"),
    ).reset_index()
    if {"wrapper_allocations", "wrapper_requested_bytes"} <= set(data):
        for field in ("wrapper_allocations", "wrapper_requested_bytes"):
            observed = (data.groupby(keys, dropna=False)[field].mean()
                        .reset_index(name="mean_" + field))
            table = table.merge(observed, on=keys, how="left")
    output.parent.mkdir(parents=True, exist_ok=True)
    table.to_csv(output, index=False)
    return table


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("files", type=Path, nargs="+")
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    frame = summarize(args.files, args.output)
    print(frame.to_string(index=False))


if __name__ == "__main__":
    main()
