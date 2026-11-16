# Calendar permutation: constrained one-factorizations and recoloring

## Abstract

The 3.0.12 generator creates alternative fixture calendars while preserving the historical competition's directed home/away encounters and a strict no-rematch-until-all-opponents rule. It supports seeded round shuffling, two-round alternating-cycle changes and optional bounded three-round recoloring.

## 1. Introduction and scientific motivation

Sports scheduling can be expressed as edge coloring of complete graphs (Januario et al., 2016; Ribeiro et al., 2023). Recoloring moves enable local exploration of valid one-factorizations (Urrutia et al., 2021). This implementation is a reproducible experiment generator, **not** a proven uniform sampler of all valid calendars.

## 2. Mathematical model

Let $N$ be even, $P=N-1$, $V=\{0,\ldots,N-1\}$, and $M_r$ the unordered perfect matching at round $r$. The no-rematch rule implies one-factorization and periodicity:

```math
|M_r|=N/2,\qquad \biguplus_{r=0}^{P-1}M_r=E(K_N),\qquad M_{r+P}=M_r.
```

For a season of $R=mP+q$ rounds, $0\le q<P$, pairings in the first $q$ base positions occur $m+1$ times, while other pairings occur $m$ times. Preserving original pair counts means transformations cannot move fixtures across incompatible frequency classes. Preserve directed home/away counts as well:

```math
D_G(a,b)=D_0(a,b),\qquad D_G(a,b)+D_G(b,a)=C_0(a,b).
```

## 3. Algorithm and design

**Reference baseline:** constrained Fisher–Yates uniformly reorders eligible *base-round positions* within each frequency class, without claiming uniformity over all valid fixture recombinations (Durstenfeld, 1964).

**Primary local move:** take two compatible perfect matchings. Their union decomposes into even alternating cycles; swapping the round colors along a cycle preserves one incidence of each color at every vertex. The matching invariant survives the move.

**Optional three-round diversification:** color three rounds' selected edge multiset with colors $0,1,2$, requiring:

```math
\forall v\in V,\ \forall c\in\{0,1,2\}:\quad
\sum_{e\ni v}{\bf1}[color(e)=c]=1.
```

The search is bounded in randomized restarts and explored nodes. Accepted moves expand the base period across the season while maintaining original directed fixture multiplicities.

<details>
<summary>Four-team periodicity example</summary>

For $N=4$, $P=3$. A valid base is `AB|CD`, `AC|BD`, `AD|BC`. Under the rematch constraint, round 4 must contain the same unordered pairs as round 1, even if home/away orientation changes between encounters. Arbitrarily shuffling season rounds would generally violate this invariant.

</details>

## 4. Invariants and correctness

`calendar_validate` independently checks dimensions, one fixture per team and round, matchday mapping, original directed pair counts and the strict cooldown. A generated calendar does not alter the source `competition_t`. Generator errors do not advance its published RNG/chain state. Successful outputs can equal the original or a previous generated calendar.

## 5. Complexity

Fixture storage is $\Theta(NR)$. A full base-round shuffle is $O(P)$ round swaps when matchings are represented as references, with reconstruction/output costs dependent on fixture copying. Two-round cycle detection and recoloring visit $O(N)$ vertices/edges per candidate pair. Three-round bounded backtracking can have exponential unrestricted search complexity; the implementation enforces explicit restart and node budgets. Generation cost depends on configured move and search budgets. Do not infer constant-time or unbiased global sampling from these local operations.

## 6. Public C11 interface and ownership

See [`calendar.h`](../../include/calendar.h). `calendar_options_default` supplies an explicit seed and default recombination options; callers can choose `CALENDAR_METHOD_ROUND_SHUFFLE` or `CALENDAR_METHOD_RECOMBINE`, and `CALENDAR_SAMPLING_INDEPENDENT` or `CALENDAR_SAMPLING_CHAIN`. `calendar_generator_create` borrows the original competition; `calendar_generator_next` returns an independent `calendar_t` and optional diagnostics. `calendar_fingerprint` uses FNV-1a over a canonical unordered representation, not collision-proof deduplication.

## 7. Verification

CTest includes calendar contract, fault handling and sampling diagnostics; the 10-team 35-round fixture is a critical incomplete-cycle regression. The real dataset has 40 pairs meeting four times and five pairs meeting three times. Repeatability tests and diversity counts are **finite observations**, not proof of uniform distribution. See [research methodology](../research/methodology.md).

## 8. Limitations

Two-round alternating-cycle moves need not connect every valid one-factorization; three-round bounded search might fail to find a new schedule. Chain samples are correlated. Neither mixing time nor uniformity over the full feasible state space has been established. There is no enforced cross-sample uniqueness in 3.0.12.

## References

[Januario et al. (2016)](../bibliography.md#januario-et-al-2016), [Urrutia et al. (2021)](../bibliography.md#urrutia-et-al-2021), [Ribeiro et al. (2023)](../bibliography.md#ribeiro-et-al-2023) and [Durstenfeld (1964)](../bibliography.md#durstenfeld-1964). The complete original algorithm derivation is preserved in the [3.0.12 archived document](../archive/calendar-permutation-3.0.12.md).
