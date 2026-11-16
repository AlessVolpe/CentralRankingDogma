# League Data Model and Fixture Representation (3.0.11)

**Status:** Implemented native C domain contract. CMake version: **3.0.11**.
**Not implemented:** fixture permutations (3.0.12), fantasy-goal/match outcome
calculation, athlete scoring, databases, JSON/Python integration. A separate
`standings_t` now accumulates **supplied** W/D/L outcomes and league points.

## System boundary

The future Python web app registers teams, stores their UUIDs and scores in a
DB and imports the original full Fantacalcio calendar. The C library receives
an `competition_input_t` snapshot; it never queries a database or depends on
the web app. The `ranking_t` module remains domain-neutral.

## Identifiers and representation

- Public team IDs use canonical 36-character UUID text with 8-4-4-4-12 hex
  digits; parsing is case-insensitive and UUID versions are unrestricted.
  UUIDs are **provided** by the application. The legacy noncryptographic
  `uuid_generate()` is NOT a database ID generator.
- `team_uuid_t` contains 16 bytes. Internal `team_index_t` is `uint32_t`.
  Indices are assigned by ascending binary UUID bytes, irrespective of
  incoming registration order, and are NOT persistent database identifiers.
- `team_t` owns an allocated display name (`TEAM_NAME_MAX_BYTES == 255`),
  allows duplicate names, and retains a nonnegative aggregate fantasy score
  in fixed-point tenths. `team_create` deep-copies the name.
- The immutable `competition_t` owns all teams, fixture arrays, round-to-Serie-A
  matchday mapping, scores, and a symmetric opponent-pair count matrix.
  `competition_team_at` returns a borrowed **read-only** name pointer, valid
  until `competition_destroy`; accessors return copied values otherwise.
- Mutable `championship_points` no longer exists in `team_t`. It belongs to
  per-simulation `standings_t` records. The standings API accepts supplied
  results and preserves the invariant `championship_points = 3*wins + draws`.
  It does not calculate match outcomes from fantasy scores. See the
  [3.0.11 standings contract](standings-contract-3.0.11.md).

## Calendar and score validation

Let N be an even team count >=2 and R be the number of original league rounds.
For each round, the library requires exactly N/2 head-to-head fixtures, each
team appearing once, and two distinct existing team UUIDs per fixture.
Rounds map to strictly increasing positive Serie A matchday identifiers,
which need not start at 1 and may skip dates. Original round and fixture
ordering is retained; this API does not generate permutations.

The importer explicitly describes **every** team/Serie-A-matchday cell using
`competition_score_input_t`, even if the corresponding matchday is not yet
played. Thus `score_count == N*R`; each cell is unique. Scores are either:

- `FANTASY_SCORE_FINAL`: `points_tenths` is an exact nonnegative decimal x10,
  including the valid score **zero**.
- `FANTASY_SCORE_PENDING`: `points_tenths == 0` is only a placeholder and is
  **not** counted towards the team's total or treated as a finalized zero.

The snapshot derives `team_t.total_points_tenths` from FINAL scores and
checks `uint32_t` addition overflow. For unfinished seasons, that value is a
**partial** aggregate. `competition_is_simulation_ready()` returns `true`
only when all N*R entries are finalized. It does not prevent loading an
otherwise valid ongoing calendar.

The future Python adapter should convert human-readable grades to integers
using decimal arithmetic before calling C. The C engine does not accept
floating-point grades or silently truncate values.

## Original pair frequencies (3.0.12 handoff)

`competition_pair_count(c,a,b)` returns the number of imported fixtures
between the unordered pair {a,b}; it is symmetric. The original counts are
cached. Every future valid permutation G must preserve each count exactly:

`count_G(a,b) == count_original(a,b)` for all a != b.

Every generated round must still be a perfect matching, and historical scores
remain attached to **Serie A matchdays**, not to movable fixture objects.
The pair counts need not be uniform. 3.0.11 only records and validates the
baseline; it does not sample any alternative calendars.

Imported home/away orientation is retained in the source fixtures but **home
advantage is disabled** for the planned experiment. Home/away balance in
new calendars is deliberately unspecified until 3.0.12.

## Real calendar regression

`tests/fixtures/calendar_2026_2027.tsv` is extracted from the user-provided
2026/27 `Calendario_2026-2027.xlsx`, not scraped from an external service.
It contains 10 teams, 35 competition rounds, 5 matches per round and 175
fixtures. Rounds 1..35 map to Serie A days 4..38. At the November 16, 2026
snapshot, 40 fixtures from rounds 1..8 have finalized scores; 135 fixtures
from rounds 9..35 are still pending. The original 45 unordered pairs have
40 frequencies of 4 and 5 frequencies of 3. The test harness creates local,
repeatable UUIDs solely for testing; the workbook does NOT supply DB UUIDs.

## Memory and error contracts

- `competition_create()` either returns a completely initialized, immutable
  snapshot or `*out == NULL` with a negative errno-style status. It deep-copies
  all input strings, and converts UUID strings to byte identifiers.
- All dynamic allocations use the library's allocation boundary, which allows
  one-shot failure injection in test builds; every failed constructor path
  frees partial allocations. No test-only fault hooks are publicly installed.
- The snapshot is single-threaded and not mutation-safe. It has no persistence
  guarantees. Callers may release input buffers after successful creation;
  borrowed accessor names must not be modified or retained after destruction.
- Strict C11 headers, cross-platform CMake packaging, regression fixtures,
  fault-injection tests, ASan/UBSan where available, and installed-consumer
  checks form the acceptance suite.

## Athlete-level bonus/malus: documented for future work, NOT applied in 3.0.11

The following table reproduces the user's league rules screenshot.
`Differenzia per ruolo` is **disabled**. These are informational defaults
for later raw-score production, not a configurable C scoring engine at this
milestone. They must not be added again to externally finalized team scores.

| Event | Points |
| --- | ---: |
| Assist | +1 |
| Porta inviolata | +1 |
| Gol subito | -1 |
| Gol della vittoria | 0 |
| Gol del pareggio | 0 |
| Assist gold | +1 |
| Player of the match | 0 |
| Rigore sbagliato | -3 |
| Autogol | -2 |
| Espulsione | -1 |
| Rigore parato | +3 |
| Gol segnato | +3 |
| Rigore segnato | +3 |
| Assist soft | +1 |
| Ammonizione | -0.5 |

The source scores represent final team-level fantasy totals after bonuses,
maluses and any league-specific modifiers. **Therefore these modifiers do not
appear in `team_t` or `competition_t` calculations in 3.0.11.**

## Migration / compatibility

`include/player.h`, `src/player.c`, `player_t`, and the `player_*` functions
are replaced by `include/team.h`, `src/team.c`, `team_t`, and `team_*`.
Unlike the old `player_t`, a standalone `team_t` owns its name; it has a UUID
and no championship-points field. The CLI retains a separate local demo
standings value to preserve its old output and its test fixtures. Existing
code and binaries compiled against the public `player_t` layout must migrate
and recompile. CMake now declares `3.0.11` and installed consumers request
that version. This does not imply backwards ABI compatibility.
