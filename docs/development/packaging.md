# CMake packaging and independent consumers

The native modules are compiled into a single static archive; downstream CMake consumers use the installed `Leaderboard::leaderboard` target. Private module headers under `src/internal` are not part of the installed public contract.

## Linux Release installation

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEADERBOARD_WARNINGS_AS_ERRORS=ON
cmake --build build-release --parallel
cmake --install build-release --prefix "$PWD/installed"
cmake -S examples/external_consumer -B build-consumer -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PWD/installed"
cmake --build build-consumer --parallel
ctest --test-dir build-consumer --output-on-failure
```

External code includes only headers under `include/`. The `find_package(Leaderboard 3.0.12 CONFIG REQUIRED)` version request checks package compatibility configuration; it does not guarantee ABI compatibility with older `player_t` clients. The current 3.0.11+ API requires migration and rebuilding.

Legacy GNU-extension hashmap macros remain available but modern consumers should use the strict C11 typed interface. See [API architecture](../architecture/internal-c.md) and [historical portable packaging contract](../archive/portable-callbacks-failures-packaging.md).
