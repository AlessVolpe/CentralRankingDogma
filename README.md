# LeaderboardSystem

[![C library build and smoke tests](https://github.com/AlessVolpe/LeaderboardSystem/actions/workflows/ci.yml/badge.svg)](https://github.com/AlessVolpe/LeaderboardSystem/actions/workflows/ci.yml)

**An ISO C11 library for ranking, validated Fantacalcio competitions, and constrained calendar experiments.** The implementation is designed both as a reusable developer dependency and as a reproducible foundation for scientific evaluation. It is developed alongside, but independently of, the planned Python application.

**Current native development version:** 3.1.2. PR A provides the optional composite-key AVL backend, and PR B implements the [research toolkit](research/README.md) for controlled comparisons, memory profiling and statistical analysis. Full experimental evidence is still pending independent review; version 3.1.2 is not yet a claim that the scientific evaluation is complete. External integration contracts are planned for 3.1.3.

## Read the research or use the library

- **Research path:** [Scientific documentation](docs/index.md#research-reading-path), [notation](docs/notation.md), [calendar mathematics](docs/reference/calendar.md), [match scoring](docs/reference/match.md), [season resolution](docs/reference/season-resolution.md), [Monte Carlo experiments](docs/reference/monte-carlo.md), [methodology](docs/research/methodology.md), [bibliography](docs/bibliography.md).
- **Developer path:** [Library reference](docs/index.md#developer-reading-path), [build and test](docs/development/building.md), [packaging](docs/development/packaging.md), [public headers](include/), [current C architecture](docs/architecture/internal-c.md).
- **Development record:** [Roadmap and milestone ledger](docs/roadmap.md); the original documentation is [archived](docs/archive/README.md), not discarded.

## Internal C architecture

The native dependency/ownership relationships are the center of the current architecture. Arrows indicate **uses or constructs**, not necessarily ownership; consult the [architecture contract](docs/architecture/internal-c.md) for the distinct lifetimes.

```mermaid
flowchart TB
    subgraph DOMAIN["Fantacalcio domain"]
        TEAM["team_t / UUIDs"]
        COMP["competition_t / immutable snapshot"]
        CAL["calendar_t / generator"]
        ST["standings_t / mutable results"]
        MATCH["3.1.0 deterministic match and season resolution"]
        COMP --> TEAM
        CAL --> COMP
        ST --> COMP
        MATCH --> COMP
        MATCH --> CAL
        MATCH --> ST
        MC["3.1.1 Monte Carlo batches"]
        MC --> CAL
        MC --> MATCH
    end
    subgraph CORE["Generic infrastructure"]
        HASH["Hashmap"]
        AVL["BOSTree / AVL order statistics"]
        RANK["ranking_t"]
        RNG["MT19937"]
        RANK --> HASH
        RANK --> AVL
    end
    CAL --> RNG
    MATCH --> RANK
```

Explore the additional [system boundary](docs/architecture/system.md) and [scientific data-flow](docs/architecture/data-flow.md) diagrams. Solid arrows indicate implemented module relationships. The new `MATCH --> RANK` arrow identifies the optional, composite-key AVL backend in `season_resolve_with_options()`; legacy `season_resolve()` still uses heapsort. The Python application remains separate.

## Deterministic match and season resolution (3.1.0)

The original fixture schedule is the essential research baseline. `match_rules_default()`
uses the league's thresholds 66, 70, ..., 94 and an additional goal every four
points after the eighth, with optional victory/draw limitations disabled.
`season_resolve()` evaluates either the original schedule or a validated
alternative, using the same immutable matchday scores. It supports complete
seasons and the maximal contiguous prefix of finalized rounds, then publishes
owned fixture results and ranked standings (league points, resolved-round
fantasy totals, goal difference, shared exact ties). The [match paper](docs/reference/match.md)
and [season paper](docs/reference/season-resolution.md) specify the proofs,
errors, complexity and empirical baseline. The implemented Monte Carlo
orchestrator calls this deterministic resolver for each accepted calendar.

## Monte Carlo experiments (3.1.1)

`monte_carlo_run()` evaluates seeded batches of generated calendars, retaining
an independently resolved historical baseline and online team-by-rank statistics.
It supports complete seasons or a nonempty finalized prefix, duplicate-inclusive
sampling (default), optional exact directed-prefix uniqueness, finite generation
budgets, cooperative cancellation, and raw plus aggregate CSV. The configured
sampler is **not proven uniform** and observed frequencies must not be labelled
population championship-winning probabilities. See the [Monte Carlo research
paper](docs/reference/monte-carlo.md) and [milestone](docs/milestones/3.1.1.md).

Optional home-field fantasy-score bonuses are off by default and can be
suppressed throughout the final, potentially partial round-robin cycle.

## Composite ranking and scientific evaluation (3.1.2)

`ranking_t` retains its signed-score API and supports copied fixed-size keys
with a caller-provided comparator and borrowed context. The optional AVL-backed
season resolver uses the sporting key
`(championship_points, fantasy_total_tenths, goal_difference)` while preserving
shared competition positions and UUID display order. The original heapsort is
still the default and correctness reference. `monte_carlo_options_t` selects
one backend for both its historical baseline and all sampled calendars.
The [3.1.2 research framework](research/README.md) implements paired static
and incremental ranking benchmarks, separately instrumented stage measurements,
whole-process Massif heap profiles and reproducible Monte Carlo analysis.
Performance and statistical findings require full recorded experiments and
scientific review; no performance advantage or uniform sampling is assumed.

## Mathematical foundation

For an even number of teams $N$, a league round is a perfect matching $M_r$ of the team set $V$. The strict no-rematch-until-all-opponents rule implies an unordered fixture period $P=N-1$:

```math
|M_r| = \frac{N}{2},\qquad
\biguplus_{r=0}^{N-2}M_r=E(K_N),\qquad
M_{r+N-1}=M_r.
```

Calendar generation must preserve the original **directed** encounter counts $D_0(a,b)$, including home/away orientation:

```math
D_G(a,b)=D_0(a,b),\qquad
D_G(a,b)+D_G(b,a)=C_0(a,b).
```

Separate standings have the invariant $P_{\mathrm{league}}=3W+D$, whereas the original team fantasy scores remain fixed by Serie A matchday. The generic ranking abstraction orders signed scores descending, then identifiers ascending. Detailed proofs, algorithms, asymptotic bounds and scientific limitations belong to the respective [module sub-papers](docs/index.md#module-sub-papers), not to this overview. The scheduling model follows the edge-coloring perspective of [Januario et al. (2016)](docs/bibliography.md#januario-et-al-2016) and [Ribeiro et al. (2023)](docs/bibliography.md#ribeiro-et-al-2023).

<details>
<summary>What the library does not yet do</summary>

It does not reconstruct footballer bonus/malus, integrate with a database, provide a Python web application, guarantee uniform scheduling samples, or produce thesis-level statistical inference. `calendar_t` generates fixtures, `season_resolve()` evaluates them, and `monte_carlo_run()` aggregates batches. JSON/external integration remains planned for 3.1.3.

</details>

## Quick build

Linux with CMake 3.16+, Ninja and GCC or Clang:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The exported static-library target is `Leaderboard::leaderboard`. Use [building](docs/development/building.md) for Linux and Windows UCRT64, sanitizers, and Release configurations; use [packaging](docs/development/packaging.md) for installed consumers. The CLI is an educational example, not the Fantacalcio simulator.

## Reproducibility and history

The [research methodology](docs/research/methodology.md) separates mathematical correctness, deterministic replay, diversity diagnostics, and statistical representativeness. The [milestone ledger](docs/roadmap.md) links each change to its implementation and verification evidence. Historical documents remain available in the [archive](docs/archive/README.md), with [third-party provenance](THIRD_PARTY_NOTICES.md) and [GPL-3.0-only licensing](LICENSE) preserved.
