# Performance methodology and interpretation

This document distinguishes the **existing performance benchmark machinery** introduced in 3.0.10 from the planned sporting-outcome evaluation of 3.1.2. It does not report unmeasured speedups.

## Existing experiments

`leaderboard_bench` exercises deterministic public hashmap, BOSTree and ranking workloads; configuration requires `LEADERBOARD_BUILD_BENCHMARKS=ON`. The existing CSV reports `clock()`-based process CPU time and runs a correctness smoke test without failing solely on a timing threshold.

## Proposed 3.1.2 comparisons

For fixture generation, vary \(N\in\{8,10,12\}\), round count, seed, sample count, independent/chain mode and three-round budgets. Record elapsed/CPU time, peak memory, accepted/rejected moves, search limits and diversity. Compare the constrained shuffle baseline with two-/three-round recombination under **identical input and hardware**.

Time and storage should be interpreted alongside their algorithmic cost models, not as absolute machine-independent numbers. Statistical analysis should report repeated-run variation and account for correlated chain samples. Avoid cross-machine speed comparisons without normalized configurations.

## Verification

See [build instructions](../development/building.md), [CTest](../development/verification.md) and [historical 3.0.10 benchmark contract](../archive/performance-release-3.0.10.md). The documentation migration performs no new performance measurement.
