# Internal C architecture: current 3.1.1 baseline

The principal Mermaid dependency diagram is in the [root README](../../README.md#internal-c-architecture). Its graph is deliberately more prominent than the future web application, because this repository ships the native C11 library.

## Modules and dependencies

| Layer | Exported interface | Dependency and purpose |
| --- | --- | --- |
| Generic containers | `hashmap_base.h`, `hashmap_typed.h`, `bostree.h` | Generic indexed lookup and order statistics |
| Ranking | `ranking.h` | Maintains independent signed scores over hashmap and BOSTree; **not currently used by season resolution** |
| Randomness | `mtwister.h` | Deterministic, noncryptographic MT19937 generation |
| Team identity | `team.h` | Externally assigned UUID, owned display name, fixed-point aggregate |
| Historical competition | `competition.h` | Validates and deep-copies teams, fixtures, mapped Serie A days and score cells |
| Calendar | `calendar.h` | Borrows competition while generating, returns independent calendar objects; uses MT19937 |
| Standings | `standings.h` | Copies UUID mapping, accumulates W/D/L, scored goals and league points |
| Match scoring | `match.h` | Converts FINAL scores into goals using configurable fixed-point bands and corrections |
| Season resolution | `season_resolution.h` | Owns independently resolved fixtures, standings and deterministic ranks |
| Monte Carlo experiments | `monte_carlo.h` | Owns batch statistics and independent baseline; streams CSV, optionally tracks exact schedule identities |

The CMake build combines distinct object modules into a single installed static target `Leaderboard::leaderboard`. These components are **not independently installed libraries**. The original CLI is an example consumer and is not part of the research simulator.

## Ownership map

- `team_t` owns its allocated display name; the caller supplies the UUID.
- `competition_t` deep-copies imported input and owns its fixture/score arrays and team records. Names returned by accessors are borrowed read-only views.
- `calendar_generator_t` borrows the competition during its lifetime. `calendar_t` owns its returned fixtures and can outlive the generator.
- `standings_t` copies the UUID mapping and owns per-team goal statistics; its entries are mutable per experiment.
- `season_resolution_t` owns copied UUIDs, ranked entries, fixture outcomes and private standings; it outlives competition/calendar/rules source objects.
- `monte_carlo_result_t` owns the historical baseline and online aggregates; temporary generated calendars and season outputs are destroyed after each draw. CSV streams remain caller-owned.
- The opaque generic `ranking_t` owns copied identifier strings and stores signed scalar scores. Its hashmap/BOSTree representation is independent from `season_resolve()`, which currently ranks finished standings via heapsort.

## Future boundary

The 3.1.0 resolver and 3.1.1 Monte Carlo layer are implemented in `master`: the latter composes the seeded generator with the deterministic resolver and streams descriptive CSV output. Season-resolution rankings currently use in-place heapsort; they do not call the independent `ranking_t`. The mandatory [3.1.2](../milestones/3.1.2.md) integration PR will introduce an optional composite-key hashmap/BOSTree backend, followed by a separate scientific evaluation PR. External Python and database concerns remain outside this repository; see the [system diagram](system.md).

**See also:** [failure guarantees](../reference/failure-safety.md), [data flow](data-flow.md), and [API reference index](../index.md#module-sub-papers).
