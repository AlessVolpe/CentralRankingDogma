# Local verification matrix

Run the commands from the repository root. Use **new build directories** for
different compilers, configurations and sanitizer settings. The configured
CTest count is not a release invariant; inspect it with `ctest --test-dir ... -N`.

## Windows: MSYS2 UCRT64

Install the UCRT64 GCC, CMake and Ninja packages through Pacman after a
complete `pacman -Syu` update. Do not mix the MSYS2 compiler with an NMake or
Visual Studio CMake cache.

```sh
cmake -S . -B build-ucrt-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCRD_WARNINGS_AS_ERRORS=ON
cmake --build build-ucrt-debug --parallel
ctest --test-dir build-ucrt-debug --output-on-failure

cmake -S . -B build-ucrt-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCRD_BUILD_BENCHMARKS=ON -DCRD_WARNINGS_AS_ERRORS=ON
cmake --build build-ucrt-release --parallel
ctest --test-dir build-ucrt-release --output-on-failure
```

Run the native Python tests from this same shell only when its Python has the
required dependencies (for example, via matching UCRT64 Pacman packages). A
missing `pip`, `jsonschema` or `pytest` is a Python-environment problem, not a
native CTest failure. For reproducible *pinned* scientific dependencies, use a
separate Linux/WSL Python virtual environment.

## Linux / WSL: GCC and Clang

```sh
cmake -S . -B build-gcc-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCRD_BUILD_BENCHMARKS=ON -DCRD_WARNINGS_AS_ERRORS=ON
cmake --build build-gcc-release --parallel
ctest --test-dir build-gcc-release --output-on-failure

cmake -S . -B build-clang-asan -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCRD_ENABLE_SANITIZERS=ON -DCRD_WARNINGS_AS_ERRORS=ON
cmake --build build-clang-asan --parallel
ctest --test-dir build-clang-asan --output-on-failure
```

The sanitizer and libFuzzer options require compiler/runtime support. MSYS2
UCRT64 Clang need not provide the libFuzzer driver; use compatible Linux/WSL
Clang for that optional target.

## Installed C and JSON subprocess consumers

```sh
cmake --install build-gcc-release --prefix "$PWD/installed-pr-c"
cmake -S examples/external_consumer -B build-external-consumer -G Ninja -DCMAKE_PREFIX_PATH="$PWD/installed-pr-c"
cmake --build build-external-consumer --parallel
ctest --test-dir build-external-consumer --output-on-failure
"$PWD/installed-pr-c/bin/crd-json" --version
```

The installed consumer CTest also runs in the main project suite. The separate
Python process can validate the installed binary:

```sh
CRD_INSTALLED_JSON_BINARY="$PWD/installed-pr-c/bin/crd-json" python3 -m unittest discover -s tests/integration -p test_installed_json_consumer.py -v
```

## Python JSON contracts and scientific tooling

With the research/test dependencies installed:

```sh
python3 -m unittest discover -s tests/contracts -p test_json_contract.py -v
python3 -m unittest discover -s tests/tooling -p test_commit_messages.py -v
python3 -m pytest -q research/tests
python3 research/tools/run_experiments.py --smoke --native-dir build-gcc-release --output research/output/pr-c-smoke
```

The `--smoke` run is a *tooling* check, **not** a new scientific conclusion.
Scientific evidence verification requires the originally recorded experiment
artifacts and SHA-256 values; see [research procedure](../../research/README.md).

## Optional: Linux/WSL libFuzzer and Massif

```sh
cmake -S . -B build-clang-fuzz -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCRD_BUILD_FUZZERS=ON
cmake --build build-clang-fuzz --parallel
ctest --test-dir build-clang-fuzz --output-on-failure
./build-clang-fuzz/tests/fuzz_hashmap_libfuzzer -runs=1000
./build-clang-fuzz/tests/fuzz_bostree_libfuzzer -runs=1000

python3 research/tools/memory_profile.py --output research/output/pr-c-massif -- ./build-gcc-release/crd-memory-compare 8 heap 5489
```

Install Valgrind for the Massif step. Profile results must be interpreted for
the local platform and workload, not substituted for published 3.1.2 evidence.

## Documentation, policy and CI parity

```sh
python3 scripts/check_docs.py
python3 scripts/check_commit_messages.py --range origin/master..HEAD
python3 -m unittest discover -s tests/tooling -p test_commit_messages.py -v
npm install --no-save --package-lock=false markdownlint-cli2@0.20.0 mermaid@11.12.0 jsdom@26.1.0
npx --no-install markdownlint-cli2 'README.md' 'CONTRIBUTING.md' 'docs/**/*.md' '#docs/archive/**'
node scripts/check_mermaid.mjs
```

Node 22+ is used for the documentation tools. GitHub Actions additionally
checks native builds, research methodology, optional memory profiling and
release packaging on its own toolchains. Local smoke tests do not establish
that all CI jobs have passed.
