# Known limitations and boundaries of scientific inference

## Native C scope as of 3.0.12

- The full original calendar is imported and validated; incomplete historical score matrices are allowed but cannot support completed-season match experiments.
- Calendars can be generated; automatic match goals, standings simulation, Monte Carlo orchestration, statistical evaluation, and Python bindings are **not** implemented.
- The strict no-rematch-until-all-opponents rule excludes legitimate alternative league formats. It is deliberately stronger than the 3.0.11 snapshot importer.

## Mathematical and statistical uncertainty

- Constrained Fisher–Yates is uniform over eligible round-order permutations, **not** over the set of all feasible calendars.
- Two-round cycle exchanges can be unable to reach some one-factorizations; bounded three-round recoloring need not repair global connectivity.
- Independent seeding does not guarantee independent uniform samples. Chain mode introduces further sample dependence; no mixing-time proof is currently supplied.
- FNV-1a schedule fingerprints are useful diagnostics but not collision-proof identities.
- Asymptotic complexity ignores constant factors, compiler variations and memory allocator effects.

## Data and external systems

Historical performances are fixed across simulated calendars by design. This is a counterfactual assumption, not a model of psychological reactions to different opponents. Imported scores already contain bonus/malus; applying those modifiers a second time would be incorrect. UUID creation and database storage belong to the future Python layer.

## Evidence standard

A green CI run is bounded verification evidence. Mathematical proof, performance measurement and sporting-outcome inference must be independently stated and supported. See the [style guide](../STYLE_GUIDE.md) and [research methodology](methodology.md).
