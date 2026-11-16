# Scientific documentation style guide

## Editorial contract

GitHub Markdown is the canonical format. The reference papers form chapters of a larger scientific document. Author-year citations link to [bibliography.md](bibliography.md), whose citation keys match [references.bib](references.bib). Every mathematical symbol must be defined locally or in [notation.md](notation.md). Prefer exact claims about the current implementation to aspirations presented in the present tense.

## Reference-paper structure

Each `docs/reference/*.md` must have, in order:

1. `## Abstract`
2. `## 1. Introduction and scientific motivation`
3. `## 2. Mathematical model`
4. `## 3. Algorithm and design`
5. `## 4. Invariants and correctness`
6. `## 5. Complexity`
7. `## 6. Public C11 interface and ownership`
8. `## 7. Verification`
9. `## 8. Limitations`
10. `## References`

A section can say that an asymptotic claim or formal proof is not applicable. Do not invent theorems to fill a heading. Put GitHub `math` fenced blocks around displayed equations; use inline `\(...\)` for simple symbols. Long examples can use GitHub `<details>` with a descriptive `<summary>`.

## Evidence labels

- **Contract:** implemented behavior supported by source, public headers and focused tests.
- **Proof:** mathematical statement with hypotheses and an argument.
- **Observation:** finite test or benchmark result; include seed, workload, compiler and date where material.
- **Hypothesis / planned:** not yet implemented or not yet established empirically.

Do not describe seed reproducibility as uniform sampling. Attribute external scientific work using author-year references, and source-code adaptations through [third-party notices](../THIRD_PARTY_NOTICES.md).

## Maintenance and migration

When editing a module, update its canonical reference; when a milestone changes, update its ledger record and `roadmap.md`. A historical snapshot in `archive/` must not be silently rewritten. The compatibility pointers left at former URLs keep old links navigable. Run the [documentation checks](development/documentation.md) locally and in CI before merging. The documentation-only migration does **not** change CMake's `3.0.12` version or public ABI.
