# Performance methodology and interpretation

This document distinguishes the existing 3.0.10 benchmark machinery from the
3.1.2 experimental harness. The [research toolkit](../../research/README.md)
includes reproducible paired ranking workloads, public-API stage measurements,
separate Linux/WSL whole-process heap profiling via Valgrind Massif, a strict
experimental-evidence audit and descriptive outcome analysis. The
[verified 3.1.2 measurements](results-3.1.2.md) report batch, incremental,
full-season and heap-memory trade-offs on explicitly identified workloads.
No unmeasured algorithmic superiority or uniform-sampler claim is asserted.

## Existing experiments

`leaderboard_bench` exercises deterministic public hashmap, BOSTree and ranking workloads; configuration requires `LEADERBOARD_BUILD_BENCHMARKS=ON`. The existing CSV reports `clock()`-based process CPU time and runs a correctness smoke test without failing solely on a timing threshold.

## 3.1.2 evaluation protocol and measured scope

The completed experiment and its [quantitative tables](results-3.1.2.md)
use default `recombine` + `independent` sampling on fixed synthetic one-period
league snapshots and matched ranking inputs. The following broader design
space describes possible follow-ups, **not** additional completed trials.

For fixture generation, vary $N\in\{8,10,12\}$, round count, seed, sample count, independent/chain mode and three-round budgets. Record elapsed/CPU time, peak memory, accepted/rejected moves, search limits and diversity. Compare the constrained shuffle baseline with two-/three-round recombination under **identical input and hardware**.

**Ranking is a separate experiment.** The current season resolver uses a
worst-case $O(N\log N)$ heapsort by default, while PR A adds an optional
hashmap/BOSTree backend using exact lexicographic sporting keys. Differential
regression tests establish observable ranking equivalence within their tested
cases. PR B measured workload-specific trade-offs, including a batch heapsort
advantage and a large-input incremental AVL advantage; see
[results](results-3.1.2.md) for measured effect sizes and limitations.

The released research runner measures completed-ranking construction and
incremental insert/update/delete separately on 8, 10 and 12 entries and larger
synthetic ranking-only sizes. The measured query phase is **not** a fair
string-ID comparison: the array uses dense integer indices, whereas AVL
resolves string IDs. Exclude the raw query-speed ratio from comparative
conclusions until a matched query harness is implemented. Massif peaks cover
whole-process heap, not isolated stages; complete Monte Carlo and individual
stage timings are measured separately and cannot be added together.

Time and storage should be interpreted alongside their algorithmic cost models, not as absolute machine-independent numbers. Statistical analysis should report repeated-run variation and account for correlated chain samples. Avoid cross-machine speed comparisons without normalized configurations.

## Verification

See [build instructions](../development/building.md), [CTest](../development/verification.md) and [historical 3.0.10 benchmark contract](../archive/performance-release-3.0.10.md). The documentation migration performs no new performance measurement.
