# Match scoring: fixed-point goals and optional corrections

## Abstract

The 3.1.0 `match.h` module provides an allocation-free, deterministic conversion from two finalized team fantasy scores into a scored head-to-head result. Historical inputs are represented in integer tenths. The default profile is the actual league's 66-point first-goal threshold, four-point goal intervals through the eighth goal, and a four-point interval thereafter. Two optional corrections, *limita vittoria* and *limita pareggio*, are disabled by default. Neither a home-field advantage nor an athlete-level bonus/malus engine is present.

## 1. Introduction and scientific motivation

To compare championship outcomes under constrained fixture permutations, the same observed performance must yield the same goals regardless of opponent assignment or evaluation order. Score conversion is therefore pure: no stochastic process, global configuration, fixture identity or standings state is consulted. The domain rules follow the documented Leghe Fantacalcio *scontri diretti* options (Fantacalcio, n.d.). The user-supplied league-settings screenshot specifies the 66/4 default; the official guide explains that leagues may select variable goal bands and optional match corrections.

## 2. Mathematical model

Let $s\in\{0,\ldots,2^{32}-1\}$ denote a finalized, nonnegative fantasy score in tenths. Define strictly increasing integer thresholds $T_1=660<T_2<\cdots<T_8$ and a positive post-eighth interval $\Delta_9$.

```math
G(s)=\sum_{j=1}^{8}\mathbf{1}[s\ge T_j]
+\mathbf{1}[s\ge T_8]\left\lfloor\frac{s-T_8}{\Delta_9}\right\rfloor.
```

For the default league, $(T_1,\ldots,T_8)=(660,700,740,780,820,860,900,940)$ and $\Delta_9=40$. In particular $G(659)=0$, $G(660)=1$, $G(940)=8$, $G(979)=8$, and $G(980)=9$. Configurations for other leagues may have irregular $T_2,\ldots,T_8$ and a different $\Delta_9$. The first threshold must remain 660.

For home and away scores $(s_h,s_a)$, set $s_+=\max(s_h,s_a)$, $s_-=\min(s_h,s_a)$, $\delta=s_+-s_-$, and base goals $g_+=G(s_+)$, $g_-=G(s_-)$. The higher-scoring side is unambiguous when $s_h\ne s_a$.

- **Victory limitation:** if enabled, $g_+=g_-+1$ and $\delta\le X_v$, subtract one goal from the higher-scoring side. If $g_-=0$, this also requires the explicit `limit_victory_include_subthreshold` flag.
- **Draw limitation:** if enabled, $g_+=g_-$, $s_+>s_-$ and $\delta\ge X_d$, award one additional goal to the higher-scoring side. If $g_+=0$, this also requires the explicit `limit_draw_include_subthreshold` flag.

Both gap thresholds are in integer tenths, inclusive. The predicates are disjoint: one requires adjacent bands, the other equal bands. Correction conditions use **base** goals before applying either rule. Equal fantasy scores never activate a correction. Home/away labels never influence eligibility.

## 3. Algorithm and design

The implementation performs eight threshold comparisons, then integer division above $T_8$. A single `match_result_t` accumulates the two original scores, adjusted goals, and a `match_adjustment_t` value identifying the correction. `match_rules_default` and `match_rules_uniform` provide explicit reproducible profiles; alternative nonuniform thresholds can be assigned and checked using `match_rules_validate`.

The input score must already be FINAL. The low-level function intentionally accepts numeric values rather than a `fantasy_score_t`: it cannot silently interpret PENDING as an observation. The season resolver is responsible for status validation.

## 4. Invariants and correctness

**Threshold monotonicity.** Since $T_{j+1}>T_j$ and $\Delta_9>0$, $G$ is nondecreasing. For each threshold $T_j$ with $j\le8$, crossing the boundary changes the corresponding indicator from zero to one; for $s\ge T_8$ each $\Delta_9$ tenths adds exactly one subsequent goal. Under the validated public `uint32_t` score domain, the maximum goal count fits `uint32_t` because at least 660 tenths are required for the first goal and every further interval is positive.

**Orientation invariance.** Exchanging input home and away scores exchanges the two result goal values without affecting their multiset or correction kind, because all predicates use maximum, minimum, and difference.

**Correction exclusivity.** `limit_victory` requires unequal adjacent base goal counts; `limit_draw` requires equal base goal counts. Their intersection is empty.

**Failure atomicity.** Validation precedes assignment of the returned value. On error, a non-NULL caller-owned result is unchanged. The implementation allocates no memory.

## 5. Complexity

With exactly eight configurable thresholds, goal conversion and optional corrections use $O(1)$ time and $O(1)$ auxiliary memory. This is independent of score magnitude and number of league rounds. No floating-point rounding or random sampling is performed.

## 6. Public C11 interface and ownership

[`include/match.h`](../../include/match.h) exposes `match_rules_t`, `match_result_t`, `match_rules_default`, `match_rules_uniform`, `match_rules_validate`, and `match_resolve_scores`. All inputs are borrowed and read-only; the returned result is copied into caller memory. Errors are `-EINVAL` for invalid arguments or configurations. Uniform-profile constructors also reject threshold arithmetic that cannot be represented. No global state exists.

## 7. Verification

[`test_match.c`](../../tests/test_match.c) covers boundary-adjacent values at every default threshold, goals nine through eleven, configurable post-eighth intervals, irregular bands, invalid rule profiles, equal-score immunity, both correction predicates and their below-threshold flags, operand exchange, and error-atomic output. The [historical ten-team regression](../../tests/test_season_resolution.c) supplies independent fixture-level expectations for the first eight completed rounds. It contains no scores above the eighth goal; post-eighth behavior is tested synthetically and is specified by the league-settings screenshot, **not** inferred from the workbook.

## 8. Limitations

This module does not reconstruct athlete contributions, apply political scores, handle missing lineups, model home advantage or determine sport-specific draws beyond the documented two optional corrections. Validation of the external platform's optional corrections for every possible league configuration is not established by the unmodified historical baseline. Integer tenths must be provided by the adapter; this module does not parse decimal floats.

## References

The external rules source is [Fantacalcio (n.d.), *Impostazione delle Opzioni Calcolo*](https://leghe.fantacalcio.it/guide-leghe-fantacalcio/gestione-lega/impostazione-delle-opzioni-calcolo-79), recorded in the [central bibliography](../bibliography.md) and [BibTeX](../references.bib). The concrete 66/4 defaults are supplied by the league-settings screenshot and the [3.1.0 milestone contract](../milestones/3.1.0.md). See [season-resolution reference](season-resolution.md) for score-status gating and reproducible standings.
