# CMake packaging and independent consumers

The native modules are compiled into a single static archive, `libcentral_ranking_dogma.a` on Linux. Independent CMake consumers use `find_package(CentralRankingDogma 3.1.3 CONFIG REQUIRED)` and link `CentralRankingDogma::central_ranking_dogma`. The portable umbrella header `central_ranking_dogma.h` and individual public module headers are installed; private headers under `src/internal` are not. This is a deliberate clean break: no `Leaderboard` CMake package or target alias is installed.

## Linux Release installation

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-release --parallel
cmake --install build-release --prefix "$PWD/installed"
cmake -S examples/external_consumer -B build-consumer -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PWD/installed"
cmake --build build-consumer --parallel
ctest --test-dir build-consumer --output-on-failure
```

The same installation also supplies `installed/bin/leaderboard-json`, a
standalone CLI whose implementation is not part of the exported C target. To
exercise the installed binary rather than an in-tree executable:

```sh
installed/bin/leaderboard-json --version
LEADERBOARD_INSTALLED_JSON_BINARY="$PWD/installed/bin/leaderboard-json" \
  python3 -m unittest discover -s tests -p test_installed_json_consumer.py -v
python3 examples/python_subprocess_consumer/leaderboard_consumer.py \
  "$PWD/installed/bin/leaderboard-json" \
  tests/fixtures/json_cli_v1/validate.request.json
```

The Python client uses only the standard library. It starts one subprocess per
JSON request, separates JSON stdout from JSON-lines progress stderr, checks
the response envelope and process status, and imposes a configurable timeout.
It is a demonstration consumer, not the future Python worker or web app.
A build-specific `engine.id` is required for generated-calendar replay.

External code includes only installed public headers. The new package uses the existing native version `3.1.3` during PR A; the final 3.1.4 milestone release number is decided in PR D. The package rename is a breaking *build integration* change but does not rename exported C functions or domain types. The version check is not a guarantee of ABI compatibility with older `player_t` clients.

Legacy GNU-extension hashmap macros remain available but modern consumers should use the strict C11 typed interface. See [API architecture](../architecture/internal-c.md) and [historical portable packaging contract](../archive/portable-callbacks-failures-packaging.md).
