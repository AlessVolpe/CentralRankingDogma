# Contributing to LeaderboardSystem

Contributions must preserve the C11 library's mathematical contracts, ownership safety, and research reproducibility. The project uses **GitHub Markdown as its canonical documentation**, without a separate documentation website in this repository.

## Code changes

- Use CMake 3.16+, Ninja and a supported C11 GCC/Clang toolchain. Run the [Linux or Windows build](docs/development/building.md), [CTest suite](docs/development/verification.md) and installed-package verification where relevant.
- Keep the exported `Leaderboard::leaderboard` CMake target stable unless a versioned migration is deliberate. New public interfaces belong in `include/`; private implementation details belong in `src/internal/`.
- Describe pointer ownership, allocation rollback, error return behavior, deterministic test seeds and platform limitations.
- Retain third-party licenses and author provenance in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Documentation and research changes

- Read [scientific style guide](docs/STYLE_GUIDE.md). Each module is a scientific sub-paper with a mathematical model, algorithms, correctness argument, complexity, API/ownership, verification and limitations.
- Update the **current reference**, then the [roadmap](docs/roadmap.md) and corresponding [milestone](docs/milestones/) in the same PR. Historical snapshots in `docs/archive/` must not be silently edited.
- Use author-year hyperlinks to [bibliography.md](docs/bibliography.md), with consistent keys in [references.bib](docs/references.bib). Credit source-code adaptations separately.
- Run `python3 scripts/check_docs.py` and the [Markdown/Mermaid checks](docs/development/documentation.md) locally; CI also gates documentation quality.

## Repository contents

Track C source, tests, documentation, CI, scripts and CMake files. Ignore generated builds, per-machine IDE caches, test logs, and temporary `*.patch` / `*.diff` files. Do not commit copied binary dependencies, credentials or unlicensed copyrighted source.

For a previous narrative contribution guide, see the [historical snapshot](docs/archive/CONTRIBUTING-3.0.12.md).
