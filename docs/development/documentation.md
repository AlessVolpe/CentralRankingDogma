# Documentation verification and contribution workflow

The canonical documentation consists of the README, `docs/index.md`, current `docs/reference/`, `architecture/`, `research/`, `development/`, `milestones/`, the bibliography and the roadmap. `docs/archive/` preserves historical source files **unchanged** and is intentionally exempt from new-style lint and link checks; its old relative links may reference the pre-migration layout.

## Run locally

Python 3.10+ (stdlib):

```sh
python3 scripts/check_docs.py
```

To run the same Markdown and Mermaid checks as GitHub Actions, install Node 22+ and run:

```sh
npm install --no-save --package-lock=false markdownlint-cli2@0.20.0 mermaid@11.12.0 jsdom@26.1.0
npx --no-install markdownlint-cli2 'README.md' 'docs/**/*.md' '#docs/archive/**'
node scripts/check_mermaid.mjs
```

## Checks and boundaries

- **Python checker:** markdown links and anchors, math/mermaid fence pairing, bibliography-key parity, milestone coverage and metadata, mandatory scientific-paper headings, and required diagram presence.
- **Markdownlint:** mechanical Markdown formatting on current documentation (not the immutable archive).
- **Mermaid parser:** semantic diagram syntax via Mermaid's `parse()` API, bootstrapped with JSDOM for Node-based label sanitization; does not render screenshots.
- **External DOI URLs:** checked in a separate, non-blocking manual/scheduled audit, not on every PR.

These checks cannot validate mathematical proof correctness or statistical validity. Any claim of *proof*, *observation* or *hypothesis* must still be reviewed scientifically. The current C library version is **3.1.0**; module and milestone scientific documentation must be updated together with implementation, and historical archives remain immutable.
