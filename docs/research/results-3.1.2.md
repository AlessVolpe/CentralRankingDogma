# Milestone 3.1.2: measured ranking and synthetic-calendar results

**Status:** Completed *scoped* scientific evaluation; experimental source
revision `f5ac3699a0715f033720a9aab7ff7c706393950d` (2026-11-18).
This is an empirical evaluation of the recorded workloads, **not** a proof of
uniform fixture sampling, a study of real historical league outcomes, or a
claim that one ranking backend dominates all workloads. See
[methodology](methodology.md), [limitations](limitations.md), and the
[milestone contract](../milestones/3.1.2.md).

## Provenance, acceptance and archived outputs

The original external research archive is
`leaderboardsystem-3.1.2-evidence.tar.gz`, SHA-256:

```text
5c5582f9f763bc11328b9fa31d7144bd05b3c3df8765075b109aa15e286e95a7
```

It was produced from a **clean** Git revision with CMake Release `-O3 -DNDEBUG`,
GCC 13.3, Ubuntu 24.04 (WSL2, glibc 2.39), Valgrind 3.22.0, 24 logical CPUs,
and 16.31 GB WSL-visible RAM. Generation used the default
[`research/config/default.json`](../../research/config/default.json) settings:
seed base 5489, 30 ranking repetitions per size, independent seeds 5489–5498,
1,000 sampled calendars per seed and league size, `recombine` + `independent`.

The strict, unmodified `research/verify_evidence.py` for that revision was
executed against the provided archive and exited successfully. Its recorded
[`validation.json`](../../research/results/3.1.2/validation.json) reports
**2,520 benchmark timing rows, seven ranking sizes, 30 paired league/seed
configurations and 16 Massif profiles**. All **263/263 primary artifact hashes**
match the run manifest; all 30 matching heapsort/AVL observations are identical
byte-for-byte. The source revision and executable SHA-256 values are recorded
in the original manifest, though the original executable binaries are not
contained in this archive for an independent binary-hash verification.

The original manifest does **not** cover the 16 subsequently created analysis
and benchmark-summary files. Their independent SHA-256 values, and the archive
hash, are recorded in
[`supplementary-sha256.txt`](../../research/results/3.1.2/supplementary-sha256.txt).
Selected machine-readable summaries are committed under
[`research/results/3.1.2/`](../../research/results/3.1.2/). The full 5.5 MB
primary archive remains an **external research artifact**; it is not committed
to Git and must be retained alongside this manifest for independent replay.

## Research design

Two questions are kept separate: (1) batch ranking construction versus
incremental ranking maintenance, and (2) how reassignment of fixtures changes
outcomes when a synthetic team's score for each real matchday is held fixed.
Ranking sizes are 8, 10, 12, 100, 1,000, 10,000 and 100,000 entries; sizes
above 12 are **synthetic ranking-only experiments**, not possible league sizes.
The synthetic championship configurations contain a **single round-robin
period** of 7, 9 and 11 rounds, respectively, not full multi-period seasons.

## Completed ranking construction

Median wall-clock nanoseconds per entry, over 30 within-process repetitions.
The heapsort baseline includes both sorting and its separately measured
positional-index construction. Ratios compare matched workloads (AVL/heapsort).

| Entries | Heapsort + index (ns/entry) | AVL construction (ns/entry) | AVL / heapsort |
| ---: | ---: | ---: | ---: |
| 8 | 244.25 | 341.38 | 1.39x |
| 10 | 198.95 | 307.20 | 1.54x |
| 12 | 167.42 | 283.50 | 1.70x |
| 100 | 50.21 | 232.81 | 4.66x |
| 1,000 | 49.96 | 283.59 | 5.63x |
| 10,000 | 81.44 | 343.12 | 4.25x |
| 100,000 | 163.75 | 735.91 | 4.38x |

Batch construction favors heapsort for all tested sizes. Values for 8–12
entries approach clock-resolution and fixed-overhead limits; avoid treating
sub-microsecond differences as machine-independent constants.

## Incremental ranking maintenance

Identical deterministic key-update traces, with a sorted-array reference and
a hashmap/BOSTree AVL. The large-input experiment changes at most **200
entries**, not all 100,000 entries.

| Entries | Sorted array update (ns/op) | AVL update (ns/op) | Array / AVL |
| ---: | ---: | ---: | ---: |
| 8 | 178.50 | 288.12 | 0.62x |
| 10 | 150.30 | 256.00 | 0.59x |
| 12 | 131.17 | 232.17 | 0.56x |
| 100 | 93.66 | 173.78 | 0.54x |
| 1,000 | 461.06 | 223.44 | 2.06x |
| 10,000 | 5,363.25 | 345.01 | 15.54x |
| 100,000 | 65,443.92 | 1,596.29 | 41.00x |

The crossover for *this update trace* lies between 100 and 1,000 entries.
The sorted array wins at the genuine 8–12-team scale, while AVL is much faster
for larger incremental workloads, with additional heap costs. This does not
establish a universal crossover point.

**Query-benchmark qualification:** the sorted-array `get_and_select` path uses
a precomputed dense integer-ID positional index, whereas the AVL path looks up
string IDs in a hashmap and then selects tree ranks. They do **not** use
identical identifier-resolution APIs. Although result checksums agree, their
query timings must not be used to claim an AVL/array speed ratio under
identical string-ID semantics. A matched string-ID query benchmark is future
work rather than an accepted result of this milestone.

## Native Monte Carlo execution time

Median observed wall-clock **milliseconds per full 1,000-sample run**:

| Synthetic league | Heapsort (ms) | AVL (ms) | Mean paired AVL/heap ratio and 95% seed-bootstrap interval |
| ---: | ---: | ---: | :--- |
| 8 teams | 107.57 | 108.46 | 1.004 [0.970, 1.035] |
| 10 teams | 105.64 | 107.60 | 1.028 [1.004, 1.049] |
| 12 teams | 136.19 | 141.43 | 1.007 [0.885, 1.107] |

Calendar generation dominates total work; using AVL increases isolated
resolution cost but causes much smaller overall differences. Timing stages
are separately rerun public-API operations and **must not be summed** to
reconstruct `monte_full`. The intervals resample only ten seed-paired runs,
are exploratory, and cannot remove machine-scheduling and environment noise.

## Massif process-wide heap peaks

Massif separately profiles each backend; **one process run per configuration**.
The figures include Massif-estimated allocator overhead and are neither
per-stage native-library peaks nor RSS measurements.

| Workload | Heapsort (bytes) | AVL (bytes) | AVL / heap |
| :--- | ---: | ---: | ---: |
| Ranking, 8 entries | 4,472 | 4,472 | 1.00x |
| Ranking, 10 entries | 4,568 | 4,656 | 1.02x |
| Ranking, 12 entries | 4,648 | 5,120 | 1.10x |
| Ranking, 1,000 entries | 56,032 | 269,024 | 4.80x |
| Ranking, 10,000 entries | 560,032 | 2,622,432 | 4.68x |
| League, 8 teams | 60,752 | 65,096 | 1.07x |
| League, 10 teams | 79,072 | 83,912 | 1.06x |
| League, 12 teams | 99,016 | 104,368 | 1.05x |

AVL has a pronounced heap cost for the larger synthetic ranking workloads,
but only a 5–7% whole-process increase in these small-league configurations.
Single-run Massif measurements have no uncertainty interval. Valgrind changes
the runtime and its measurements cannot be used as ordinary timing data.

## Counterfactual sporting outcomes

Ten independently seeded 1,000-calendar samples yield **10,000 draws per
league size**. The original winner is the winner under the fixed baseline
fixture schedule, while a "modal champion" wins most often among the sampled
alternative schedules. No inference about actual historical leagues follows.

| Synthetic league | Original winner remains first | Modal champion | Mean abs. rank displacement (95% seed-bootstrap) | Mean abs. championship-point change | Mean Kendall tau-b |
| ---: | ---: | :--- | :--- | ---: | ---: |
| 8 | 7.26% | Team 1: 66.95%; baseline rank 5 | 2.634 [2.622, 2.644] | 3.319 | 0.063 |
| 10 | 45.52% | Team 8: 45.52%; baseline rank 1 | 1.679 [1.667, 1.690] | 2.567 | 0.559 |
| 12 | 4.35% | Team 7: 51.57%; baseline rank 3 | 3.333 [3.320, 3.344] | 3.677 | 0.186 |

The pronounced differences are **scenario-specific**: fixture assignments
change the sampled champion frequently in some fixed synthetic score matrices,
but much less in others. Sensitivity is not monotonically increasing with
league size. These frequencies characterize the **configured generator**;
they are not uniform probabilities over the complete valid-calendar space and
are not causal predictions about real teams.

In the recorded 30,000 sampled calendars (10,000 for each size) there were **no
exact sporting ties**; the regression suite tests tie handling separately.
Among 10,000 draws per size, the total distinct 64-bit schedule fingerprint
counts are 9,984 (8 teams), 10,000 (10 teams), and 10,000 (12 teams). The
fingerprints are useful diversity diagnostics, not collision-proof schedule
identities. The observed 1,000/1,000 accepted ratio measures successful
sampling calls, not the success rate of internal recombination proposals.

## Interpretation, validity limits and future research

1. **Sampling mechanism:** only `recombine` + `independent` is measured. No
   chain-mode mixing or constrained-shuffle comparison was performed. Independent
   PRNG seeds do not imply independent **uniform** calendar samples, and an
   unproven state-space coverage assumption must not be inserted post hoc.
2. **Realism:** synthetic, single-round-robin leagues and fixed scores cannot
   establish actual historical championship probabilities, multi-period
   season sensitivity, or a causal scheduling effect in human competition.
3. **Queries:** numeric-ID array positions and string-ID AVL queries are
   distinct interface workloads. Their query timings are **excluded from
   comparative performance conclusions** pending matched input semantics.
4. **Replication:** 30 ranking repetitions occur within a process, not 30
   cold independent process executions. The microbenchmark does not isolate
   allocator/caching/clock noise. Ten seeds give preliminary between-seed
   uncertainty, not guarantees about sampling representativeness.
5. **Memory:** one Massif run per workload; heap peaks are process-wide and
   exclude unsupported or uninstrumented mappings. No per-stage heap peaks or
   memory confidence intervals are claimed.
6. **Coverage:** exact shared sporting ties were not encountered in the sampled
   synthetic scenarios, so differential tests, not these experiments, validate
   sporting-tie semantics.

These boundaries **close 3.1.2 with a scoped evaluation**. Matched query APIs,
chain/shuffle contrasts, historical-data studies and process-level benchmark
replication remain prospective work; they are not presented as achieved
milestone deliverables. The installed library remains C11, the default ranking
backend remains heapsort, and the Python web application is separate.

## Reproduction

On Linux/WSL from a clean checkout of the recorded code revision, with the
pinned Python dependencies, CMake/Ninja, and Valgrind installed:

```sh
cmake -S . -B build-research -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLEADERBOARD_BUILD_BENCHMARKS=ON -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-research --parallel
ctest --test-dir build-research --output-on-failure
python -m pip install -r research/requirements.txt
python research/run_experiments.py --native-dir build-research \
  --config research/config/default.json --output research/output/full \
  --profile-memory
python research/verify_evidence.py research/output/full
python research/summarize_benchmarks.py research/output/full/ranking-*.csv \
  --output research/output/full/ranking-summary.csv
```

Review [the research runner instructions](../../research/README.md) for
per-seed statistical analysis and the limitations of adding modes. Preserve
both the original run manifest and all unmodified raw evidence for audit.
