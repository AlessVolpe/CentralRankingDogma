# Standings: mutable championship results

## Abstract

`standings_t` accumulates league points and win/draw/loss counters for a particular simulated or externally supplied sequence of match results. It is separate from the immutable team-performance snapshot.

## 1. Introduction and scientific motivation

A team can win different numbers of matches under different calendars despite unchanged fantasy scores. Standings must therefore be fresh per simulation and never stored as permanent attributes of `team_t`.

## 2. Mathematical model

For each team, with wins $W$, draws $D$ and losses $L$, the standard points invariant is

```math
P_{\rm league}=3W+D,\qquad G=W+D+L.
```

`G` is the number of recorded results. This is an **accumulation rule**, not a sporting tie-breaker or match outcome function.

## 3. Algorithm and design

An individual result increments its relevant counter, awarding 3/1/0 league points. A complete match updates its two opponents as win/loss, draw/draw or loss/win. All checked increments must fit within unsigned 32-bit integer bounds before either participant changes.

## 4. Invariants and correctness

`standings_record_match` is atomic across both teams, including overflow failures. The point identity remains true after any successful update and after `standings_reset`. A standings collection copies team UUIDs and can outlive its source competition.

## 5. Complexity

Storing one record per team uses $\Theta(N)$ space. Access by index and local result arithmetic are constant-time operations after validation. UUID lookup may require searching the retained identifier mapping, so no unspecified bound is promised.

## 6. Public C11 interface and ownership

[`standings.h`](../../include/standings.h) exposes `standings_create`, `standings_entry_at`, `standings_entry_for_uuid`, `standings_record_result`, `standings_record_match`, `standings_reset` and `standings_destroy`. The `home` argument to `standings_record_match` identifies the fixture's first side and **does not introduce home advantage**.

## 7. Verification

Tests cover 3/1/0 accumulation, counters, UUID/index lookup, reset, invalid results, atomic overflow and allocation failures. See [verification](../development/verification.md).

## 8. Limitations

`standings_t` does not compute fantasy goals, apply bonus/malus or choose tie-breaking rules. The future 3.1.0 resolver will supply its results; the generic `ranking_t` remains a separate abstraction.

## References

The implemented scoring rule is a project league convention, not a theorem derived from external research. See [3.0.11 milestone](../milestones/3.0.11.md) and the [archived standings contract](../archive/standings-contract-3.0.11.md).
