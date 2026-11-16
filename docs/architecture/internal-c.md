# Internal C architecture: current 3.0.12 baseline

The principal Mermaid dependency diagram is in the [root README](../../README.md#internal-c-architecture). Its graph is deliberately more prominent than the future web application, because this repository ships the native C11 library.

## Modules and dependencies

| Layer | Exported interface | Dependency and purpose |
| --- | --- | --- |
| Generic containers | `hashmap_base.h`, `hashmap_typed.h`, `bostree.h` | Generic indexed lookup and order statistics |
| Ranking | `ranking.h` | Maintains independent signed scores over hashmap and BOSTree |
| Randomness | `mtwister.h` | Deterministic, noncryptographic MT19937 generation |
| Team identity | `team.h` | Externally assigned UUID, owned display name, fixed-point aggregate |
| Historical competition | `competition.h` | Validates and deep-copies teams, fixtures, mapped Serie A days and score cells |
| Calendar | `calendar.h` | Borrows competition while generating, returns independent calendar objects; uses MT19937 |
| Standings | `standings.h` | Copies UUID mapping, accumulates supplied W/D/L outcomes and league points |

The CMake build combines distinct object modules into a single installed static target `Leaderboard::leaderboard`. These components are **not independently installed libraries**. The original CLI is an example consumer and is not part of the research simulator.

## Ownership map

- `team_t` owns its allocated display name; the caller supplies the UUID.
- `competition_t` deep-copies imported input and owns its fixture/score arrays and team records. Names returned by accessors are borrowed read-only views.
- `calendar_generator_t` borrows the competition during its lifetime. `calendar_t` owns its returned fixtures and can outlive the generator.
- `standings_t` copies the UUID mapping and can outlive the original competition; its `standing_entry_t` values are per-experiment mutable state.
- The opaque generic `ranking_t` owns copied identifier strings; its signed score contract is unrelated to fantasy league points.

## Future boundary

The 3.1.0 match resolver will read calendar fixtures and historical team performances, then record outcomes in standings. It is **not yet implemented**. The Python adapter, database and frontend are external to this C repository and are illustrated in the [system diagram](system.md).

**See also:** [failure guarantees](../reference/failure-safety.md), [data flow](data-flow.md), and [API reference index](../index.md#module-sub-papers).
