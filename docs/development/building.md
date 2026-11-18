# Build and test on Linux and Windows

## Linux Debug build

Requires CMake 3.16+, Ninja and a C11 compiler. Ubuntu/Debian example: `sudo apt install build-essential cmake ninja-build clang`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Linux Release and optional benchmarks

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEADERBOARD_BUILD_BENCHMARKS=ON -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
./build-release/leaderboard_bench 10000 3 > benchmark-10000.csv
```

## Linux ASan/UBSan

Use a separate build tree; do not switch a configured compiler in place.

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DLEADERBOARD_ENABLE_SANITIZERS=ON -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

## Windows / MSYS2 UCRT64

Run inside the **MSYS2 UCRT64 shell**, not an NMake shell. Install `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-cmake`, `mingw-w64-ucrt-x86_64-ninja`; then:

```sh
cmake -S . -B build-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-ucrt --parallel
ctest --test-dir build-ucrt --output-on-failure
```

Do not reuse old Visual Studio/NMake CMake caches. Sanitizer runtimes on Windows vary by toolchain; enabling sanitizers is optional and CMake verifies link support.

## Installed-package validation

See [packaging](packaging.md). Benchmark consumers remain optional and do not
add runtime dependencies to the installed native library. For reproducible
research workloads and the separate pinned Python environment, follow the
[3.1.2 research guide](../../research/README.md).
