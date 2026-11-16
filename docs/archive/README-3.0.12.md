# Leaderboard system

[![C library build and smoke tests](https://github.com/AlessVolpe/LeaderboardSystem/actions/workflows/ci.yml/badge.svg)](https://github.com/AlessVolpe/LeaderboardSystem/actions/workflows/ci.yml)

As a part of my [Fantacalcio Dummy System](https://github.com/AlessVolpe/FantacalcioDummySystem.git) project, I wanted to experiment with type-safe and efficient data structures to implement a ranking algorithm.

The original CLI first stores teams in a hashmap and later transfers them to a **name-ordered** AVL tree. The reusable `ranking_t` API, implemented since 3.0.5, instead maintains a hashmap index and an order-statistics AVL tree simultaneously to support dynamic score updates and rank queries. The CLI remains a demonstration, not a Fantacalcio fixture simulator.

## Documentation

The [documentation index](docs/index.md) distinguishes current contracts
from historical milestones. Read [current architecture](docs/current-architecture.md)
and [verification](docs/verification.md) for implementation and testing.

## 3.0.11: Team identities and validated original calendars

The public `team_t` has an externally supplied UUID, a deep-owned display name
and a partial-or-complete historical fantasy sum in exact fixed-point tenths.
League standings points are simulation-dependent and are NOT stored in the
team record. The opaque `competition_t` stores a complete original fixture
calendar and explicit FINAL/PENDING fantasy scores by Serie A matchday; the
original opponent-pair frequencies are cached for subsequent permutation.
The 2026/27 10-team, 35-round calendar is included as a regression fixture.

A separate reusable `standings_t` records wins, draws and losses, accumulating
3/1/0 championship points. It can be reset for each calendar permutation and
is independent of the immutable competition snapshot. Neither layer imports
databases, generates permutations, **calculates** match outcomes, or applies
athlete-level bonus/malus rules. The generic `ranking_t` remains unchanged.
See the [3.0.11 competition contract](docs/league-fixture-contract-3.0.11.md),
[standings contract](docs/standings-contract-3.0.11.md), and
[migration notes](docs/league-3.0.11-CHANGELOG.md).

### Generic ranking mathematics (existing 3.0.5 API)

The independent `ranking_t` sorts records by descending signed 64-bit score
and breaks ties by ascending identifier. Define `a \prec b` to mean that
`a` appears before `b`:

```math
a\prec b\iff(s_a>s_b)\lor(s_a=s_b\land\mathrm{id}_a<\mathrm{id}_b),
\qquad
\mathrm{rank}(a)=\sum_{b\ne a}\mathbf{1}[b\prec a].
```

The rank is zero-based. Balanced AVL order statistics make rank/select
`O(log n)` tree operations (subject to successful identifier lookup); the
hashmap's lookup is expected `O(1)` but has a worst-case `O(n)` probe
sequence. The [ranking contract](docs/ranking-contract.md) records the
update, removal and tie-breaking invariants independently of league scoring.

## 3.0.12: Constrained calendar permutations and scientific foundations

A complete head-to-head calendar of `N` even-numbered teams is represented by
an ordered sequence of perfect matchings `M_r`: one fixture per team in every
round. The complete single round-robin period has `P = N-1` rounds and is a
**one-factorization** of the complete graph `K_N`:

```math
|M_r| = \frac N2, \quad
\biguplus_{r=0}^{N-2} M_r = E(K_N), \quad
|E(K_N)| = \frac{N(N-1)}2.
```

Our league's strict rematch rule (each team must meet every other opponent
before a rematch) has a useful consequence. The **unordered** matchings are
periodic, but home/away assignments can change between occurrences:

```math
M_{r+(N-1)}=M_r \qquad (0\le r< R-(N-1)).
```

For a season of `R = m(N-1)+q` rounds, with `0 <= q < N-1`, every pairing
in the first `q` base positions occurs `m+1` times and all remaining base
pairings occur `m` times. An allowed permutation must preserve both its
original encounter frequency and its **directed** home/away counts:

```math
C_G(a,b)=C_0(a,b), \quad D_G(a,b)=D_0(a,b), \quad
D_G(a,b)+D_G(b,a)=C_G(a,b).
```

For the included 10-team, 35-round league, `35=3*9+8`. Forty pairs meet
four times and five meet three times; the latter pairings must remain in
the ninth base round. This constraint is checked independently of whether
historical fantasy scores are finalized or pending.

The 3.0.12 module exposes two complementary approaches:

- **Constrained Fisher–Yates shuffle:** uniformly permute the base-round
  positions *within each encounter-frequency class* using unbiased bounded
  MT19937 draws. It is a baseline, not a uniform generator of calendars.
- **Alternating-cycle recoloring:** choose two same-class perfect matchings
  and exchange the round assignments of edges on an alternating cycle.
  Both resulting matchings remain perfect matchings. Optional **three-round
  recoloring** solves a bounded edge three-coloring problem:

```math
\forall v,\;\forall k\in\{0,1,2\}:\quad
\sum_{e\ni v}\mathbf{1}[c(e)=k]=1.
```

The underlying mathematics draws from **one-factorizations and graph edge
coloring** in sports scheduling, especially Januario, Urrutia, Ribeiro and
de Werra's [2016 review](https://doi.org/10.1016/j.ejor.2016.03.038),
[recoloring research (2021)](https://doi.org/10.1016/j.tcs.2021.03.029),
and a [2023 tutorial](https://doi.org/10.1111/itor.13290). The shuffle uses
Durstenfeld's [1964 algorithm](https://doi.org/10.1145/364520.364540);
the PRNG is the existing MT19937 described below.

**Seeded reproducibility is not statistical uniformity.** Two-round moves
need not connect every factorization, bounded three-round search may fail
to discover a new schedule, and chain-mode outputs can be correlated. The
implementation exposes attempt counts, search budgets, changed-round counts
and opponent-schedule fingerprints without claiming uniform sampling or
cross-sample uniqueness. Full proofs, randomized selection formulas,
complexity bounds and ownership contracts are in
[the 3.0.12 mathematical specification](docs/calendar-permutation-3.0.12.md).

For comparison, the simulation-dependent championship standings remain
separate from fixed historical fantasy scores. The 3.0.11 invariant is

```math
P_{\mathrm{league}}=3W+D,\qquad
F_i^{\mathrm{final}}=\sum_{r:\,\mathrm{status}_{ir}=\mathrm{FINAL}} s_{ir}.
```

where `s_{ir}` is the **integer fantasy score in tenths** for team `i` on
Serie A matchday `r`; pending cells do not enter the sum. Calendar generation
never applies bonus/malus again and does not calculate standings until
match resolution is introduced in 3.1.0.

## Player pool or hashmap

The player pool is implemented using a hashmap, a data structure that maps keys to values. An index is computed via a hash function \(h\), which maps the universe \(U\) of possible keys to one of the \(m\) available slots in the table:

```math
h: U \rightarrow \{0, 1, \ldots, m-1\}
```

For each key \(x\), the resulting index satisfies:

```math
0 \leq h(x) < m
```

The implementation uses open addressing with linear probing to resolve collisions, keeping the table's load factor below or equal to 0.75:

```math
\alpha = \frac{n}{m} \leq 0.75
```

where \(n\) represents the number of stored entries and \(m\) the allocated table capacity.

A well-implemented hashmap offers expected constant-time lookup, insertion and deletion, assuming a reasonable distribution of hash values. However, collisions and clustering can degrade individual operations to linear time. Insertion may also trigger a rehash, making its constant-time complexity an *amortized* expectation rather than an unconditional guarantee.

More specifically, during lookup the key is hashed and its resulting index indicates where the corresponding value should be stored. If collisions have occurred, linear probing examines subsequent slots until a matching key or an empty slot is found. Works wonders for our case of study!

The implementation also uses backward-shift deletion to preserve probe-chain continuity when entries are removed.

I find really important to say thank you to [DavidLeeds](https://github.com/DavidLeeds) for the [original implementation](https://github.com/DavidLeeds/hashmap.git). Unfortunately, I couldn't reuse his amazing work on the CMake files because it didn't fit my particular use case.

## Leaderboard or BOSTree

The leaderboard is implemented using an order-statistics tree, a variant of the binary search tree, implemented as a self-balancing AVL tree with two additional operations:

- `Select(i)`, which finds the element at the zero-based index \(i\) in sorted order;
- `Rank(x)`, which returns the zero-based position of an existing element \(x\).

Both operations have worst-case logarithmic time complexity, \(O(\log n)\), thanks to the AVL tree's height-balancing properties. The same bound applies to lookup, insertion and deletion. It's definitely pretty good, though!

To turn a regular search tree into an order-statistics tree, the nodes must also maintain information about the size of their subtrees. In this implementation, each node stores separate counts for its left and right subtrees.

Every operation that modifies the tree must preserve the invariant:

```math
\mathrm{size}(x) =
\mathrm{size}(\mathrm{left}(x)) +
\mathrm{size}(\mathrm{right}(x)) + 1
```

where:

```math
\mathrm{size}(\mathrm{NULL}) = 0
```

by definition.

These counts allow the tree to navigate directly towards a selected rank without traversing every preceding node. AVL rotations must therefore preserve not only the binary-search ordering and balance factors, but also parent relationships and subtree cardinalities.

The tree uses map semantics, meaning that keys and values are represented by separate pointers. Its comparison function determines the ordering; the eventual score-based ranking rules and tie-breaking policies belong to the higher-level ranking abstraction.

Again, it's really important to thank [phillipberndt](https://github.com/phillipberndt) for the [original implementation](https://github.com/phillipberndt/bostree.git), including the testing performed with AFL and Valgrind.

## Player id generation

### A PRNG better than rand()

One of the challenges to overcome was generating identifiers for the players. And, as most of us know, C's `rand()` function isn't exactly cut for greatness.

```c
srand(time(NULL));
rand(); // sorry little man
```

While looking into alternative pseudo-random number generators, I came across a Stack Overflow discussion about generating random numbers for fractal flames. The requirements were quite reasonable:

- It should produce relatively high-quality streams of random numbers;
- Its period should be greater than ten billion;
- It should be fast enough to offer a good performance trade-off.

The answer suggested the Mersenne Twister, a PRNG developed by Makoto Matsumoto and Takuji Nishimura. The widely used MT19937 variant generates 32-bit values and has an exceptionally long period:

```math
2^{19937} - 1
```

The algorithm was designed to improve the statistical properties of earlier pseudo-random number generators. However, it remains deterministic: the same initial state produces the same sequence, and its statistical properties do not make it cryptographically secure.

I have to thank [ESultanik](https://github.com/ESultanik) for the first adaptation of the pseudocode described in:

M. Matsumoto and T. Nishimura, "Mersenne Twister: A 623-dimensionally equidistributed uniform pseudo-random number generator," *ACM Transactions on Modeling and Computer Simulation*, Vol. 8, No. 1, pp. 3–30, 1998.

I also took the time to address Issue #3 in that implementation and use explicitly sized integer types, mainly `uint32_t` instead of `unsigned long` and `int32_t` instead of `int`, both provided by `<stdint.h>`.

### GUID generation

After tackling the randomized part of the identifier generation, I had to find a simple, not particularly elegant, yet effective solution. Stack Overflow was a saving grace once again.

The current implementation generates UUIDv4-shaped identifiers using the Mersenne Twister and a locally managed seed. I slightly improved the original approach to avoid identical initial states within the same clock tick and to handle the allocated resources correctly.

There is, however, an important distinction between producing a random-looking identifier and guaranteeing its uniqueness.

The current implementation is sufficient for experimenting with the CLI prototype, but it doesn't provide cryptographic randomness or a formal guarantee against identifier collisions. A production implementation should use a trusted UUID library or a cryptographically secure random source.

## A really simple drawing

The following diagram represents the intended architecture of the Fantacalcio Dummy System. Not all components are implemented in the current native library.

```mermaid
graph LR
    A[Player] -- go into --> B[Hash Map]
    A <-- interacts with --> E[Fantacalcio Dummy System]
    B -- do we need to --> C{rank it?}
    C -- YES: go into --> D[BOSTree]
    C -- NO: stay into --> B
    subgraph Fantacalcio Dummy System
        E <--> F[Backend application]
        E <--> G[Web client]
        F <--> id1[(Database)]
    end
```

## Development: 3.x native engine foundation

The current CMake project/package version is **3.0.12**. The numbered
sections below describe historical milestone scopes, not the current API
in isolation.

The 3.x development line turns the original executable experiment into a reusable native library. The current CLI is retained as an example consumer while the data structures are stabilized and tested before Fantacalcio-specific domain logic is introduced.

Configure and build a debug version:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

AddressSanitizer and UndefinedBehaviorSanitizer can be enabled on supported GCC/Clang builds:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLEADERBOARD_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Compiler warnings can also be treated as errors by enabling `LEADERBOARD_WARNINGS_AS_ERRORS`.

The library target is `leaderboard` (`Leaderboard::leaderboard` inside CMake). The executable prototype is built as `leaderboard_cli` and intentionally kept separate from the library.

Correctness is evaluated using deterministic regression tests, structural invariants and randomized workloads with fixed seeds. These tests establish evidence about functional behavior, but should not be confused with a performance evaluation. Future benchmarking will require controlled workloads, recorded hardware and compiler configurations, repeated measurements, and comparisons against independent implementations.

### 3.0.1: Build & test foundation

An initial CTest suite provides smoke tests for the hashmap and BOSTree implementations, while GitHub Actions runs builds on Linux/GCC and Windows/MSYS2 UCRT64. Repository tracking rules and development documentation have also been standardized to improve reproducibility and maintainability.

This milestone establishes the development and testing infrastructure without modifying the underlying data structure algorithms.

### 3.0.2: Hashmap stabilization

The native hashmap retains open addressing, linear probing and backward-shift removal. Its ownership and error contracts, invariants, and validation methods are specified in [docs/hashmap-contract.md](docs/hashmap-contract.md).

Run `ctest --test-dir build --output-on-failure` after building to execute smoke, regression, and typed-macro tests. BOSTree stabilization is addressed separately.

### 3.0.3: BOSTree stabilization

The AVL-based order-statistics tree has been stabilized by correcting tree rotations, parent-child relationships, subtree metadata, and rank/select operations. Duplicate-key handling and insertion safety have also been improved to preserve structural consistency.

Deterministic regression tests verify AVL balancing, subtree sizes, traversal order, and order-statistics invariants across sequential and randomized insertions. The implementation contracts and validation methodology are documented in [docs/bostree-contract.md](docs/bostree-contract.md).

Node deletion and post-deletion rebalancing were subsequently addressed in milestone 3.0.4.

### 3.0.4: BOSTree deletion and lifecycle management

Node deletion now maintains AVL balancing, parent links, subtree counts and zero-based order statistics, including the two-child successor-transplant case.

Retained node references that become invalid after removal or tree destruction; payload finalization occurs exactly once, and retained tombstones can be released independently of the tree's lifetime.

The `bostree_deletion` regression suite covers exhaustive deletion orders, randomized insert/remove operations and reference lifecycle cases. The API contracts, verification strategy and limitations are documented in [docs/bostree-deletion-contract.md](docs/bostree-deletion-contract.md).

The legacy CLI also receives a compiler-warning and ownership cleanup: typed player pointers replace integer-to-pointer casts, names and generated IDs have explicit lifetimes, and CTest exercises normal and invalid input.

The PRNG and UUID utilities receive targeted correctness and resource-handling fixes. CI now treats compiler warnings as errors.

### 3.0.5: Ranking abstraction

The native library now exposes an opaque, domain-neutral ranking API built on
the hashmap and BOSTree. Participants have unique string identifiers and
signed integer scores; rankings are ordered by descending score with a
stable, deterministic identifier tie-breaker. Scores can be updated without
rebuilding the entire ranking, while rank lookup and selection remain
logarithmic in the number of participants.

Fixed-seed regression and differential tests compare updates, deletions,
ordering and rank queries against an independently sorted reference model.
The API, failure and ownership contracts, complexity bounds and limitations
are described in [docs/ranking-contract.md](docs/ranking-contract.md).
The existing example CLI remains unchanged; Fantacalcio-specific scoring and
simulation logic are deferred to later milestones.

### 3.0.6: Module boundaries and BOSTree API encapsulation

The build now compiles containers, ranking and legacy utilities as separate
internal CMake object targets, then combines them into the existing
`Leaderboard::leaderboard` static library. This preserves the established
consumer target without suggesting that each component is already installed
or independently linkable.

BOSTree now exposes opaque `bostree_t` and `bostree_node_t` handles, with
borrowed key/data and validity accessors. Older `BOSTree` and `BOSNode`
typedefs remain available, but directly accessing node or tree fields is no
longer part of the public API. Structural regression tests explicitly include
the private representation; normal consumers do not. The ownership and
finalization rules from 3.0.4 remain unchanged.

Hashmap now exposes an opaque `hashmap_base_t` handle, with storage,
probing metadata, and iterators' entry layouts hidden in a private header.
The typed `HASHMAP` macros are retained, but `hashmap_init` now returns a
status and the macro wrapper owns a heap-allocated base handle. Low-level
consumers that previously embedded the complete hashmap base must migrate to
`hashmap_base_new`/`hashmap_base_destroy` and public accessors. Dedicated
white-box tests retain access to the private representation.

The compatibility boundaries, dependency layout and remaining portability work
are described in [docs/api-encapsulation-contract.md](docs/api-encapsulation-contract.md).

### 3.0.7: Portable callbacks, failure injection and installable package

The native library now offers a C11-compatible named typed-hashmap API using
context-aware adapters with correctly typed hash, comparator and key-lifecycle
callbacks. New consumers can avoid the GNU `typeof`/statement-expression
macros and incompatible function-pointer casts of the legacy wrappers. The
example CLI uses the portable API; the historical macros are retained for
migration and regression testing.

A private allocation boundary and separate fault-injection test archive
exercise constructor failure, insertion rollback and ranking score-update
consistency. A CMake install/export package exposes the familiar
`Leaderboard::leaderboard` target, with an independent installed-consumer
CTest. See [docs/portable-callbacks-failures-packaging.md](docs/portable-callbacks-failures-packaging.md)
for limitations and verification details.

### 3.0.8: Failure safety and adversarial testing

The test suite now sweeps allocation failures across ranking insertion,
score updates, hashmap growth and owned-key callbacks, and BOSTree reference
lifecycles. A separate API-edge suite tests malformed arguments and boundary
values. Input-driven hashmap and AVL state machines run reproducible,
collision-heavy and deletion-heavy workloads in CTest, and may also be built
as Clang/libFuzzer targets with sanitizer instrumentation.

No production algorithms or public ranking operations are changed. See
[docs/failure-adversarial-contract.md](docs/failure-adversarial-contract.md)
for the tested invariants, fuzzing commands, and limitations.

### 3.0.9: Portable integration and MT19937 reproducibility

The CMake source and installed package version are now 3.0.9. A new
non-interactive `leaderboard_ranking_batch` example exercises the public ranking
API, including deterministic score updates, tie-breaking, and removals.
Installed-consumer verification now requires `Leaderboard 3.0.9`, checks the
standard MT19937 seed behavior, and still uses no private headers.

MTwister now offers additive, C11-friendly `mtwister_t` and
`mtwister_new`/`mtwister_seed_standard` APIs matching published MT19937
reference vectors. The historical `MTRand`/`seedRand`/`genRandLong` sequence
remains intact for compatibility. Strict C11 public-header compilation checks
and independent seed-vector regressions run as part of the normal build.

See [docs/portability-prng-3.0.9.md](docs/portability-prng-3.0.9.md) for
seeding guarantees, non-cryptographic limitations, API naming, GNU-only legacy
wrappers, reproducibility, and external installation instructions.

### 3.0.10: Performance and release readiness

An optional deterministic C11 benchmark exercises the public hashmap,
BOSTree, and ranking APIs with fixed operation ordering, independent
correctness checks, and machine-readable CSV results. Release builds and
installed-package consumption are validated separately; absolute timings are
not used as CI acceptance thresholds. See
[docs/performance-release-3.0.10.md](docs/performance-release-3.0.10.md)
for benchmark commands, limitations, and the release checklist.

## License and third-party notices

This project is distributed under the [GNU General Public License, version 3](LICENSE).
The hashmap, BOSTree, and MTwister implementations build on existing work;
their original authors, license notices, and the complete MIT license are
recorded in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). My modifications
are tracked in the repository history. The upstream license grants remain
intact.
