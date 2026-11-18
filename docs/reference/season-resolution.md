# Season resolution: schedule counterfactuals and championship rankings

## Abstract

Milestone 3.1.0 resolves one original or validated alternative calendar using immutable matchday-indexed fantasy performances. It publishes independently owned fixture results, championship standings and ranked positions, while preserving per-team fantasy totals under opponent permutations. The two supported scopes are a maximal finalized prefix and a fully finalized season. Ranked outputs use points, scope-local fantasy totals and goal difference, with shared sporting positions for complete ties.

## 1. Introduction and scientific motivation

The research question is how a fixed set of fantasy performances changes sporting results when the opponent schedule changes. Unlike the separately implemented 3.1.1 Monte Carlo batch engine, this module **does not** generate calendars or draw random numbers. It evaluates one fully specified schedule. The unpermuted original competition is an indispensable reference and does **not** require meeting the stronger one-factorization constraints imposed on alternative calendars. Ranked entries currently use in-place heapsort; the generic `ranking_t` is not yet integrated (planned for 3.1.2).

## 2. Mathematical model

Let an even-sized team set $V$ contain $N$ teams and a source competition have rounds $r\in\{0,\ldots,R-1\}$. Each round maps to an observed Serie A matchday $d_r$ and has exactly $N/2$ directed fixtures. The observed team score $s_{i,r}$ depends only on team $i$ and mapped day $d_r$, not the opponent chosen by a calendar. Write $k$ for the maximal finalized prefix length, so all team scores are `FINAL` for $r<k$ and either $k=R$ or at least one score of round $k$ is pending.

The invariant fantasy total for evaluated rounds is

```math
F_i(k)=\sum_{r=0}^{k-1}s_{i,r}.
```

For a match with goals $(g_h,g_a)$ computed by [`match.h`](../../include/match.h), the two corresponding records receive win/loss, draw/draw or loss/win. For each team, $P_i=3W_i+D_i$ and $G_i=W_i+D_i+L_i$. Each scored match updates the goals-for and goals-against accumulators, with $\mathrm{GD}_i=\mathrm{GF}_i-\mathrm{GA}_i$ represented in `int64_t`.

The sporting ordering key is lexicographic, descending:

```math
K_i=(P_i,F_i(k),\mathrm{GD}_i).
```

Teams with identical $K$ share their **sporting rank**; UUID order is used only for a deterministic display order. The numeric rank convention uses standard competition positions (`1, 1, 3`), not dense ranks.

## 3. Algorithm and design

`season_resolve` takes a source `competition_t`, optional selected `calendar_t`, `match_rules_t`, and a resolution scope. A `NULL` candidate means the **original competition fixtures**; a non-NULL candidate must independently pass `calendar_validate`, including matchday mapping and directed pair-frequency constraints.

**FINALIZED_PREFIX:** resolve every round before the first round containing any PENDING score; an empty prefix is valid. A FINAL score after the first incomplete round is not evaluated.

**FULL_SEASON:** require `competition_is_simulation_ready` and reject unfinished snapshots with `-EAGAIN`.

After validation, the resolver creates privately owned standings, fixture records, UUID values and ranking entries. It reads original fantasy scores by `(team_index, round_index)` even when the selected fixtures are permuted. At the end, entries are sorted and shared sporting ranks assigned. All published outputs are copies and survive source-object destruction. No partial results are exposed on failure.

## 4. Invariants and correctness

**Score invariance.** Each team contributes exactly one observed score for each selected round, so $F_i(k)$ is independent of the selected fixture matching. The implementation adds these scores in a separate team-major step, not via opponents; this prevents double-counting and links the invariant directly to the algorithm.

**Result accounting.** As each round is a perfect matching and every pair is updated atomically, each team receives exactly $k$ results. Thus $W_i+D_i+L_i=k$ and $P_i=3W_i+D_i$. Each fixture contributes its home goals once to total GF and once to the opponent's total GA, and conversely for away goals, so

```math
\sum_{i\in V}\mathrm{GF}_i=\sum_{i\in V}\mathrm{GA}_i.
```

**Ranking independence.** Because UUIDs are sorted by binary value in `competition_t`, the original team index supplies a stable presentation-only tie key. A pair tied on all three sporting quantities shares the same rank. Subsequent positions skip the occupied slots in standard competition ranking.

**Transaction boundary.** Every intermediate allocation and arithmetic check occurs before `*out` is assigned. On `-ENOMEM`, `-EOVERFLOW`, `-EINVAL` or `-EAGAIN`, all privately constructed records are destroyed and no partial season result is published. The implementation does not mutate its source objects or generator RNG state.

## 5. Complexity

For $N$ teams and $k$ resolved rounds, fixture processing, copying and the fixed eight-threshold score conversion cost $\Theta(kN)$ time; ranking costs $O(N\log N)$ through the explicit in-place heapsort. The result owns $\Theta(kN+N)$ memory (fixture records, standings, UUIDs and sorted entries). A non-NULL candidate incurs additional `calendar_validate` work and temporary space; its independent complexity is specified in the [calendar paper](calendar.md). Lookup by rank index or fixture index is constant time, excluding caller-side formatting.

## 6. Public C11 interface and ownership

[`include/season_resolution.h`](../../include/season_resolution.h) defines `season_resolution_scope_t`, `season_fixture_result_t`, `season_rank_entry_t` and the opaque `season_resolution_t`. `season_resolve` creates an owned result or sets the output pointer to NULL on failure; `season_resolution_destroy` releases every owned resource. Accessors return copies. Public indexes always use the same UUID-sorted team-index mapping as the source competition. The mutable `standings_t` remains internal to the completed season output; its public scored-match and goal-statistics APIs are covered in [standings](standings.md).

Results are single-threaded immutable snapshots after publication. Neither selected calendars nor scoring rules are modified or retained by reference.

## 7. Verification

[`test_season_resolution.c`](../../tests/test_season_resolution.c) exercises the original baseline, generated alternatives, finalized-prefix stopping rules, empty prefixes, score invariance, shared ties and ownership. The historical 2026/27 regression fixture contains ten teams, 35 scheduled league rounds and the first eight rounds finalized. Forty precomputed fixture goal pairs are checked independently, together with all ten championship point totals, scope-local fantasy totals, goal differences, $\sum P_i=106$ and $\sum GF_i=166$. These finite observations do not imply uniform calendar sampling.

[`test_resolution_failures.c`](../../tests/test_resolution_failures.c) exercises fault-injected construction and two-team rollback, legacy/scored state completeness and invalid arguments. The installed public C headers, Windows UCRT64, GCC Linux and sanitizer jobs remain release gates. No new CI result is asserted before the branch is tested on GitHub.

## 8. Limitations

The one-shot resolver does not generate Monte Carlo samples, prove a distribution of calendar outcomes, rank incomplete goal-statistics collections or select arbitrary nonconsecutive round subsets. The displayed $F_i(k)$ includes **only** resolved rounds, not any separately finalized scores occurring after a gap. All results assume opponent-independent historical fantasy performances, which is a counterfactual modeling assumption rather than an empirical causal claim.

## References

The [match scoring paper](match.md) specifies sporting goal rules. See the [competition snapshot](competition.md), [calendar mathematics](calendar.md), [standings](standings.md) and [research methodology](../research/methodology.md) for architectural assumptions and reproducibility limits. The source workbook and settings screenshot are described in the [3.1.0 milestone record](../milestones/3.1.0.md); academic scheduling context and source bibliography remain in [bibliography](../bibliography.md).
