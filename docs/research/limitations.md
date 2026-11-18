# Known limitations and boundaries of scientific inference

## Native C scope of the evaluated 3.1.2 implementation

- Imported original fixtures, generated alternative calendars, deterministic match and season resolution (3.1.0), and reproducible Monte Carlo batches (3.1.1) are implemented.
- The optional composite-key AVL sporting-ranking backend was introduced in 3.1.2 PR A. Heapsort remains the default and correctness reference.
- Completed-season evaluation requires all scores FINAL. A finalized-prefix calculation is provisional, and later finalized scores after a gap cannot be silently incorporated.
- The experimental toolkit can compare ranking methods and summarize sampled outcomes, but no uniform-sampling proof, automatic causal inference or Python bindings exist.
- The strict no-rematch-until-all-opponents rule excludes legitimate alternative league formats. It is deliberately stronger than the original snapshot importer.
- The optional internal profiler counts allocations through library wrappers, not direct third-party allocations or frees. Separate Linux/WSL Massif runs can measure peak instrumented whole-process heap usage (including third-party allocator calls); those peaks are not per-stage or equivalent to process RSS. Sampled RSS is only an observed lower bound on peak RSS; Massif may omit unmapped allocations or allocations outside its supported heap interception.

## Mathematical and statistical uncertainty

- Constrained Fisher–Yates is uniform over eligible round-order permutations, **not** over the set of all feasible calendars.
- Two-round cycle exchanges can be unable to reach some one-factorizations; bounded three-round recoloring need not repair global connectivity.
- Independent seeding does not guarantee independent uniform samples. Chain mode introduces further sample dependence; no mixing-time proof is currently supplied.
- FNV-1a schedule fingerprints are useful diagnostics but not collision-proof identities.
- Asymptotic complexity ignores constant factors, compiler variations and memory allocator effects.

## Data and external systems

Historical performances are fixed across simulated calendars by design. This is a counterfactual assumption, not a model of psychological reactions to different opponents. Imported scores already contain bonus/malus; applying those modifiers a second time would be incorrect. UUID creation and database storage belong to the future Python layer.

## Limitations of the recorded 3.1.2 experiment

- Results cover only `recombine` + `independent` sampling with fixed synthetic
  scores and one round-robin period of 7, 9 or 11 rounds; chain/shuffle modes,
  multi-period seasons, historical observations, and sampler mixing were not
  experimentally compared.
- The sorted-array query harness uses dense numeric indexing, whereas the AVL
  harness resolves string IDs; their direct query timing ratio is not a fair
  common-interface performance comparison.
- Thirty ranking replications share a process; Massif records one heap-peak
  run per configuration. Very short timings and memory ratios do not support
  hardware-independent or tight uncertainty claims.
- No exact shared sporting rank occurred in the 30,000 sampled calendars;
  deterministic differential tests cover sporting-tie correctness separately.
- [Published results and evidence record](results-3.1.2.md) explicitly bound
  the conclusions; a green CI job is not a scientific result by itself.

## Evidence standard

A green CI run is bounded verification evidence. Mathematical proof, performance measurement and sporting-outcome inference must be independently stated and supported. See the [style guide](../STYLE_GUIDE.md) and [research methodology](methodology.md).
