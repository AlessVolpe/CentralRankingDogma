# Monte Carlo calendar experiments: descriptive sampling of counterfactual standings

## Abstract

Milestone 3.1.1 performs reproducible synchronous batches of independently generated
or stateful-chain fixture calendars. Each draw is evaluated by the deterministic
3.1.0 season resolver. Descriptive statistics are accumulated online; the complete
historical calendar is evaluated separately. The generator does **not** guarantee
uniform sampling or independent draws, and the resulting first-place frequencies
are **not** population championship-winning probabilities.

## 1. Model and statistical interpretation

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

In **duplicate-inclusive mode**, repeated calendars contribute repeated
observations. This estimates the observed distribution of the configured
sampler, not a uniform distribution over valid schedules. In **unique-only
mode**, repeats are rejected and sampling continues until the requested number
of unique evaluated schedules, an attempt limit, cancellation, or a memory
budget. The resulting frequencies describe discovered unique schedules, not
a probability measure over the full configuration space. In chain mode,
draws may be correlated. No mixing or effective sample size is claimed.

A team sharing first place receives one first-place observation, with no
invented tie-break. Consequently, summed first-place counts across all teams
may exceed the accepted sample count. The sporting comparison remains exactly
`(championship_points, fantasy_total_tenths, goal_difference)`, followed by
UUID order for presentation **only**. Position counts use competition ranks:
`1,1,3` rather than dense ranking.

## 2. Identity of an evaluated calendar

A calendar is canonicalized as its ordered rounds, each holding directed
`(home_team_index, away_team_index)` pairs sorted by home then away index.
Fixture storage order is irrelevant; direction and round assignment are not.
Only the `k` evaluated rounds contribute to the canonical representation.
Thus two schedules differing solely in pending future fixtures are duplicates
for provisional experiments. The original calendar is held as an independently
owned baseline but a generated occurrence of the original **is accepted**.

A 64-bit FNV-1a hash supports a hash table and raw-output diagnostics;
collisions are resolved by comparing the complete canonical fixture sequence.
This is not `calendar_fingerprint`, which intentionally ignores orientation.

Exact tracking requires explicit memory: the uniqueness budget bounds the live
hash-table bucket payload and allocated canonical-node payload, including
peak coexistence of old and new bucket arrays during rehash. General library
allocation, the returned statistics, and C allocator metadata are not covered.
When exact tracking is disabled, distinct/duplicate counts are **unknown**,
not zero. The fixed 3.0.12 generator remains untouched.

## 3. Algorithm and numerical accumulation

The batch creates one seeded `calendar_generator_t`, resolves the original
calendar once and then calls `calendar_generator_next` on each attempt.
Each accepted draw is evaluated by `season_resolve`, accumulated and destroyed.
The engine does not retain all sample standings or fixture results.

For each team, the online Welford recurrence updates the championship-point
mean and second central moment. Reported point variance is the empirical
**population** variance `M2/m` (zero for one observation, undefined for no
observations). Goal-difference means are accumulated online. Minimum and
maximum observed championship points and rank-frequency counts are retained.
These are descriptive values; no confidence interval or independence
assumption is introduced. Integer counters and seed/configuration are the
primary reproducibility metadata; floating-point results depend on target
floating-point semantics and compiler flags.

A simulation performs `requested_samples` successful observations or stops at
`max_generation_attempts`. Both numbers are explicit and finite. Normal
exhaustion, cancellation and uniqueness-memory caps return an owned partial
result with distinct completion status. Invalid input, real allocation
failures, arithmetic overflow, and resolver/generator errors return negative
errno, destroy private results and clear the output pointer. No individual
failed resolution is silently skipped.

An optional callback receives attempted/accepted counts every configured
attempt interval, and can cancel synchronously. Interval zero means a
terminal notification only, not cancellable interim progress. Callbacks must
not reenter the same batch.

## 4. Home advantage and partial cycles

`match_rules_t` now permits a fixed home-team bonus in score tenths, disabled
by default. The bonus is applied before goal-band calculation and before
victory/draw limitation predicates. `match_result_t.home_fantasy_tenths`
continues to report the **original** home performance; the resulting goals
reflect the effective score. The unchanged team fantasy totals remain tied
to the real Serie A matchday.

The final scheduling cycle starts at zero-based round index

```math
r_\mathrm{last}=\left\lfloor\frac{R-1}{N-1}\right\rfloor(N-1).
```

When configured, the bonus is disabled for every round from this index
onward, even when the cycle is incomplete. With ten teams and 35 fixture
rounds, the final cycle comprises rounds 28–35. This does not depend on
whether the Monte Carlo analysis currently evaluates only a finalized prefix.
Even team counts are required by the current calendar model; the common 8,
10 and 12 team leagues are supported, without hardcoded team-size cases.
The external competition input contract is responsible for respecting the
underlying Serie A season's maximum matchday count.

## 5. Ownership and exports

`monte_carlo_run` is a synchronous ISO C11 API. On success, the caller owns
an opaque `monte_carlo_result_t` with an independent historical baseline,
UUID-indexed team summaries, and a team-by-rank frequency table. The source
competition and match rules are never modified. The historical baseline
is not included in the accepted sample count.

The optional caller-owned `FILE *` receives one raw CSV row per team per
accepted observation. Fields are: accepted sample index, generator attempt,
16-hex-digit **evaluated directed fingerprint**, team UUID, sporting rank,
championship points, original fantasy score total in tenths, goal difference,
goals for and against. `monte_carlo_write_summary_csv` writes reproducibility
metadata, aggregate calendar-generation diagnostics, original baseline results,
aggregate statistics and rank-frequency
columns for the caller-owned summary stream. JSON is deferred to 3.1.3.

CSV is **not transactional**. A file can contain partial content following
`-EIO`, `-ENOMEM` or other failures, or following an explicitly partial
successful batch. Always check the returned completion status; do not label
truncated files as complete experiment datasets. Streams are never closed,
renamed or otherwise owned by the library.

## 6. Complexity and limitations

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

Neither finite diversity statistics nor observed-first-place frequencies
prove statistical uniformity. Full thesis-level evaluation, independent
replications, uncertainty treatment and benchmarking remain 3.1.2 scope.

## 7. Verification

`test_monte_carlo.c` exercises seeded replay, provisional and full evaluation,
exact uniqueness and attempt-budget completion, cancellation, zero-round
rejection, CSV rows and optional partial final-cycle home-advantage exclusion.
`test_monte_carlo_failures.c` injects allocation failures and checks output
transactionality. Build, sanitizer, Windows UCRT64, installed-consumer and
documentation verification remain merge gates, not implied claims of success.

## References

See [calendar mathematics](calendar.md), [match rules](match.md),
[season resolution](season-resolution.md) and the [experimental methodology](../research/methodology.md) for assumptions and threats to inference.
