# Performance methodology and interpretation

This document distinguishes the **existing performance benchmark machinery** introduced in 3.0.10 from the planned sporting-outcome evaluation of 3.1.2. It does not report unmeasured speedups.

## Existing experiments

`leaderboard_bench` exercises deterministic public hashmap, BOSTree and ranking workloads; configuration requires `LEADERBOARD_BUILD_BENCHMARKS=ON`. The existing CSV reports `clock()`-based process CPU time and runs a correctness smoke test without failing solely on a timing threshold.

## Proposed 3.1.2 comparisons

For fixture generation, vary $N\in\{8,10,12\}$, round count, seed, sample count, independent/chain mode and three-round budgets. Record elapsed/CPU time, peak memory, accepted/rejected moves, search limits and diversity. Compare the constrained shuffle baseline with two-/three-round recombination under **identical input and hardware**.

**Ranking is a separate experiment.** The current season resolver uses a
worst-case $O(N\log N)$ heapsort, while the independent `ranking_t` combines
hashmap lookup and AVL order statistics. Before benchmarking an integrated
backend, 3.1.2 PR A must add lexicographic sporting keys and demonstrate exact
sporting-rank equivalence, including ties, against heapsort. The existing
scalar-score ordering is not an adequate substitute.

In 3.1.2 PR B, measure completed-rank construction and selection separately
from incremental insert/update/delete and order-query workloads. Specify an
array baseline for each, use identical update/query traces, distinguish
sporting ties from deterministic UUID display order, and record allocations,
peak memory, CPU/wall time, repeat variability and compiler/hardware settings.
Use real league sizes 8, 10 and 12 for domain workloads; use larger sizes only
for explicitly labelled synthetic **ranking-only** experiments. Report ranking
cost separately from generation, scored-match resolution and aggregation.

Time and storage should be interpreted alongside their algorithmic cost models, not as absolute machine-independent numbers. Statistical analysis should report repeated-run variation and account for correlated chain samples. Avoid cross-machine speed comparisons without normalized configurations.

## Verification

See [build instructions](../development/building.md), [CTest](../development/verification.md) and [historical 3.0.10 benchmark contract](../archive/performance-release-3.0.10.md). The documentation migration performs no new performance measurement.
