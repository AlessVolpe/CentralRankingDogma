# Monte Carlo calendar experiments: descriptive sampling of counterfactual standings

## Abstract

Milestone 3.1.1 performs reproducible synchronous batches of independently generated
or stateful-chain fixture calendars. Each draw is evaluated by the deterministic
3.1.0 season resolver. Descriptive statistics are accumulated online; the complete
historical calendar is evaluated separately. The generator does **not** guarantee
uniform sampling or independent draws, and the resulting first-place frequencies
are **not** population championship-winning probabilities.

## 1. Introduction and scientific motivation

The question is how counterfactual fixture assignments change championship
outcomes when each team's historical fantasy performance is held fixed for its
original Serie A matchday. The 3.0.12 calendar generator supplies constrained
alternatives, and the 3.1.0 resolver evaluates one such calendar; 3.1.1
orchestrates repeated evaluations without changing those underlying contracts.

The original historical calendar is evaluated separately as the reference
baseline. A generator may also produce that calendar as an ordinary sample;
its appearance is not excluded from the simulation's observed distribution.
Statistical results are **descriptive for the selected sampling procedure**,
not claims about every mathematically feasible calendar.

## 2. Mathematical model

Let a source competition contain an even number of teams `N`, `R` fixture
rounds and fixed team-by-real-matchday scores. Let `k` be `R` when the full
season is FINAL, or the maximal consecutive finalized prefix length. The
engine rejects `k = 0` with `-EAGAIN` (the consuming web app can translate this
into a more informative message). For each sampled calendar `C_j`, the prior
`season_resolve` produces sporting positions and championship points.

For an event `A`, the empirical frequency over **accepted draws** is

```math
\widehat{p}_m(A)=\frac{1}{m}\sum_{j=1}^m \mathbf{1}\{C_j\in A\}.
```

A team sharing first place receives one first-place observation, with no
invented tie-break. Consequently, summed first-place counts across all teams
may exceed the accepted sample count. The sporting comparison remains exactly
`(championship_points, fantasy_total_tenths, goal_difference)`, followed by
UUID order for presentation **only**. Position counts use competition ranks:
`1,1,3` rather than dense ranking.

For home-advantage rules, the final scheduling cycle starts at zero-based
round index

```math
r_\mathrm{last}=\left\lfloor\frac{R-1}{N-1}\right\rfloor(N-1).
```

When configured, the home bonus is disabled for every round from this index
onward, even when the cycle is incomplete. With ten teams and 35 fixture
rounds, the final cycle comprises rounds 28–35. The boundary uses the full
competition's round count, even when only a finalized prefix is evaluated.

## 3. Algorithm and design

The batch creates one seeded `calendar_generator_t`, resolves the original
calendar once and then calls `calendar_generator_next` on each attempt.
Each accepted draw is evaluated by `season_resolve`, accumulated and destroyed.
The engine does not retain all sample standings or fixture results.

In **duplicate-inclusive mode**, repeated calendars contribute repeated
observations. This estimates the observed distribution of the configured
sampler, not a uniform distribution over valid schedules. In **unique-only
mode**, repeats are rejected and sampling continues until the requested number
of unique evaluated schedules, an attempt limit, cancellation, or a memory
budget. The resulting frequencies describe discovered unique schedules, not
a probability measure over the full configuration space.

A calendar is canonicalized as its ordered rounds, each holding directed
`(home_team_index, away_team_index)` pairs sorted by home then away index.
Fixture storage order is irrelevant; direction and round assignment are not.
Only the `k` evaluated rounds contribute to the canonical representation.
Thus two schedules differing solely in pending future fixtures are duplicates
for provisional experiments. A 64-bit FNV-1a hash supports a hash table and
raw-output diagnostics; collisions are resolved by comparing the complete
canonical fixture sequence. This is not `calendar_fingerprint`, which
intentionally ignores orientation.

For each team, the online Welford recurrence updates the championship-point
mean and second central moment. Reported point variance is the empirical
**population** variance `M2/m` (zero for one observation, undefined for no
observations). Goal-difference means are accumulated online. Minimum and
maximum observed championship points and rank-frequency counts are retained.

`match_rules_t` permits a fixed home-team bonus in score tenths, disabled
by default. The bonus is applied before goal-band calculation and before
victory/draw limitation predicates. `match_result_t.home_fantasy_tenths`
continues to report the **original** home performance; the resulting goals
reflect the effective score. Team fantasy totals remain tied to the real
Serie A matchday. Even team counts are required by the current calendar
model; common 8, 10 and 12 team leagues are supported without hardcoded cases.
The external input contract is responsible for respecting the underlying
Serie A season's maximum matchday count.

## 4. Invariants and correctness

**Fixed performances:** every generated schedule is evaluated with the same
historical team-by-matchday score matrix. Home advantage affects only match
resolution; it does not modify historical fantasy scores or the fantasy totals
used for ranking.

**Exact calendar identity:** a matching fingerprint is not sufficient to
reject a duplicate. The complete directed fixture representation is compared
on hash collisions. Fixture storage order is irrelevant, but home/away
orientation and evaluated round assignments are not. The original calendar
is a separate baseline and is never counted as an additional sample.

**Sporting ties:** teams tied on championship points, fantasy totals and goal
difference retain the same sporting rank; UUID ordering only makes the
presentation deterministic and never invents a champion.

**Completion and failure:** a simulation performs `requested_samples`
successful observations or stops at `max_generation_attempts`; both limits
are explicit and finite. Normal exhaustion, cancellation and
uniqueness-memory caps return an owned partial result with a distinct
completion status. Invalid input, allocation failure, arithmetic overflow,
and generator/resolver errors return negative errno, destroy private results
and clear the output pointer. No failed resolution is silently skipped.

**Tracking budget:** exact uniqueness requires explicit memory. The budget
bounds live hash-table bucket payload and allocated canonical-node payload,
including peak coexistence of old and new bucket arrays during rehash.
General library allocation, returned statistics and allocator metadata are
not covered. When tracking is disabled, distinct/duplicate counts are
**unknown**, not zero. The existing 3.0.12 generator remains unchanged.

## 5. Complexity

Excluding calendar generation and resolver costs, team rank accumulation is
`O(N)` per accepted draw, plus `O(N^2)` retained rank frequencies.
Canonicalization requires `O(kN^2)` in the worst case for insertion sorting
within each round, but typically only 4–6 fixtures per round. With tracking
disabled, the batch retains `O(N^2 + kN)` space beyond the one-shot resolver
and generator. Exact unique tracking additionally costs `O(U kN)` space for
`U` distinct evaluated schedules, plus hash-table overhead. Duplicate checks
use expected near-constant bucket lookup under ordinary hash behavior but
make no worst-case constant-time claim; hash collisions always compare full
canonical directed schedules. A requested sample count can be impossible to
reach in uniqueness-enforced mode.

## 6. Public C11 interface and ownership

`monte_carlo_run` is a synchronous ISO C11 API. On success, the caller owns
an opaque `monte_carlo_result_t` with an independent historical baseline,
UUID-indexed team summaries, and a team-by-rank frequency table. The source
competition and match rules are never modified. The historical baseline
is not included in the accepted sample count.

`monte_carlo_options_t.ranking_backend` selects heapsort (default) or AVL
for both the independent historical baseline and every accepted sample.
The selection changes neither draws, seed handling nor sporting rules.

An optional progress callback receives attempted/accepted counts at its
configured attempt interval and can cancel synchronously. Interval zero
means a terminal notification only, not cancellable interim progress.
Callbacks must not reenter the same batch.

The optional caller-owned `FILE *` receives one raw CSV row per team per
accepted observation. Fields are: accepted sample index, generator attempt,
16-hex-digit **evaluated directed fingerprint**, team UUID, sporting rank,
championship points, original fantasy score total in tenths, goal difference,
goals for and against. `monte_carlo_write_summary_csv` writes reproducibility
metadata, aggregate calendar-generation diagnostics, original baseline results,
aggregate statistics and rank-frequency columns for the caller-owned summary
stream. JSON is deferred to 3.1.3.

CSV is **not transactional**. A file can contain partial content following
`-EIO`, `-ENOMEM` or other failures, or following an explicitly partial
successful batch. Always check the returned completion status; do not label
truncated files as complete experiment datasets. Streams are never closed,
renamed or otherwise owned by the library.

## 7. Verification

`test_monte_carlo.c` exercises seeded replay, provisional and full evaluation,
exact uniqueness and attempt-budget completion, cancellation, zero-round
rejection, CSV rows and optional partial final-cycle home-advantage exclusion.
`test_monte_carlo_failures.c` injects allocation failures and checks output
transactionality. Build, sanitizer, Windows UCRT64, installed-consumer and
documentation verification remain merge gates, not implied claims of success.

## 8. Limitations

Neither finite diversity statistics nor observed first-place frequencies prove
statistical uniformity. Chain draws may be autocorrelated; neither mixing nor
effective sample size has been established. Descriptive statistics do not
include confidence intervals or assume independent observations. Integer
counters and seed/configuration are the primary reproducibility metadata;
floating-point results depend on the target's floating-point semantics and
compiler flags.

Full thesis-level evaluation, independent replications, uncertainty treatment
and benchmarking remain within the scope of milestone 3.1.2.

## References

See [calendar mathematics](calendar.md), [match rules](match.md),
[season resolution](season-resolution.md) and the [experimental methodology](../research/methodology.md) for assumptions and threats to inference.
