# Automated native releases

The canonical native release version is `project(CentralRankingDogma VERSION ...)` in the root `CMakeLists.txt`. The version is updated through a reviewed pull
request. **No maintainer creates or pushes a release tag manually.** The minimum
automatically releasable version is `3.1.4`, so merging this workflow before
the version bump cannot accidentally publish the old `3.1.3` baseline.

## What happens automatically

1. Pull requests and manual runs of [Automatic native release](../../.github/workflows/release.yml)
   validate build, packaging, native CTest, installed independent consumers,
   sanitizer/libFuzzer checks, JSON schemas, native research smoke tests, and
   documentation.
   They **never** create a tag or GitHub Release.
2. Every push to `master` checks the current CMake version. If `vX.Y.Z` has
   already been published, the workflow exits successfully without building
   or republishing anything.
3. If the version is new and its tag does not exist, both Linux GCC and Windows
   MSYS2 UCRT64 packages are built and tested. Linux Clang ASan/UBSan and the
   scientific/documentation checks must also succeed.
4. Only after all required jobs succeed does the workflow create the `vX.Y.Z`
   tag **at the verified `master` commit** and publish a GitHub Release with
   generated release notes, the two archives, and a `SHA256SUMS.txt` file.
   Release notes use the [category configuration](../../.github/release.yml),
   falling back to Other changes for unlabelled pull requests.

For the first canonical CRD release, merge a reviewed PR that changes CMake and
all active version references to `3.1.4`. The resulting `master` push then
publishes `v3.1.4` automatically if all checks succeed. Subsequent versions
follow exactly the same procedure.

## Distributed files and compatibility

- `central-ranking-dogma-vX.Y.Z-linux-x86_64.tar.gz`
- `central-ranking-dogma-vX.Y.Z-windows-ucrt64-x86_64.zip`
- `SHA256SUMS.txt`

Archives contain the installed static C library, public C headers, exported
CMake package, installed `crd-json` executable, README, license and third-party
notices. Windows packages target **MSYS2 UCRT64** and require compatible UCRT64
runtime libraries/toolchain; the zip is **not** claimed to be an independent
Windows installer or a portable MSVC binary distribution. No Python application,
research datasets or historical evidence are repackaged.

Installed consumers link `CentralRankingDogma::central_ranking_dogma`; native
protocol schema `1.0` remains independent of the package release number.
`engine.id` is build-specific, so old replay selectors need not remain valid
across release builds.

## Release rules and failure recovery

- The workflow's only publishing trigger is a push to `master` with an as-yet
  unpublished CMake version. A manual **Run workflow** invocation validates
  without publication.
- The release job alone receives `contents: write` permission. Building,
  testing and preparing artifacts only require read access.
- Existing releases are never overwritten. An existing tag without a release
  causes a **failure**, rather than silently replacing its commit or assets.
- After a failed build/test job, correct the problem in a new PR or rerun the
  failed workflow run. If a new commit is merged without changing the version,
  the release workflow can try again as long as the tag was never created.
- If publishing creates an orphan tag before failure, inspect the tag and
  release state before repairing it; do not force-update an existing tag.
- Branch protection and required PR checks must still be configured in GitHub.
  A successful release workflow is not a substitute for repository permissions.
- Repository tag rulesets must permit the release job to create a new versioned
  tag. The workflow uses the built-in `GITHUB_TOKEN`; no personal access token
  or third-party publishing action is required.

To verify an archive after downloading, use `sha256sum -c SHA256SUMS.txt` in a
folder containing the two distribution archives and the checksum file. The
checksums detect altered bytes but are not a cryptographic signature of the
publisher. See [local verification](local-verification.md) and [package usage](packaging.md).
