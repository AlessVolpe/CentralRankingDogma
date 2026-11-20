# Example consumers

These examples exercise the public Central Ranking Dogma interfaces. They are
not part of the installed static library.

- `native/ranking_batch.c` is a deterministic, non-interactive C example built
  as `crd-ranking-batch` and exercised by CTest.
- `external_consumer/` is an independent CMake project that tests the installed
  `CentralRankingDogma::central_ranking_dogma` package and public headers.
- `python_subprocess_consumer/crd_consumer.py` demonstrates the versioned JSON
  subprocess protocol through the installed `crd-json` executable.

The interactive educational CLI source is maintained with the native executable
adapters in `native/cli/educational/`, not in the examples directory. Run the
build and installation procedures from the repository root; see
`docs/development/building.md` and `docs/development/packaging.md`.
