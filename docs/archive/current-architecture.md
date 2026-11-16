# Current architecture and integration boundaries

**Baseline:** milestone 3.0.12. The library exports `Leaderboard::leaderboard`
as a single installed C11 static archive. The Python application is not built
in this repository; fixture permutations are implemented but match resolution is not.

## Native layers

| Layer | Interface | Responsibility |
| --- | --- | --- |
| Hashmap | `hashmap_base.h`, `hashmap_typed.h` | Identifier lookup / typed callbacks |
| AVL order-statistics tree | `bostree.h` | Ordered search, rank and selection |
| Generic ranking | `ranking.h` | Independent signed-64-bit scores and deterministic ID tie-break |
| Team identity | `team.h` | Parsed UUIDs, owned names, historical sum in tenths |
| Competition | `competition.h` | Validated original calendar, scores, pair frequencies, immutable snapshot |
| Championship standings | `standings.h` | Per-simulation W/D/L and checked 3/1/0 point accumulation |
| Calendar permutation | `calendar.h` | Periodic opponent schedules, seeded shuffle, 2/3-round recoloring, validation |
| PRNG and examples | `mtwister.h`, legacy utilities | Deterministic/noncryptographic PRNG and demos |

CMake builds `leaderboard_containers_obj`, `leaderboard_ranking_obj`,
`leaderboard_legacy_obj`, and `leaderboard_domain_obj` into one static target.
The old GNU-extension `HASHMAP(...)` macros remain compatibility-only; use
`hashmap_typed.h` for strict C11 clients. Only designated white-box tests can
access internal representations.

## Domain boundaries

Team UUIDs are supplied externally (future DB) and converted to 16-byte
identifiers. Competition indices are stable for an input set because they are
sorted by UUID bytes; they are not persistent IDs. `team_t` owns its copied
name. Immutable `competition_t` owns a deep copy of input data and exposes
only read-only names plus copied fixture, score, and UUID values.

There are no simulation-dependent championship points in `team_t`.
`standings_t` owns a separately allocated array of zeroed championship records
with copied UUIDs in the same sorted order as the original competition.
It can outlive `competition_t`, tracks wins/draws/losses and the derived
championship points, and supports reset between calendar experiments.
`standings_record_match` applies an externally supplied outcome to both
teams atomically; fantasy-goal and match outcome calculations remain future work.
A team's `total_points_tenths` is derived from finalized historical day scores; when
the season is incomplete this is a **partial** sum. Pending scores are
represented explicitly and do not equal a finalized score of zero.

A league round maps to a distinct Serie A matchday, possibly beginning later
than Serie A matchday 1. The original fixtures are preserved verbatim after
UUID-to-index normalization. Pair frequencies are used in 3.0.12, whose
generated calendars preserve exact unordered and directed home/away pair
counts. The stronger cooldown requires every team to face all opponents
before repeating one; generated pairings therefore repeat every `N-1`
rounds. It is enforced by generator creation, not by 3.0.11 snapshot import.
Home advantage remains disabled. Only calendars are generated, not match results.

The existing `ranking_t` maintains its independent signed-64-bit score API
and has no implicit Fantacalcio tie-break rules. The interactive CLI remains
a demonstration, not a fixture simulator.

No persistence, database calls, athlete-grade calculation, configurable
bonus/malus, **match-outcome calculation**, or Python bindings are included. The standings layer only records outcomes provided by
its caller.

See [3.0.11 league/fixture contract](league-fixture-contract-3.0.11.md),
[standings contract](standings-contract-3.0.11.md), and
[3.0.11 migration notes](league-3.0.11-CHANGELOG.md), and
[3.0.12 mathematical contract](calendar-permutation-3.0.12.md).
