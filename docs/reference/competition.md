# Competition: immutable historical calendar and fantasy scores

## Abstract

`competition_t` deep-copies externally imported teams, a full original fixture calendar and one score cell per team and mapped Serie A matchday. Its snapshot is valid for ongoing seasons where some score cells are pending.

## 1. Introduction and scientific motivation

Calendar variation is meaningful only if historical player performance is held fixed. The native boundary therefore separates original data and time mappings from generated opponents and simulation-dependent standings.

## 2. Mathematical model

With $N$ teams and $R$ league rounds, a head-to-head round contains $N/2$ fixtures and all teams exactly once:

```math
\forall r,\quad |M_r|=N/2,\quad
\forall v\in V:\ \sum_{e\in M_r}{\bf1}[v\in e]=1.
```

Every round maps to an increasing positive Serie A matchday $d_r$. A score cell $(i,r)$ is `FINAL` or `PENDING`; for FINAL scores only, the partial historical total is

```math
F_i=\sum_{r:\,status(i,r)=FINAL}s_{ir}.
```

A finalized zero is legitimate; a pending zero is not a performance observation.

## 3. Algorithm and design

`competition_create` validates each UUID, unique team identity, strict day ordering, complete round participation, complete score-cell coverage and score status, then deep-copies the inputs. Team indices are sorted by UUID bytes. It caches unordered pair encounter counts $C_0(a,b)$, which the calendar engine later preserves.

## 4. Invariants and correctness

The constructor is failure-atomic: any invalid input or allocation failure leaves `*out == NULL`. Successful construction has exactly $NR$ score cells, positive monotonically increasing real-day mappings, no self-fixtures and no team appearing twice in a round. Score totals use checked nonnegative arithmetic. All future mutation occurs in separate objects.

## 5. Complexity

The mandatory $NR$ score matrix requires $\Theta(NR)$ memory. The pair-frequency matrix has $\Theta(N^2)$ entries; construction additionally validates and indexes $O(NR)$ fixtures. UUID lookup and sorting costs depend on the indexing strategy and are not claimed as fixed amortized bounds here.

## 6. Public C11 interface and ownership

See [`competition.h`](../../include/competition.h): `competition_create/destroy`, `competition_team_at`, `competition_fixture_at`, `competition_score_at`, `competition_pair_count`, `competition_round_matchday` and `competition_is_simulation_ready`. Input strings/arrays may be freed after successful construction. Borrowed display-name pointers remain valid only while the competition exists.

## 7. Verification

Tests cover duplicate and unknown UUIDs, malformed calendars, pending-versus-final score handling, overflow, failure rollback and a real ten-team 35-round 2026/27 fixture. See [verification](../development/verification.md).

## 8. Limitations

This snapshot does not compute footballer bonus/malus, generate fixtures or decide match outcomes. `competition_is_simulation_ready` only confirms score completeness, not whether simulation software is present. Rematch periodicity is enforced by the 3.0.12 generator rather than basic 3.0.11 import.

## References

See [Januario et al. (2016)](../bibliography.md#januario-et-al-2016) for the graph model and [3.0.11 milestone](../milestones/3.0.11.md) for input-format and historical guarantees. The original implementation contract is [archived](../archive/league-fixture-contract-3.0.11.md).
