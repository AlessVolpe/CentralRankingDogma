# Native verification and evidence levels

The existing CTest matrix checks structural invariants, differential workloads, allocation injection, public headers, CLI regressions, installed-package consumption, MT19937 vectors, team snapshots, standings and calendar permutations. The full test roster must be read from the configured build with `ctest --test-dir build -N`; avoid hardcoding a count into future documentation.

## Recommended sequence

1. Configure and build the GCC Debug target using [building](building.md).
2. Run all CTests with `--output-on-failure`.
3. Repeat on Linux Clang ASan/UBSan in a fresh directory.
4. Run a Release build and the isolated installed consumer.
5. Review Windows MSYS2 UCRT64 and Linux GitHub Actions CI.
6. Run [documentation lint and link checks](documentation.md).

## Evidence interpretation

- Deterministic reference vectors verify reproducibility for their specified seeding semantics.
- Randomized fixed-seed tests explore finite cases and cannot prove all possible schedules or independent sampling.
- Error-injection tests check enumerated failure points, not arbitrary memory corruption.
- Benchmarks establish measured workloads on recorded environments, not universal complexity constants.

The original detailed [3.0.11 verification guide](../archive/verification.md) remains archived for project history. Native CI workflows remain unchanged by the documentation patch.
