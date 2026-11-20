# Native verification and evidence levels

The existing CTest matrix checks structural invariants, differential workloads, allocation injection, public headers, CLI regressions, installed-package consumption, MT19937 vectors, team snapshots, scored-match standings, calendar permutations, 3.1.0 season resolution and 3.1.1 Monte Carlo experiments. The full test roster must be read from the configured build with `ctest --test-dir build -N`; avoid hardcoding a count into future documentation.

## Recommended sequence

1. Configure and build the GCC Debug target using [building](building.md).
2. Run all CTests with `--output-on-failure`.
3. Repeat on Linux Clang ASan/UBSan in a fresh directory.
4. Run a Release build and the isolated installed consumer.
5. Review Windows MSYS2 UCRT64 and Linux GitHub Actions CI.
6. Run [documentation lint and link checks](documentation.md).

## 3.1.0 regression evidence

The reproducible ten-team fixture has 40 FINAL matches and the remaining season pending. `match_contract` checks exact goal-band and modifier boundaries; `season_resolution_contract` checks the 40 observed fixture outcomes, the derived league totals and original/generated-calendar controls. `season_resolution_failures` covers allocation rollback, scored/legacy goal-stat completeness and arithmetic atomicity. These are code-level CTests, not claims about future matches or sampling uniformity.

## 3.1.1 merged baseline and CI evidence

The merged 3.1.1 [PR #19](https://github.com/AlessVolpe/LeaderboardSystem/pull/19)
adds `monte_carlo_contract` and `monte_carlo_failure_contract` alongside the
existing deterministic season and calendar tests. On 2026-11-18, the
post-merge `master` [C library workflow](https://github.com/AlessVolpe/LeaderboardSystem/actions/runs/44204500916),
[Release/installed-consumer workflow](https://github.com/AlessVolpe/LeaderboardSystem/actions/runs/44204500913), and
[scientific documentation workflow](https://github.com/AlessVolpe/LeaderboardSystem/actions/runs/44204500788)
all completed successfully. Such finite verification establishes build and
regression evidence for a recorded revision, not uniformity, statistical
independence, comparative ranking performance or universal memory safety.

## Current 3.1.2–3.1.4 regression boundary

Milestone 3.1.2 completed differential verification of the optional composite-key
AVL ranking backend and a [scoped synthetic evaluation](../research/results-3.1.2.md).
Milestone 3.1.3 added native JSON CLI `validate`, `generate`, `resolve`, and
`simulate` contracts, with an independent installed Python consumer. Milestone
3.1.4 PR A/B migrated package and executable identity; PR C modularizes source,
tests and research tooling without changing those contracts. Use the
[local verification matrix](local-verification.md) and [JSON CLI contract](../integration/json-cli-v1.md)
for the current test commands. The installed-consumer test and native JSON CLI
CTest run only when their corresponding build/test prerequisites are met.

## Evidence interpretation

- Deterministic reference vectors verify reproducibility for their specified seeding semantics.
- Randomized fixed-seed tests explore finite cases and cannot prove all possible schedules or independent sampling.
- Error-injection tests check enumerated failure points, not arbitrary memory corruption.
- Benchmarks establish measured workloads on recorded environments, not universal complexity constants.

The original detailed [3.0.11 verification guide](../archive/verification.md) remains archived for project history. Recorded 3.1.1 CI runs remain historical evidence about that revision; current PR C results must be evaluated independently, not inferred from earlier green checks.
