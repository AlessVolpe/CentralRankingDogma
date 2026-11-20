# Migration to Central Ranking Dogma (3.1.4)

Milestone **3.1.4** changes the *project and developer-tooling identity* of the
former `LeaderboardSystem` library. PR D sets the native package version to **3.1.4**; the GitHub Release will be
created automatically after the verified change reaches `master`. This document
describes the merged PR A/B/C interfaces, not a new C API or transport protocol.

## CMake consumers

Use the new package name and link target:

```cmake
find_package(CentralRankingDogma 3.1.4 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE
    CentralRankingDogma::central_ranking_dogma)
```

The installed static archive is `libcentral_ranking_dogma.a` on Linux. Use
`#include "central_ranking_dogma.h"` or individual installed public headers.
The source headers now live in `native/include/`, but the **installation**
continues to place them in `<prefix>/include/`. Neither the old `Leaderboard`
package/target nor old executable aliases are installed. Do not include
`native/src/internal/` from external consumers.

## Native executable and build configuration

| Former development name | Supported name |
| --- | --- |
| `leaderboard-json` | `crd-json` |
| `leaderboard_cli` | `crd-cli` |
| `leaderboard_ranking_batch` | `crd-ranking-batch` |
| `leaderboard_bench` | `crd-bench` |
| `leaderboard_ranking_compare` | `crd-ranking-compare` |
| `leaderboard_memory_compare` | `crd-memory-compare` |
| `leaderboard_season_experiment` | `crd-season-experiment` |
| `LEADERBOARD_ENABLE_SANITIZERS` | `CRD_ENABLE_SANITIZERS` |
| `LEADERBOARD_WARNINGS_AS_ERRORS` | `CRD_WARNINGS_AS_ERRORS` |
| `LEADERBOARD_BUILD_FUZZERS` | `CRD_BUILD_FUZZERS` |
| `LEADERBOARD_BUILD_BENCHMARKS` | `CRD_BUILD_BENCHMARKS` |
| `LEADERBOARD_RESEARCH_PROFILE` | `CRD_RESEARCH_PROFILE` |

Use new build directories or remove stale CMake caches before configuring
with the renamed options. CMake's internal target identifiers may use
underscores while produced executable filenames use hyphens.

## Python subprocess clients

The example consumer is
`examples/python_subprocess_consumer/crd_consumer.py`. Its error classes are
`CRDTransportError` and `CRDResponseError`; the native CLI test/consumer
environment variables are `CRD_JSON_BINARY` and `CRD_INSTALLED_JSON_BINARY`.
Clients still send **one JSON request to stdin per process** and parse a
single response envelope from stdout, with optional JSON-lines progress on
stderr. The [wire schema remains 1.0](../integration/json-cli-v1.md):
`validate`, `generate`, `resolve` and `simulate` retain their documented
semantics. Do not reuse build-specific `engine.id` replay selectors across
incompatible engine builds.

## Research and historical compatibility

The active Python research scripts now live in `research/tools/`; native
benchmark executables retain their `crd-*` names and build-root locations.
The historical `leaderboardsystem-3.1.2-run-manifest-v1` identifier and
`schemas/leaderboard-cli-v1-*.json` filenames **must not be rewritten** simply
because the product display name changed. They identify versioned historical
formats and published provenance. Evidence under `research/results/3.1.2/`
remains unchanged.

## What is not included

No C symbol renaming, ABI guarantee for incompatible older clients, JSON
schema upgrade, algorithm change, uniform-calendar sampling claim, Python web
application, database adapter or Docker worker is introduced by 3.1.4.

For installation commands and independent consumer tests see
[packaging](packaging.md). For the source tree see
[repository layout](repository-layout.md).
