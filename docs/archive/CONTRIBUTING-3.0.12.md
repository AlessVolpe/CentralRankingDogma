# Contributing and repository tracking

The native C11 library (`leaderboard`, exported as `Leaderboard::leaderboard`)
and the example consumers (`leaderboard_cli`, `leaderboard_ranking_batch`) are
separate targets. The examples are not the Fantacalcio simulator. The normal
CTest suite now covers structural invariants, differential regressions,
failure injection, installed-consumer integration, MT19937 vectors, and the
team/player model. See the [documentation index](docs/index.md).

## GCC / Clang builds

The library and new typed-hashmap interface target ISO C11. The **deprecated**
`HASHMAP(...)` macros still use GNU `typeof`/statement expressions and are not
strict-C11 portable. GCC/Clang are exercised by CI; MSVC is not currently
validated as a supported toolchain.

On Linux, install a compiler, CMake (3.16+), and Ninja, then run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows: MSYS2 UCRT64

Open the **MSYS2 UCRT64** shell and install:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

Run the same CMake/CTest commands from the repository root. Use a **new build
directory** when switching generators or compilers; an existing NMake or
Visual Studio cache must not be reused for Ninja.

### Optional sanitizer builds

Use a separate build tree on a GCC/Clang toolchain where the sanitizer
runtimes can compile and link:

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLEADERBOARD_ENABLE_SANITIZERS=ON -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

Sanitizers are optional and may need additional runtime support on MSYS2.
The legacy CLI has received ownership and input-handling regression fixes,
but it remains a demonstration, not production league logic. See
[verification](docs/verification.md) for additional checks and limitations.

## Changes to APIs and documentation

Preserve ownership contracts: `team_t` **owns** a copied display name,
whereas the immutable `competition_t` deep-copies the entire input snapshot.
Accessors return read-only borrowed name views whose lifetime ends on
`competition_destroy`. Team UUIDs are supplied by the application, never
created by the noncryptographic legacy identifier utility. The generic
`ranking_t` continues to copy its IDs and use independent signed 64-bit
scores. The 3.0.11 public rename is a source and ABI migration: all previous
`player_t` consumers must be rewritten and rebuilt. See the
[current architecture](docs/current-architecture.md) and
[3.0.11 domain contract](docs/league-fixture-contract-3.0.11.md).

## What belongs in Git

Track source (`src/`, `include/`, `examples/`), test sources (`tests/`), CMake files,
documentation, and `.github/workflows/` CI configuration. These are necessary to reproduce
a build. Do **not** commit `build/`, `cmake-build-*/`, compiler output, CMake caches,
per-machine `.idea/` or `.vscode/` configuration, or test logs.

The root `.gitignore` stops *new* files from being tracked; existing tracked generated files
must be removed from the index with `git rm -r --cached`, without deleting local copies.

Current work on the web application, database, fixture permutations, and
Python integration is planned separately from this documentation update.
