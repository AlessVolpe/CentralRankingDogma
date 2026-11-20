# 3.1.4 final release verification and compatibility audit

**Status:** PR D (#32) release candidate. The source-level checks and release
packaging workflow are defined here; the `v3.1.4` tag and GitHub Release must
**not** be recorded as published until the post-merge job succeeds. Refer to
[release automation](release-automation.md) for its triggers and recovery rules.

## Acceptance matrix

| Boundary | Required evidence | Interpretation |
| --- | --- | --- |
| Linux native | Release GCC build, CTest, Clang ASan/UBSan, two bounded libFuzzer campaigns | Finite regression/diagnostic checks on hosted Linux runners |
| Windows native | MSYS2 UCRT64 GCC Release build and CTest | UCRT64-compatible build, **not** MSVC or a standalone installer |
| Public C package | Installed `libcentral_ranking_dogma.a`, public C11 headers, CMake config/targets and independent `find_package` consumer | New package identity only; no legacy `Leaderboard` alias |
| JSON subprocess | Installed `crd-json --version`, independent Python integration and JSON schema tests | Protocol `schema_version: "1.0"` remains separate from native `3.1.4` |
| Release assets | Offline source-metadata, `.tar.gz`/`.zip` layout and package-version checks, SHA-256 verification | Contents/integrity check, **not** a signature or a reproducible-build proof |
| Scientific tooling | Unit tests, bounded native smoke, analysis smoke | No new statistical inference, calendar uniformity or dataset claims |
| Active documentation | `scripts/check_docs.py`, Markdownlint, Mermaid, release notes categories | Current navigation and metadata consistency; not a mathematical proof |
| Provenance | Review Git diff of archives and 3.1.2 evidence against the PR base | Historical records are outside the release package and must remain untouched |

## Compatibility audit

- **Supported public identity:** `CentralRankingDogma` CMake package,
  `CentralRankingDogma::central_ranking_dogma` link target, installed
  `central_ranking_dogma.h`, `libcentral_ranking_dogma.a`, and `crd-json`.
  The package and executable renames deliberately break older build scripts.
  No backward-compatible `Leaderboard` package aliases are installed.
- **C interface:** ranking, hashmap, BOSTree, calendar, team, competition,
  standings, match/season resolution and Monte Carlo entry points preserve the
  documented current public contracts. No blanket ABI compatibility with old
  `player_t`-based clients is claimed. Check actual installed headers and the
  independent compiled C consumer rather than assuming compatibility from a
  version number alone.
- **JSON protocol:** request/response/progress schema filenames remain
  `leaderboard-cli-v1-*.schema.json` to preserve the versioned format.
  Their canonical `$id` URLs point at `AlessVolpe/CentralRankingDogma`.
  Wire `schema_version` remains `"1.0"`; `engine.version` reports the native
  package and build-specific `engine.id` is not guaranteed across builds.
- **Algorithms and evidence:** heapsort is still the default resolver backend;
  composite-key AVL remains optional. Fixed seeds and documented inputs give
  the scoped deterministic contract, not uniform calendar sampling. Preserve
  `docs/archive/`, `research/results/3.1.2/`, and historical manifest strings,
  including `leaderboardsystem-3.1.2-run-manifest-v1`.
- **Licensing:** release archives contain `LICENSE` and
  `THIRD_PARTY_NOTICES.md`; GPL-3.0-only is the project license, with existing
  source-specific third-party notices retained.

## Verify the release checks before merge

From the project root after building an installed Release configuration:

```sh
python3 scripts/verify_release_bundle.py source --repo .
python3 -m unittest discover -s tests/tooling -p test_release_verification.py -v
python3 scripts/check_docs.py
```

GitHub Actions runs the archive verifier against the **finished distribution**
for each supported platform, after compilation and installed-consumer testing.
The publish job downloads both archives, constructs `SHA256SUMS.txt`, and
runs the verifier over the manifest and both archive payloads before calling
`gh release create` on the verified merge commit.

To audit historical evidence differences without modifying it:

```sh
git fetch origin master
git diff --exit-code origin/master...HEAD -- docs/archive research/results/3.1.2
```

The diff check is intended **before merge**, when `origin/master` is still the
PR base. A passing output means no tracked changes within those two paths for
this PR; it is not proof that external research archives were regenerated or
that their published external SHA-256 values were reverified.

## Post-merge release acceptance

Once PR D has passed required checks and been merged, the workflow on `master`
should detect CMake version `3.1.4` and attempt publication automatically.
Only **after a successful publication job**, record the release as completed.

```sh
gh release view v3.1.4 --repo AlessVolpe/CentralRankingDogma \
  --json tagName,isDraft,isPrerelease,publishedAt,assets
```

Confirm that the tag targets the verified `master` commit, the release is
neither draft nor prerelease, both archives are attached, and `SHA256SUMS.txt`
is attached. To independently validate downloaded artifacts in one directory:

```sh
python3 scripts/verify_release_bundle.py checksums \
  --directory /path/to/downloaded-assets --version 3.1.4
```

If publication fails, do **not** mark 3.1.4 as released or overwrite a tag.
Review the workflow logs, repository contents permissions, tag ruleset and
existing GitHub Release/tag states. Document the actual post-merge run URL and
release SHA separately when known; do not pre-fill results in the milestone
ledger. Archives and checksums are integrity-checked but not signed, and
compressed archives are not promised bit-for-bit reproducible across reruns.
