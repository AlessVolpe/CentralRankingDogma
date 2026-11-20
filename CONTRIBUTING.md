# Contributing to Central Ranking Dogma

Contributions must preserve the C11 library's mathematical contracts, ownership safety, and research reproducibility. The project uses **GitHub Markdown as its canonical documentation**, without a separate documentation website in this repository.

## Code changes

- Use CMake 3.16+, Ninja and a supported C11 GCC/Clang toolchain. Run the [Linux or Windows build](docs/development/building.md), [CTest suite](docs/development/verification.md) and installed-package verification where relevant.
- Keep the exported `CentralRankingDogma::central_ranking_dogma` CMake target stable unless a versioned migration is deliberate. New public interfaces belong in `native/include/`; private implementation details belong in `native/src/internal/`. Consult the [repository ownership map](docs/development/repository-layout.md) before adding native modules, adapters, tests or research tools.
- Describe pointer ownership, allocation rollback, error return behavior, deterministic test seeds and platform limitations.
- Retain third-party licenses and author provenance in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Documentation and research changes

- Read [scientific style guide](docs/STYLE_GUIDE.md). Each module is a scientific sub-paper with a mathematical model, algorithms, correctness argument, complexity, API/ownership, verification and limitations.
- Update the **current reference**, then the [roadmap](docs/roadmap.md) and corresponding [milestone](docs/milestones/) in the same PR. Historical snapshots in `docs/archive/` must not be silently edited.
- Use author-year hyperlinks to [bibliography.md](docs/bibliography.md), with consistent keys in [references.bib](docs/references.bib). Credit source-code adaptations separately.
- Run `python3 scripts/check_docs.py`, the [Markdown/Mermaid checks](docs/development/documentation.md), and the appropriate [local verification matrix](docs/development/local-verification.md) commands; CI also gates documentation quality.

## Commit and pull request naming

**Every new commit and pull request title must have a scoped Conventional
Commits subject.** Use the form `type(scope): imperative description`, or
`type(scope)!: imperative description` for an intentionally breaking change.
Keep the scope lowercase and specific to the component affected.

Examples:

```text
feat(simulation): add deterministic Monte Carlo summaries
fix(match): preserve home advantage in partial final cycles
test(calendar): cover directed fixture uniqueness
docs(research): clarify nonuniform sampling limitations
ci(actions): upgrade GitHub Actions dependencies
build(cmake): update installed consumer requirements
feat(api)!: replace an incompatible public interface
```

The **canonical allowlists** for commit types and scopes are maintained in
[`scripts/commit_policy.json`](scripts/commit_policy.json). The active native-C
scope families include:

- **Infrastructure:** `core`, `api`, `hashmap`, `bostree`, `ranking`, `prng`, `cli`.
- **Fantacalcio domain:** `domain`, `team`, `competition`, `calendar`, `match`, `standings`.
- **Simulation and research:** `simulation`, `sampling`, `statistics`,
  `evaluation`, `benchmarks`, `research`.
- **Development and delivery:** `actions`, `cmake`, `packaging`, `testing`,
  `fuzzing`, `tooling`, `deps`, `release`, `security`, `linux`, `windows`,
  `repo`, `commits`.
- **Documentation and integration:** `docs`, `readme`, `contributing`,
  `license`, `integration`, `serialization`.

Reserve `core` for changes crossing native infrastructure boundaries, not as
an all-purpose substitute for a more precise scope. The removed historical
`player` abstraction is not an active scope. Scope names for the future Python
application are not part of this repository's policy.

The **Validate scoped commits** GitHub Actions job checks the PR title and
all commits introduced by a PR, and rechecks commits pushed to `master`.
Earlier unscoped history is grandfathered in: the validator never rewrites or
scans unrelated previous commits. Run the same policy locally:

```sh
python3 scripts/check_commit_messages.py --title "docs(contributing): document commit policy"
python3 scripts/check_commit_messages.py --range origin/master..HEAD
python3 -m unittest discover -s tests/tooling -p 'test_commit_messages.py' -v
```

**Repository setting required:** allow *squash merging only* and configure the
squash commit subject to use the pull request title. GitHub's default merge
commit subjects (for example `Merge pull request #19`) do not meet this policy.
Set a branch ruleset for `master` that requires the **Validate scoped commits**
status check, blocks direct pushes, and prevents bypasses where possible.
CI detects violations; without protected merges it cannot prevent someone from
pushing an invalid subject directly to `master`. Squash merging also avoids
replaying unscoped historical merge subjects.

When a new native subsystem genuinely needs its own scope, update the JSON
allowlist, its regression tests and this guide together. No need to add scopes
for the future Python application until its separate repository and pipeline
are established.

## Repository contents

Track C source, tests, documentation, CI, scripts and CMake files. Ignore generated builds, per-machine IDE caches, test logs, and temporary `*.patch` / `*.diff` files. Do not commit copied binary dependencies, credentials or unlicensed copyrighted source.

For a previous narrative contribution guide, see the [historical snapshot](docs/archive/CONTRIBUTING-3.0.12.md).
