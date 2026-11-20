# Milestone 3.1.2: reproducible scientific experiments

**Status:** 3.1.2 research tooling and the **scoped** synthetic evaluation
are complete. The [scientific results](../docs/research/results-3.1.2.md) and
[public machine-readable summaries](results/3.1.2/) record the experimental
findings and evidence hashes. No universal algorithm advantage or uniform
calendar-sampling claim is made; chain/shuffle comparisons remain future work.

This directory contains **research tooling**, not the future Python web service.
The C11 library and installed `CentralRankingDogma::central_ranking_dogma` package have no Python
runtime dependency. Research scripts require pinned Python dependencies in
[`requirements.txt`](requirements.txt). The optional benchmark C targets require
`LEADERBOARD_BUILD_BENCHMARKS=ON`.

## Data provenance and privacy

1. **Synthetic**: `leaderboard_season_experiment 8|10|12 ...` generates fixed,
   seeded fantasy performances and one-factorization calendars. These are not
   observations of a historical Fantacalcio league.
2. **Existing historical workbook**: `tests/fixtures/calendar_2026_2027.tsv`
   contains a ten-team fixture with only its finalized prefix; import a
   pseudonymized copy using `research/import_workbook.py`. Treat predictions or
   eventual champion claims as outside the available evidence.
3. **Authorized historical exports**: `research/import_league.py` accepts three
   normalized CSV files. It checks completeness and structural constraints and
   produces a pseudonymized `competition.tsv` under `research/private/`.

Never commit scraped source data, locally pseudonymized private exports, output
runs, keys or personal mapping files without a rights and privacy review. The
`.gitignore` exclusion applies to **both raw and pseudonymized** private data.
HMAC-derived UUIDs and stripped names reduce direct identifiers but do not
prove anonymity when fixture and scores patterns can be linked to public
records. Scraping a publicly viewable page does not grant redistribution rights.
This repository does not perform automated scraping or bypass access controls.

Importer input schema:

- `teams.csv`: `team_id,name`
- `fixtures.csv`: `round,matchday,home_id,away_id` (rounds start at 1)
- `scores.csv`: `matchday,team_id,points_tenths,status` (FINAL/PENDING)

```sh
python research/import_league.py research/private/raw_export \
  --output research/private/pseudonymized
python research/import_workbook.py
```

The importers support only complete score matrices, including explicit
PENDING cells. The C runner supports up to 12 teams and 40 rounds; the native
competition library applies additional fixture and cooldown validation.

## Research CI policy

The **required**, short checks are `Research methodology smoke / native-profile-smoke`
and `Research methodology smoke / python-methodology`. Configure these two job
contexts as required in the `master` branch protection rule or repository
ruleset. Their workflow runs for every pull request, regardless of changed
paths, and on pushes to `master` after merge. Feature-branch pushes do not
start redundant runs. Merge-queue events are supported. An outdated PR run
may be cancelled when newer commits arrive.

`Research memory profiling / massif-memory-smoke` is a **non-required**,
separate workflow. It runs for PRs touching the native implementation,
benchmark memory runner, memory analysis/protocol, or its own workflow;
`workflow_dispatch` also allows an explicit manual run. This path-filtered
check must **not** be added to GitHub's required status checks: it may not be
scheduled for unrelated documentation-only pull requests. The PR 3.1.2
acceptance checklist still requires a successful Massif check and review of
its archived evidence before merging. A smoke profile is not itself the full
replicated scientific experiment.

## Build and run

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLEADERBOARD_BUILD_BENCHMARKS=ON -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
python -m pip install -r research/requirements.txt
pytest -q research/tests
python research/run_experiments.py --smoke --native-dir build-release
```

Full research workloads (not CI):

```sh
python research/run_experiments.py --native-dir build-release \
  --config research/config/default.json --output research/output/full
python research/summarize_benchmarks.py research/output/full/ranking-*.csv \
  --output research/output/full/ranking-summary.csv
```

The launcher uses 30 ranking replications, 10 independently seeded sampling
batches and 1000 accepted samples per configuration by default. It creates a
run manifest recording revision, configuration, platform and file hashes. The
same synthetic calendar scores/seed are used for both ranking backends; their
raw observation CSV must be byte-identical. Synthetic sizes `8,10,12` are
Fantacalcio-like competition configurations. `100,1000,10000,100000` are
**ranking-only** sizes, not claimed to be real Serie A leagues. Synthetic
league score matrices use a fixed, league-size-specific seed **independent**
of the Monte Carlo sampling seeds, preserving a common comparison baseline.

To run a private or existing-workbook snapshot manually:

```sh
./build-release/leaderboard_season_experiment \
  research/private/workbook_2026_2027/competition.tsv \
  5489 1000 heap recombine independent \
  research/output/observations.csv research/output/summary.csv \
  > research/output/stages.csv
```

**Do not pool finalized-prefix experiments with full-season experiments.** The
pseudonymized workbook retains the actual FINAL/PENDING scope; its results are
provisional. The C runner rejects zero finalized rounds or invalid calendars.

For 10 independently seeded CSV pairs (not two backends for the same seed), run:

```sh
python research/analyze.py \
  --pair research/output/full/league-10-seed-5489-heap.observations.csv \
         research/output/full/league-10-seed-5489-heap.summary.csv \
  --pair research/output/full/league-10-seed-5490-heap.observations.csv \
         research/output/full/league-10-seed-5490-heap.summary.csv \
  --output research/output/full/analysis-10
```

Add the remaining seed pairs for the intended replication count. Bootstrap
intervals operate on **independently seeded batch summaries**, not individual
correlated chain observations. With one batch, the script returns no bootstrap
interval. Results are descriptive conditional on the configured generator.

## Benchmark contracts and threats to validity

`leaderboard_ranking_compare` emits CSV with `backend,phase,count,operations,
replication,seed,wall_seconds,cpu_seconds,checksum`. It uses identical deterministic
input keys and update traces for heapsort (static reference), hashmap/BOSTree
(composite AVL), and a dynamically maintained sorted array (incremental
baseline). The array's initialization from an already sorted snapshot does not
pretend that inserting one item at a time is necessary for static sorting.
Hash-index construction for heapsort is measured separately. Rank query
workloads include both ID lookup and position selection for either structure.
Integrity comparisons are performed outside timed regions where practical.

`leaderboard_season_experiment` measures the **entire** Monte Carlo API call and
separate, repeatable generation, season-resolution and rank-access calls. These
stage measurements are **not additive components of the full API call**:
full batches also perform aggregation, uniqueness bookkeeping and CSV I/O.
The uninstrumented resolution stage includes ranking; the opt-in internal
profiler separates ranking inside that call. Never describe the uninstrumented
resolution duration as ranking-only.
Independent seed/configuration repetitions must use matched datasets.

For **instrumented internal stage measurements**, configure a separate build:

```sh
cmake -S . -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLEADERBOARD_BUILD_BENCHMARKS=ON -DLEADERBOARD_RESEARCH_PROFILE=ON
cmake --build build-profile --parallel
python research/run_experiments.py --smoke --native-dir build-profile \
  --output research/output/profile-smoke
```

This opt-in profile measures stage calls and CPU/wall time for calendar
generation, season resolution (inclusive of ranking), ranking and outcome
aggregation. It counts successful allocations and requested bytes **only**
through `leaderboard_alloc_malloc/calloc`, attributed to the innermost active
stage. Direct third-party allocations, deallocations and allocator overhead
are excluded, so the values are neither complete allocation totals nor peak
live memory. The `profile_canonicalization` stage excludes hash-table lookup
and duplicate insertion. Compare profiling overhead against an uninstrumented
build before interpreting performance differences.

Wall time uses POSIX `CLOCK_MONOTONIC` or Windows
`QueryPerformanceCounter`; CPU time uses C11 `clock()`. Very short operations
approach timer granularity and should be repeated. The launcher samples
process RSS approximately every 10 ms; its maximum is **only an observed lower
bound**, not exact peak allocated bytes. The internal-stage peak live heap is still unavailable; optional
Massif runs report measured process-wide heap peaks instead. Do not silently
interchange them. Memory usage
must be interpreted with allocator, OS and sampling interval stated.

## Full-process heap profiling and acceptance gate

The internal `LEADERBOARD_RESEARCH_PROFILE` allocator counters are deliberately
**not** a complete memory measurement. They omit direct malloc/free calls in
some container implementations and cannot compute peak live heap bytes.

For a measured process-wide heap peak, the research protocol uses **Valgrind
Massif on Linux/WSL** in a *separate run*. The C11
`leaderboard_memory_compare` executable allocates the same deterministic keys
but selects **only one** backend per process (heapsort + index, or hashmap/AVL).
The launcher checks that both runs produce the same rank-order checksum. An
additional pair of Massif runs exercises the actual Monte Carlo executable.

Massif profiles allocation events, including third-party container calls,
that the wrapper profiler cannot observe. Each run archives the unmodified
`massif.out`, Valgrind diagnostics, command, version, checksums and parsed
snapshot peaks. The reported values are whole-process **Massif heap bytes**
(including Massif-estimated allocator overhead), not per-stage allocations,
peak resident set size, or a complete census of anonymous mappings. Default
Massif heap tracking can omit memory-mapped allocations. Profiling adds heavy
overhead: **never compare its CPU/wall times to normal benchmark timings**.

For the complete research protocol on Linux or Ubuntu under WSL:

```sh
sudo apt-get install valgrind
cmake -S . -B build-research -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLEADERBOARD_BUILD_BENCHMARKS=ON -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-research --parallel
ctest --test-dir build-research --output-on-failure
python -m pip install -r research/requirements.txt
python research/run_experiments.py --native-dir build-research \
  --config research/config/default.json --output research/output/full \
  --profile-memory
python research/verify_evidence.py research/output/full
```

The full run is deliberately excluded from ordinary CI. The verification script
fails on smoke runs, incomplete batches, missing replications, mismatched AVL
and heapsort observations, missing Massif profiles, changed artifact hashes,
unknown native executable provenance or a dirty source revision. It establishes
**artifact integrity and methodological coverage**, not a claim that either
backend is superior. The accepted manifest and results must then be examined
for timing uncertainty, instrument effects and platform differences. Results
from a dirty working tree may be explored using the runner, but the strict
acceptance gate requires a committed, clean, identified source revision.

Massif and the native timing experiment should run on the same machine and
build revision, but in separate executions. The default memory protocol
profiles 8, 10, 12, 1000 and 10000 synthetic ranking entries and 8-, 10-,
12-team Monte Carlo samples. These are *selected, controlled workloads*, not
proofs about every workload or every possible league. Using private data in
place of synthetic configurations requires its own provenance report.

The analysis produces mean rank displacement, point changes, Kendall tau-b
(for sporting ties), shared-first-place frequencies, observed fingerprint
diversity and lag-one first-team first-place autocorrelation. The latter is a
limited diagnostic, **not** a mixing-time proof or a trustworthy effective
sample size. Fingerprint diversity is not a collision-proof uniqueness proof.
Bootstrap intervals represent variation among independent seeded experiments,
not credible statements about the proportion of every possible calendar.

Experimental measurements are intentionally excluded from CI pass/fail speed
thresholds. A benchmark smoke test can establish functionality and schema,
never ranking superiority. Separate full research records must include compiler
version/flags, CPU, memory, OS, source revision, seed/configuration and caveats
before reporting quantitative performance findings.

See [research methodology](../docs/research/methodology.md),
[performance methodology](../docs/research/performance.md), and
[limitations](../docs/research/limitations.md).
