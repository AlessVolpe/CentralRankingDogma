# Repository structure and maintenance boundaries

Central Ranking Dogma is a single installed ISO C11 library, two native CLI
adapters, independent example consumers, a test suite and research tooling.
The directory structure follows **responsibility**, not the history of when a
module was introduced. Paths below describe the 3.1.4 PR C source tree;
installed include paths and the exported CMake target remain stable.

## Ownership map

| Source location | Responsibility | Maintenance rule |
| --- | --- | --- |
| `native/include/` | Supported C11 headers and `central_ranking_dogma.h` | Document ownership and compatibility for public changes |
| `native/src/containers/` | Hashmap and BOSTree implementations | Preserve invariants and failure handling |
| `native/src/ranking/` | Generic `ranking_t` and ranking keys | Maintain scalar and custom-key contracts |
| `native/src/random/` | Deterministic MT19937 implementation | Preserve reference vectors and seed semantics |
| `native/src/domain/` | Teams, competition snapshots and standings | Keep validation and snapshot ownership explicit |
| `native/src/calendar/` | Calendar construction, shuffling and recombination | Preserve directed fixture counts and sampling limits |
| `native/src/resolution/` | Match scoring and season resolution | Keep the original schedule as an independent baseline |
| `native/src/simulation/` | Monte Carlo orchestration and aggregates | Preserve cancellation, deduplication and CSV contracts |
| `native/src/support/` | Allocation, profiling and common utilities | Keep test-only instrumentation out of production exports |
| `native/src/internal/` | Private implementation headers | Never install or expose as public API |
| `native/cli/json/` | `crd-json` request/response transport | Preserve JSON schema v1; never link JSON parsing into the library |
| `native/cli/educational/` | Interactive `crd-cli` adapter | Treat as an executable consumer, not a public ABI |
| `cmake/` | Compiler/sanitizer and installed-package rules | Keep the canonical exported target stable |
| `benchmarks/` | Optional native performance executables | Avoid asserting universal timing guarantees |
| `tests/unit/` | Focused component regressions | Keep deterministic inputs and expected outcomes |
| `tests/contracts/` | Public and JSON contract tests | Update together with interface changes |
| `tests/adversarial/` | Allocation failures, malformed inputs and fuzzing | Preserve separate instrumentation builds |
| `tests/integration/` | Installed and subprocess consumers | Exercise the actual binaries and package boundary |
| `tests/tooling/` | Development-policy tests | Keep scope validation independent of the native library |
| `tests/support/` and `tests/fixtures/` | Shared helpers and frozen fixtures | Do not conflate fixtures with measured research results |
| `examples/` | Independent C and Python consumers | Build against supported interfaces only |
| `research/tools/` | Import, run, analyze, profile and evidence verification | Run from repository root; record provenance |
| `research/results/3.1.2/` | Published scoped synthetic measurements | Immutable evidence; do not regenerate in maintenance PRs |
| `schemas/` | Versioned JSON request/response/progress contracts | Historical `leaderboard-cli-v1-*` filenames are intentional |
| `scripts/` | Documentation and commit-policy validation | Keep local commands aligned with GitHub Actions |
| `docs/reference/` | Current scientific module papers | Document contracts, assumptions, proofs and limitations |
| `docs/archive/` | Historical documents and milestone snapshots | Do not rewrite to match current naming |

## Build and installation boundaries

The root `CMakeLists.txt` defines the project and delegates to `native/`,
`benchmarks/` and `tests/`. The native object libraries are build details; the
only installed computational CMake target is
`CentralRankingDogma::central_ranking_dogma`. The installed public headers live
under the install prefix's `include/`, not `native/include/`. The installed
`crd-json` executable is a separate process adapter and is **not** part of the
static library. Benchmarks and test-only fault-injection libraries are never
exported as production dependencies.

## Change checklist

1. Update implementation and focused tests in the same change.
2. If a public contract changes, update its corresponding
   [scientific reference](../index.md#module-sub-papers), examples and installed
   consumer verification.
3. If transport changes, update [JSON CLI v1](../integration/json-cli-v1.md),
   `schemas/`, schema fixtures and Python subprocess tests together. A wire
   breaking change requires an explicit new schema version.
4. If research methodology changes, record a new experiment and provenance;
   **do not** overwrite [3.1.2 results](../research/results-3.1.2.md).
5. Update the [roadmap](../roadmap.md), relevant milestone, and
   [documentation validation](documentation.md) as necessary.

See [migration notes](migration-3.1.4.md) for renamed development interfaces
and [local verification](local-verification.md) for supported smoke tests.
