# Internal C architecture: native 3.1.3 baseline

The principal Mermaid dependency diagram is in the [root README](../../README.md#internal-c-architecture). Its graph is deliberately more prominent than the future web application, because this repository ships the native C11 library.

## Modules and dependencies

| Layer | Exported interface | Dependency and purpose |
| --- | --- | --- |
| Generic containers | `hashmap_base.h`, `hashmap_typed.h`, `bostree.h` | Generic indexed lookup and order statistics |
| Ranking | `ranking.h` | Generic scalar or owned composite keys over hashmap and BOSTree; optional season backend |
| Randomness | `mtwister.h` | Deterministic, noncryptographic MT19937 generation |
| Team identity | `team.h` | Externally assigned UUID, owned display name, fixed-point aggregate |
| Historical competition | `competition.h` | Validates and deep-copies teams, fixtures, mapped Serie A days and score cells |
| Calendar | `calendar.h` | Borrows competition while generating, returns independent calendar objects; uses MT19937 |
| Standings | `standings.h` | Copies UUID mapping, accumulates W/D/L, scored goals and league points |
| Match scoring | `match.h` | Converts FINAL scores into goals using configurable fixed-point bands and corrections |
| Season resolution | `season_resolution.h` | Owns independently resolved fixtures, standings and deterministic ranks |
| Monte Carlo experiments | `monte_carlo.h` | Owns batch statistics and independent baseline; streams CSV, optionally tracks exact schedule identities |

The root CMake project delegates native targets to `native/CMakeLists.txt`, benchmarks to `benchmarks/CMakeLists.txt`, and tests to `tests/CMakeLists.txt`. Native implementation sources live under `native/src/` in responsibility-based directories, with public headers in `native/include/` and private headers in `native/src/internal/`. The CMake object libraries combine into a single installed static target, `CentralRankingDogma::central_ranking_dogma`; they are **not independently installed libraries**. The separate `native/cli/json/` and `native/cli/educational/` adapters build executable consumers, not libraries exported to C applications. See the [full ownership map](../development/repository-layout.md).

## Ownership map

- `team_t` owns its allocated display name; the caller supplies the UUID.
- `competition_t` deep-copies imported input and owns its fixture/score arrays and team records. Names returned by accessors are borrowed read-only views.
- `calendar_generator_t` borrows the competition during its lifetime. `calendar_t` owns its returned fixtures and can outlive the generator.
- `standings_t` copies the UUID mapping and owns per-team goal statistics; its entries are mutable per experiment.
- `season_resolution_t` owns copied UUIDs, ranked entries, fixture outcomes and private standings; it outlives competition/calendar/rules source objects.
- `monte_carlo_result_t` owns the historical baseline and online aggregates; temporary generated calendars and season outputs are destroyed after each draw. CSV streams remain caller-owned.
- The opaque `ranking_t` owns IDs and either scalar scores or copied fixed-size composite keys. Comparator context is borrowed. `season_resolve_with_options()` can use it for sporting order, while `season_resolve()` keeps heapsort by default.

## Future boundary

The 3.1.0 resolver and 3.1.1 Monte Carlo layer retain their documented defaults. Milestone [3.1.2](../milestones/3.1.2.md) added optional composite-key AVL ranking and completed a bounded synthetic comparison against heapsort; the [published results](../research/results-3.1.2.md) apply only to their recorded workloads. Milestone 3.1.3 implemented the JSON stdin/stdout adapter and independent Python subprocess example. The Python **web application**, authentication and database remain outside this C repository; see the [system diagram](system.md).

**See also:** [failure guarantees](../reference/failure-safety.md), [data flow](data-flow.md), and [API reference index](../index.md#module-sub-papers).
