# Calendar Permutation Engine: mathematics and implementation (3.0.12)

**Status:** implemented in the native C11 library. **Scope:** fixture scheduling
only, with no match resolution, point bonuses, database access or Python binding.
This is the scientific and implementation reference for `calendar.h`; the main
[README](../README.md) retains the condensed mathematical overview.

## 1. Sporting model: one-factorizations

Let `N` be an **even** team count (`N >= 2`) and `R` be the number of historical
league rounds (`R >= 1`). Each round contains exactly `N/2` fixtures; every
team participates in exactly one. Let `V={0,...,N-1}` and let
`M_r \subseteq {{a,b}: a,b \in V, a != b}` denote the unordered pairings in
round `r`. Then

```math
|M_r|=N/2, \qquad \deg_{M_r}(v)=1\quad \forall v\in V.
```

Thus each `M_r` is a **perfect matching** (one-factor). A complete cycle of
`P=N-1` rounds partitions all edges of the complete graph `K_N`:

```math
\biguplus_{r=0}^{P-1} M_r=E(K_N),\qquad
|E(K_N)|=\binom N2=\frac{N(N-1)}2.
```

This is an *ordered one-factorization*. Edge coloring, where each color is a
competition round, is a standard mathematical model for sports scheduling
(Januario et al., 2016; Ribeiro et al., 2023, listed below). The implementation
never relies on club names for graph identity: vertices are the compact,
UUID-sorted indices of `competition_t`.

### 1.1 Strict rematch cooldown and periodicity

The league rule is **no repeated opponent before every other opponent has
been faced**. With one match per team each round, the first `P` rounds
(if present) cannot repeat an unordered pair. At round `P`, each team has
met every other team. Its only permissible opponent at round `P+1` (1-based)
is then its opponent from round 1; induction gives:

```math
M_{r+P}=M_r \qquad\text{for every } r \ge 0\text{ with }r+P<R.
```

The *pairings* repeat, but home/away orientation can alternate. The engine
validates this condition on creation. A valid 3.0.11 snapshot violating the
strict cooldown is rejected with `-ENOTSUP`. It is never silently repaired.

Write `R=mP+q` with `0 <= q < P`. Pairings assigned to base slots
`0,...,q-1` appear `m+1` times; pairings in remaining base slots appear
`m` times. Hence for each unordered pair `{a,b}`:

```math
C(a,b)=m+\mathbf{1}[\mathrm{slot}(a,b)<q].
```

The `q` high-frequency slots and `P-q` low-frequency slots **cannot mix**
under a legal move, since the original pair-frequency matrix must remain
fixed. For `q=0` all `P` slots have equal frequency. For `R<P`, only `R`
base slots exist, and each represented pair appears once.

**Real regression:** `N=10, P=9, R=35=3*9+8`. Eight base positions occur
four times, but the ninth appears three times. The five pairs in the ninth
original round are consequently anchored in that base slot. Every emitted
schedule retains 40 four-encounter pairs and five three-encounter pairs.
The underlying imported matchday mapping is `league round + 3` (Serie A
matchdays 4..38), and the source calendar contains 40 finalized fixtures and
135 pending fixtures. Pending scores **do not** prevent schedule generation.

### 1.2 Directed home/away preservation

Write `D(a,b)` for the count of original matches where `a` hosts `b`. Every
candidate must satisfy both

```math
D_G(a,b)=D_{\mathrm{original}}(a,b),\quad
C_G(a,b)=D_G(a,b)+D_G(b,a)=C_{\mathrm{original}}(a,b).
```

To preserve directed counts without constructing a second optimization
problem, the generator retains the *chronological orientation sequence* of
every original pair and applies it to that pair's ordered occurrences after
reassigning its base-round slot. This is stronger than the validation
contract (which checks directed counts only) and introduces no home-advantage
bonus. Because fantasy scores stay attached to their original Serie A days,
permutations change opponents at a fixed historical performance date.

## 2. Two algorithms sharing the same valid state space

### 2.1 Baseline: constrained Fisher–Yates round shuffle

Permute base-round positions independently in the two frequency groups.
For a group containing `g` positions, Durstenfeld's in-place Fisher–Yates
algorithm performs, for `i = g-1,...,1`, a swap with uniform `j \in [0,i]`.
This is an `O(g * N)` fixture-copy procedure and, for unbiased bounded
random draws, samples permutations **within that particular group** uniformly.
It **does not** sample all one-factorizations or all legal schedules.

### 2.2 Primary: alternating-cycle recoloring of two rounds

Choose two base-round perfect matchings `M_a,M_b` in the **same** frequency
group. The graph `(V,M_a \cup M_b)` is 2-regular; its connected components
are alternating even cycles. On a chosen component `Q`, swap the two
round colors of every edge:

```math
M_a'=(M_a\setminus E(Q))\cup(M_b\cap E(Q)),\qquad
M_b'=(M_b\setminus E(Q))\cup(M_a\cap E(Q)).
```

Both new groups remain perfect matchings. The fixture multiset is unchanged.
When there is only one component, the move merely exchanges whole rounds;
`two_round_whole_swaps` counts such operations separately from proper partial
cycle moves. A given input factorization might not be connected under only
these moves, so no global reachability claim is made. The algorithm follows
the local graph recoloring perspective developed in sports scheduling
(Januario et al., 2016; recoloring work in *Theoretical Computer Science*, 2021).

### 2.3 Optional three-round bounded exact cover

Choose three base-round matchings in the same frequency group. Their union
contains `E=3N/2` original fixtures. Reassign each fixture `e` a label
`c(e) \in {0,1,2}` such that every team appears once in each new round:

```math
\forall v\in V,\ \forall k\in\{0,1,2\}:\quad
\sum_{e\ni v}\mathbf{1}[c(e)=k]=1.
```

This is an edge 3-coloring / exact-cover formulation. The implementation
uses an **iterative**, seeded randomized backtracking search with explicit
limits on attempted color placements per restart and a limit on restarts.
When a three-round move is enabled and its frequency group has at least three
base rounds, it is selected with probability `1/4` on a move attempt; other
attempts use two-round recoloring. These heuristic selection weights are
fixed in 3.0.12 and are not calibrated for a uniform stationary measure.
A complete solution that only reorders the three input rounds is rejected
as trivial. On exhaustion, no partial work is committed; the next scheduled
move still operates on a fully valid factorization. Worst-case exhaustive
search is exponential (`O(3^E)` color assignments), but actual search visits
at most `three_round_node_limit` placements **per restart**. This optional
operator can reach schedules outside simple two-round recoloring neighborhoods,
but is not proved to make the full state space connected.

## 3. Reproducibility and sampling limitations

All random choices use `mtwister_seed_standard` plus the library's published
MT19937 32-bit stream. No clocks or platform-dependent `rand()` calls are used.
To choose uniformly among the integers `0,...,b-1`, for `1<b<=2^32-1`:

```math
M=2^{32},\qquad t=M\bmod b;\qquad
X\sim\mathrm{Uniform}(\{0,\ldots,M-1\}).
```

Reject `X<t` and output `X mod b`. Since `M-t` is divisible by `b`,
all accepted residues have the same cardinality. This removes modulo bias,
but it **does not** make the *overall schedule sampler* uniform.

- **Independent (default):** reset the working base matching to the original
  on every `next` call. The stateful PRNG stream continues from its previous
  successful draw. Each sample starts from the same fixture baseline, but
  outputs are not asserted to be independent, identically distributed, or
  uniform over all legal calendars.
- **Chain:** start from the previously generated matching, with explicit
  extra `chain_warmup_moves` on the first sample and extra
  `chain_thinning_moves` on later samples. These are additional shuffle passes
  for the baseline method, or additional moves for recombination. The shuffle
  always performs one normal pass per call; `moves_per_sample` applies only to
  the recombination method. Outputs are correlated in general.
- **No deduplication:** identical schedules may be emitted repeatedly;
  batch-level uniqueness analysis is deferred to milestone 3.1.1.
- **Failure atomicity:** an allocation or validation error leaves the PRNG
  stream, chain state, and prior outputs unchanged. A repeated call after
  clearing a fault has the same first successful output as if the failure
  had never occurred.

The FNV-1a `calendar_fingerprint()` canonicalizes the unordered opponent
for each team in each round; it ignores fixture order and orientation. This
is an efficient **diagnostic**, not a collision-resistant identifier and not
proof of unique calendars. Report empirical diversity, chain autocorrelation,
accepted moves, color-search exhaustion, and performance separately. There
is **no claim of stationary distribution, mixing time, or uniform sampling**.

## 4. Complexity and bounded behavior

Let `K=N/2`, `P=N-1`, `B=min(R,P)` and `T=moves_per_sample` plus any chain
warm-up/thinning budget. An immutable generated calendar materializes all
`R*K` fixtures. A standard call therefore necessarily needs `Theta(RN)`
space for its returned calendar, and `Theta(RN)` time to materialize it.

| Operation | Time | Additional temporary storage |
| --- | --- | --- |
| Generator construction | `O(RN + BN + N^2 + B^2 N)` | `O(BN + N^2 ceil(R/P))` |
| Baseline round shuffle | `O(BN)` | `O(1)` beyond base copy |
| Two-round recoloring | `O(N)` per attempt | `O(N)` |
| Three-round recoloring | `O(A * L + AN)` per attempted move; `A` restarts each of `L` nodes | `O(N)` |
| Full candidate validation | `O(RN + N^2)` | `O(N^2 + BN)` |
| Generated calendar storage | `Theta(RN)` | `Theta(RN)` owned |

Generator initialization presently checks base-pair uniqueness by comparing
earlier partner lookups, hence the conservative `O(B^2 N)` component.
Search bounds limit work per requested attempt; they do not guarantee a new
calendar. Failure-safe allocation and validation also contribute to runtime.

## 5. Public C11 API and ownership

`calendar.h` provides `calendar_options_default()`, creation/destruction,
`calendar_generator_next()`, read-only matchday/fixture accessors,
`calendar_validate()` and `calendar_fingerprint()`.

- The generator **borrows** its `const competition_t*`: the source must
  outlive it. Creating a generator on a structurally valid but cooldown-
  incompatible competition returns `-ENOTSUP`.
- Each successful `calendar_t` has an independently owned copy of its
  fixtures and matchday mapping. It survives generator destruction. Team
  indices refer to its original UUID-sorted competition.
- A successful call supplies complete `calendar_diagnostics_t` for that call.
  `calendar_generator_next()` clears `*out` before any error. Pending fantasy
  scores are acceptable; match resolution is deliberately absent.
- The two frequency groups can be too small for nontrivial moves. In that
  case the engine produces a valid possibly unchanged calendar, not a hang.
- All production allocations, including scratch, use the library allocation
  boundary; fault injection is compiled only into dedicated tests.

## 6. Verification and scientific reproducibility

`test_calendar` exercises N=2,4,8,10,12 with complete/incomplete cycles,
independent/chain modes, both methods, seed replay, and the original
2026/27 10-team/35-round TSV fixture. `test_calendar_failures` tests
allocation-failure rollback, rejection of invalid source schedules, semantic
validation errors and bounded three-round exhaustion. `test_calendar_sampling`
empirically measures seeded baseline and recombination diversity. For the
small `K_4` baseline, six possible round orders give a reference distribution;
with `S` samples and `K=6` possible permutations, the reported Pearson
statistic is:

```math
\chi^2=\sum_{i=1}^K\frac{(O_i-S/K)^2}{S/K}.
```

This is a smoke diagnostic with a loose cutoff, not a proof of independent
uniform random calendars and not a test of stationary distributions. The user fixture's
35-round test verifies both directed home/away counts and the anchored
ninth-round matching for every emitted calendar.

Recorded study results should include: the original calendar fingerprint,
team UUID mapping, input dataset version, program commit SHA, seed,
algorithm/mode/options, sample index, all diagnostics, compiler flags,
hardware/runtime, and implementation version. Separate computational
performance analysis from sporting-outcome simulation once 3.1.0/3.1.1 exist.

## References and attribution

1. T. Januario, S. Urrutia, C. C. Ribeiro, and D. de Werra,
   **"Edge coloring: A natural model for sports scheduling,"** *European
   Journal of Operational Research* **254**(1), 1–8 (2016).
   DOI: [10.1016/j.ejor.2016.03.038](https://doi.org/10.1016/j.ejor.2016.03.038).
   Graph modeling, one-factorizations and local-search neighborhoods.
2. **"Recoloring subgraphs of K₂ₙ for sports scheduling,"** *Theoretical
   Computer Science* **877**, 36–45 (2021).
   DOI: [10.1016/j.tcs.2021.03.029](https://doi.org/10.1016/j.tcs.2021.03.029).
   Inspiration for recoloring cycles and recognizing limitations of local moves.
3. C. C. Ribeiro and colleagues, **"A tutorial on graph models for scheduling
   round-robin sports tournaments,"** *International Transactions in
   Operational Research* **30**(6), 3267–3295 (2023).
   DOI: [10.1111/itor.13290](https://doi.org/10.1111/itor.13290).
4. R. Durstenfeld, **"Algorithm 235: Random permutation,"** *Communications
   of the ACM* **7**(7), 420 (1964). DOI:
   [10.1145/364520.364540](https://doi.org/10.1145/364520.364540).
   In-place Fisher–Yates shuffle and unbiased bounded choices.
5. M. Matsumoto and T. Nishimura, **"Mersenne Twister: A 623-dimensionally
   equidistributed uniform pseudo-random number generator,"** *ACM
   Transactions on Modeling and Computer Simulation* **8**(1), 3–30 (1998).
   Existing MT19937 source and references: [MT19937 portability contract](portability-prng-3.0.9.md).

The implementation is original project code following these mathematical
ideas, not a transcription of the cited articles' implementations. Its
reported validity and reproducibility are testable properties; sampling
uniformity and full reachability remain open scientific limitations.
