# Roadmap and milestone ledger

This file is the **canonical documentation status ledger**, not a package release registry. `Implemented in master` means the documented milestone code is present in the merged source tree; it does **not** assert a published binary release or CI result for every historical revision. Each milestone contains its scope and source-evidence links. Dates, PR numbers and release tags are omitted when not independently verified.

| Milestone | Subject | Documentation status | Record |
| --- | --- | --- | --- |
| `3.0.1` | Build and test foundation | Implemented in master | [Details](milestones/3.0.1.md) |
| `3.0.2` | Hashmap stabilization | Implemented in master | [Details](milestones/3.0.2.md) |
| `3.0.3` | BOSTree stabilization | Implemented in master | [Details](milestones/3.0.3.md) |
| `3.0.4` | BOSTree deletion and lifecycle | Implemented in master | [Details](milestones/3.0.4.md) |
| `3.0.5` | Generic ranking abstraction | Implemented in master | [Details](milestones/3.0.5.md) |
| `3.0.6` | API encapsulation | Implemented in master | [Details](milestones/3.0.6.md) |
| `3.0.7` | Portable callbacks and packaging | Implemented in master | [Details](milestones/3.0.7.md) |
| `3.0.8` | Failure safety and adversarial tests | Implemented in master | [Details](milestones/3.0.8.md) |
| `3.0.9` | PRNG portability and integration | Implemented in master | [Details](milestones/3.0.9.md) |
| `3.0.10` | Performance and release readiness | Implemented in master | [Details](milestones/3.0.10.md) |
| `3.0.11` | Team, competition and standings model | Implemented in master | [Details](milestones/3.0.11.md) |
| `3.0.12` | Calendar permutation engine | Implemented in master | [Details](milestones/3.0.12.md) |
| `3.1.0` | Match resolution and standings integration | Implemented in current source | [Details](milestones/3.1.0.md) |
| `3.1.1` | Monte Carlo simulation engine | Implemented in master; CI verified | [Details](milestones/3.1.1.md) |
| `3.1.2` | Ranking integration and scientific evaluation | Implemented in current source; scoped synthetic evidence verified (PR #23) | [Details](milestones/3.1.2.md) |
| `3.1.3` | External integration contract and JSON CLI | PR A and PR B merged; PR C Monte Carlo transport implementation in progress | [Contract](milestones/3.1.3.md) |

## Evidence and change policy

- The 3.1.2 native source preserves 3.1.1 Monte Carlo defaults and the heapsort resolver, with PR A's optional composite-key AVL backend. PR B implements and empirically evaluates controlled ranking and synthetic-calendar workloads. The [published results](research/results-3.1.2.md) describe the measured trade-offs and scope limits; the complete external research archive is identified by SHA-256, not committed to Git. See the [research protocol](../research/README.md) and [reference papers](index.md#module-sub-papers).
- Original contracts, README milestone sections and migration notes are preserved **unchanged** in [archive](archive/README.md).
- Tests and CI are evidence for specific commits, workloads and toolchains; they do not prove all possible executions correct.
- On future releases, update this ledger and the affected milestone *in the same PR*. Record the commit, tests/CI results, breaking changes, and scientific assumptions when verified.
- The 3.1.1 implementation is merged via [PR #19](https://github.com/AlessVolpe/LeaderboardSystem/pull/19); the recorded `master` build, release and documentation checks succeeded. This supports implementation status, not claims of uniform calendar sampling or thesis-level statistical validity.
- Milestone 3.1.2 completed the composite-key ranking integration and controlled synthetic research evaluation. Milestone 3.1.3 specifies the [versioned JSON CLI boundary](integration/json-cli-v1.md) before native transport and Python consumer implementation. The Python web application remains outside this C repository and is not part of 3.1.3.
