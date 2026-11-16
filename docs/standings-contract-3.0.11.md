# Championship standings contract (milestone 3.0.11)

**Scope:** Reusable, single-threaded mutable `standings_t` for head-to-head
championship W/D/L outcomes. This supplements the 3.0.11 immutable
`competition_t` snapshot and is the incremental **post-v2** addition.

## Identity, ownership and lifetime

- `standings_create(competition, &standings)` copies all 16-byte team UUIDs
  from the input snapshot, preserving its UUID-sorted `team_index_t` mapping.
  It also creates one zero-initialized `standing_entry_t` per team.
- A standings instance **owns** its UUID and entry allocations. The source
  `competition_t` can be destroyed immediately after successful creation.
- A partially finalized original calendar is a valid input; the standings
  state represents supplied outcomes, and does not claim to know the result
  of pending matches.
- `standings_entry_at` returns a **copy** of one entry. `standings_entry_for_uuid`
  resolves a canonical UUID and returns the same data. No internal mutable
  pointers escape. `standings_reset` clears W/D/L and league points while
  retaining identities.
- The snapshot is immutable, while standings are intended to be reset and
  reused for every generated calendar simulation. They are not thread-safe.

## Arithmetic and state invariants

A `standing_entry_t` records:

```c
uint32_t championship_points;
uint32_t wins;
uint32_t draws;
uint32_t losses;
```

The checked update operations maintain:

`championship_points = 3 * wins + draws`

`STANDINGS_WIN` awards three points, `STANDINGS_DRAW` one point, and
`STANDINGS_LOSS` zero. Each call increments the corresponding W/D/L counter.
No undocumented result types or overflow wraparounds are accepted.

- `standings_record_result(s, index, result)` updates one team. It is useful
  for replaying externally calculated outcomes but does **not** know whether
  a matching opponent received the corresponding result.
- **Prefer** `standings_record_match(s, home, away, home_result)` when recording
  a known fixture. It infers the away result and checks both updates before
  publishing either. If the second team overflows, neither team changes.
- The home/away argument order identifies fixture participants only; **home
  advantage is disabled**. Neither an additive bonus nor a conversion from
  fantasy-point totals to fantasy goals occurs here.
- `standings_reset(NULL)` and `standings_destroy(NULL)` are safe.
- Invalid parameters and outcome enums return `-EINVAL`; out-of-range index
  returns `-ERANGE`; unknown, valid UUID returns `-ENOENT`; points or counter
  overflow returns `-EOVERFLOW`; allocation failure returns `-ENOMEM`.

## Example

```c
standings_t *s = NULL;
if (standings_create(competition, &s) != 0) {
    /* Handle construction failure. */
}

/* The caller already knows the outcome of a match. */
if (standings_record_match(s, home_index, away_index,
                           STANDINGS_WIN) != 0) {
    /* Neither entry was modified on an arithmetic failure. */
}

standing_entry_t home;
if (standings_entry_at(s, home_index, &home) == 0) {
    /* home.championship_points == 3, home.wins == 1 */
}

standings_reset(s); /* Next calendar permutation. */
standings_destroy(s);
```

## Not in 3.0.11

`standings_t` does not rank teams, calculate goals, inspect the calendar to
validate fixture participation, implement tie-breakers, or persist results.
A 3.1.0 match-resolution layer will map fixed historical fantasy scores
and a selected calendar to win/draw/loss outcomes, then feed this collection.
The independent generic `ranking_t` API remains unchanged; integration and
league-specific ordering require a separate design decision.

## Verification

`standings_contract` tests incomplete input snapshots, detached snapshot
lifetime, binary UUID lookup, match/individual updates, reset, invalid input
and W/D/L invariants. `standings_failure_contract` tests all three allocation
sites and two-team atomicity under artificial arithmetic overflow. The
installed package consumer exercises the public C11 interface.
