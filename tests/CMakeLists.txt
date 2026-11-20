add_executable(test_hashmap test_hashmap.c)
target_link_libraries(test_hashmap PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_hashmap)
add_test(NAME hashmap_smoke COMMAND test_hashmap)

add_executable(test_hashmap_regressions test_hashmap_regressions.c)
target_link_libraries(test_hashmap_regressions PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_hashmap_regressions)
add_test(NAME hashmap_regressions COMMAND test_hashmap_regressions)

add_executable(test_hashmap_macros test_hashmap_macros.c)
target_link_libraries(test_hashmap_macros PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_hashmap_macros)
add_test(NAME hashmap_typed_macros COMMAND test_hashmap_macros)

add_executable(test_bostree test_bostree.c)
target_link_libraries(test_bostree PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_bostree)
add_test(NAME bostree_smoke COMMAND test_bostree)

add_executable(test_bostree_invariants test_bostree_invariants.c)
target_link_libraries(test_bostree_invariants PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_bostree_invariants)
add_test(NAME bostree_invariants COMMAND test_bostree_invariants)

add_executable(test_bostree_deletion test_bostree_deletion.c)
target_link_libraries(test_bostree_deletion PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_bostree_deletion)
add_test(NAME bostree_deletion COMMAND test_bostree_deletion)

# Exercise the actual interactive example, including cleanup under sanitizers.
add_test(NAME crd_cli_unique COMMAND ${CMAKE_COMMAND}
    "-DCLI=$<TARGET_FILE:crd_cli>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cli_unique.txt"
    -DEXPECT_INSERTED=5
    -DEXPECT_DUPLICATES=0
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_crd_cli.cmake)

add_test(NAME crd_cli_duplicate COMMAND ${CMAKE_COMMAND}
    "-DCLI=$<TARGET_FILE:crd_cli>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cli_duplicate.txt"
    -DEXPECT_INSERTED=4
    -DEXPECT_DUPLICATES=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_crd_cli.cmake)

add_test(NAME crd_cli_invalid_count COMMAND ${CMAKE_COMMAND}
    "-DCLI=$<TARGET_FILE:crd_cli>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cli_invalid_count.txt"
    -DEXPECT_ERROR=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_crd_cli.cmake)

add_test(NAME crd_cli_incomplete_input COMMAND ${CMAKE_COMMAND}
    "-DCLI=$<TARGET_FILE:crd_cli>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cli_incomplete.txt"
    -DEXPECT_INCOMPLETE=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_crd_cli.cmake)

# 3.0.5: deterministic ranking API and independent reference-model tests.
add_executable(test_ranking test_ranking.c)
target_link_libraries(test_ranking PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_ranking)
add_test(NAME ranking_regressions COMMAND test_ranking)

add_executable(test_ranking_differential test_ranking_differential.c)
target_link_libraries(test_ranking_differential PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_ranking_differential)
add_test(NAME ranking_differential COMMAND test_ranking_differential)

# 3.0.6: structural white-box tests explicitly opt into private headers;
# applications and public API tests do not receive that include directory.
target_include_directories(test_bostree_invariants PRIVATE
    ${PROJECT_SOURCE_DIR}/native/src/internal)
target_include_directories(test_bostree_deletion PRIVATE
    ${PROJECT_SOURCE_DIR}/native/src/internal)

add_executable(test_bostree_public_api test_bostree_public_api.c)
target_link_libraries(test_bostree_public_api PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_bostree_public_api)
add_test(NAME bostree_public_api COMMAND test_bostree_public_api)

# The base regressions inspect the table as an explicitly private white-box test.
target_include_directories(test_hashmap_regressions PRIVATE
    ${PROJECT_SOURCE_DIR}/native/src/internal)

add_executable(test_hashmap_public_api test_hashmap_public_api.c)
target_link_libraries(test_hashmap_public_api PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_hashmap_public_api)
add_test(NAME hashmap_public_api COMMAND test_hashmap_public_api)

# The original anonymous typed wrappers still require GNU C extensions.
set_target_properties(test_hashmap test_hashmap_macros test_hashmap_public_api
    PROPERTIES C_EXTENSIONS ON)

# New C11 typed wrappers never cast between incompatible function pointer types.
add_executable(test_hashmap_portable test_hashmap_portable.c)
target_link_libraries(test_hashmap_portable PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_hashmap_portable)
set_target_properties(test_hashmap_portable PROPERTIES C_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_hashmap_portable PRIVATE -pedantic-errors)
endif()
add_test(NAME hashmap_portable_callbacks COMMAND test_hashmap_portable)

# Fault injection is compiled only into a separate test archive. Production
# library objects do not expose the failure-control functions.
add_library(crd_fault_test STATIC
    ${PROJECT_SOURCE_DIR}/native/src/containers/hashmap.c
    ${PROJECT_SOURCE_DIR}/native/src/containers/bostree.c
    ${PROJECT_SOURCE_DIR}/native/src/ranking/ranking.c
    ${PROJECT_SOURCE_DIR}/native/src/domain/team.c
    ${PROJECT_SOURCE_DIR}/native/src/domain/competition.c
    ${PROJECT_SOURCE_DIR}/native/src/domain/standings.c
    ${PROJECT_SOURCE_DIR}/native/src/calendar/calendar.c
    ${PROJECT_SOURCE_DIR}/native/src/calendar/calendar_shuffle.c
    ${PROJECT_SOURCE_DIR}/native/src/calendar/calendar_recombine.c
    ${PROJECT_SOURCE_DIR}/native/src/resolution/match.c
    ${PROJECT_SOURCE_DIR}/native/src/resolution/season_resolution.c
    ${PROJECT_SOURCE_DIR}/native/src/simulation/monte_carlo.c
    ${PROJECT_SOURCE_DIR}/native/src/random/mtwister.c
    ${PROJECT_SOURCE_DIR}/native/src/support/alloc.c)
target_include_directories(crd_fault_test PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(crd_fault_test PRIVATE CRD_TEST_ALLOCATION_FAULTS)
crd_apply_dev_options(crd_fault_test)
add_executable(test_allocation_failures test_allocation_failures.c)
target_include_directories(test_allocation_failures PRIVATE
        ${PROJECT_SOURCE_DIR}/native/include
        ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_allocation_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_allocation_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_allocation_failures)
add_test(NAME allocation_failures COMMAND test_allocation_failures)

add_test(NAME installed_consumer
    COMMAND "${CMAKE_COMMAND}"
        "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DBUILD_ROOT=${PROJECT_BINARY_DIR}"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DC_COMPILER=${CMAKE_C_COMPILER}"
        "-DCONFIG=$<CONFIG>"
        "-DSANITIZERS=${CRD_ENABLE_SANITIZERS}"
        -P "${PROJECT_SOURCE_DIR}/tests/verify_installed_consumer.cmake")

# 3.0.8: exhaustive logical state checks around controlled allocation failures.
add_executable(test_failure_matrix test_failure_matrix.c)
target_include_directories(test_failure_matrix PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_failure_matrix PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_failure_matrix PRIVATE crd_fault_test)
crd_apply_dev_options(test_failure_matrix)
add_test(NAME allocation_failure_matrix COMMAND test_failure_matrix)

# Malformed inputs and boundary-value behavior on the supported public API.
add_executable(test_api_edges test_api_edges.c)
target_link_libraries(test_api_edges PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_api_edges)
add_test(NAME api_adversarial_edges COMMAND test_api_edges)

# Identical input-driven state machines serve both CTest and optional libFuzzer.
foreach (container IN ITEMS hashmap bostree)
    add_executable(test_fuzz_${container} fuzz_${container}.c)
    target_link_libraries(test_fuzz_${container} PRIVATE CentralRankingDogma::central_ranking_dogma)
    target_include_directories(test_fuzz_${container} PRIVATE
        ${PROJECT_SOURCE_DIR}/native/src/internal)
    crd_apply_dev_options(test_fuzz_${container})
    add_test(NAME ${container}_adversarial COMMAND test_fuzz_${container})
endforeach ()

if (CRD_BUILD_FUZZERS)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "CRD_BUILD_FUZZERS requires Clang with libFuzzer")
    endif ()

    # Not every Clang target supports the libFuzzer driver. In particular,
    # MSYS2 UCRT64's x86_64-w64-windows-gnu target rejects -fsanitize=fuzzer.
    # Probe both the instrument-only flag and an actual instrumented link.
    include(CheckCCompilerFlag)
    # The no-link flag needs a compile-only probe: its instrumented object
    # cannot link by itself because libFuzzer supplies coverage callbacks.
    set(_crd_saved_try_compile_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    check_c_compiler_flag("-fsanitize=fuzzer-no-link"
        CRD_LIBFUZZER_NO_LINK_SUPPORTED)
    if (_crd_saved_try_compile_type)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_crd_saved_try_compile_type}")
    else ()
        unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
    endif ()
    unset(_crd_saved_try_compile_type)
    include(CheckCSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=fuzzer,address,undefined")
    check_c_source_compiles("#include <stddef.h>\n#include <stdint.h>\nint LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) { (void)data; (void)size; return 0; }"
        CRD_LIBFUZZER_LINK_SUPPORTED)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if (NOT CRD_LIBFUZZER_NO_LINK_SUPPORTED OR
        NOT CRD_LIBFUZZER_LINK_SUPPORTED)
        message(FATAL_ERROR
            "Clang compiler '${CMAKE_C_COMPILER}' on ${CMAKE_SYSTEM_NAME} "
            "does not support the libFuzzer instrumentation and runtime used by this build. "
            "For deterministic adversarial CTest runs, reconfigure with "
            "-DCRD_BUILD_FUZZERS=OFF; for coverage-guided libFuzzer "
            "campaigns use a compatible Clang target (for example Linux/WSL).")
    endif ()

    # Instrument the data structures themselves, not only the fuzz entry points.
    add_library(crd_fuzz_core STATIC
        ${PROJECT_SOURCE_DIR}/native/src/containers/hashmap.c
        ${PROJECT_SOURCE_DIR}/native/src/containers/bostree.c
        ${PROJECT_SOURCE_DIR}/native/src/support/alloc.c)
    target_include_directories(crd_fuzz_core PRIVATE
        ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
    target_compile_options(crd_fuzz_core PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    crd_apply_dev_options(crd_fuzz_core)
    foreach (container IN ITEMS hashmap bostree)
        add_executable(fuzz_${container}_libfuzzer fuzz_${container}.c)
        target_compile_definitions(fuzz_${container}_libfuzzer PRIVATE CRD_LIBFUZZER)
        target_include_directories(fuzz_${container}_libfuzzer PRIVATE
            ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
        target_link_libraries(fuzz_${container}_libfuzzer PRIVATE crd_fuzz_core)
        target_compile_options(fuzz_${container}_libfuzzer PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
        target_link_options(fuzz_${container}_libfuzzer PRIVATE
            -fsanitize=fuzzer,address,undefined)
        crd_apply_dev_options(fuzz_${container}_libfuzzer)
    endforeach ()
endif ()

# 3.0.9: frozen historical MTwister vectors and published MT19937 vectors.
add_executable(test_mtwister_vectors test_mtwister_vectors.c)
target_link_libraries(test_mtwister_vectors PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_mtwister_vectors)
set_target_properties(test_mtwister_vectors PROPERTIES C_EXTENSIONS OFF)
add_test(NAME mtwister_reference_vectors COMMAND test_mtwister_vectors)

# The batch example is a runnable public-API integration test (no stdin).
add_test(NAME ranking_batch_example COMMAND crd_ranking_batch)

# Standalone ISO C11 translation units for installed public headers.
# In contrast to a combined include test, this also catches missing headers.
foreach (header IN ITEMS
        central_ranking_dogma bostree commons hashmap_base hashmap_typed mtwister team competition standings calendar ranking match season_resolution monte_carlo)
    add_library(header_c11_${header} OBJECT test_public_header_c11.c)
    target_include_directories(header_c11_${header} PRIVATE ${PROJECT_SOURCE_DIR}/native/include)
    target_compile_definitions(header_c11_${header} PRIVATE
        "CRD_PUBLIC_HEADER=\"${header}.h\"")
    set_target_properties(header_c11_${header} PROPERTIES C_EXTENSIONS OFF)
    crd_apply_dev_options(header_c11_${header})
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(header_c11_${header} PRIVATE -pedantic-errors)
    endif()
endforeach()

# 3.0.11 team/UUID contract and the full user-provided fixture regression.
add_executable(test_team test_team.c)
target_link_libraries(test_team PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_team)
set_target_properties(test_team PROPERTIES C_EXTENSIONS OFF)
add_test(NAME team_model_contract COMMAND test_team)

add_executable(test_competition test_competition.c)
target_link_libraries(test_competition PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_competition)
set_target_properties(test_competition PROPERTIES C_EXTENSIONS OFF)
target_compile_definitions(test_competition PRIVATE
    "CRD_CALENDAR_TSV=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calendar_2026_2027.tsv\"")
add_test(NAME competition_snapshot_contract COMMAND test_competition)

add_executable(test_competition_failures test_competition_failures.c)
target_include_directories(test_competition_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_competition_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_competition_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_competition_failures)
set_target_properties(test_competition_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME competition_allocation_failures COMMAND test_competition_failures)

# Team names are human-readable, including embedded spaces.
add_test(NAME crd_cli_team_names COMMAND ${CMAKE_COMMAND}
    "-DCLI=$<TARGET_FILE:crd_cli>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cli_team_names.txt"
    -DEXPECT_INSERTED=5
    -DEXPECT_DUPLICATES=0
    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_crd_cli.cmake)

# 3.0.11: reusable mutable W/D/L and championship-points state.
add_executable(test_standings test_standings.c)
target_link_libraries(test_standings PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_standings)
set_target_properties(test_standings PROPERTIES C_EXTENSIONS OFF)
add_test(NAME standings_contract COMMAND test_standings)

# White-box overflow plus deterministic allocation-failure rollback.
add_executable(test_standings_failures test_standings_failures.c)
target_include_directories(test_standings_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_standings_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_standings_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_standings_failures)
set_target_properties(test_standings_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME standings_failure_contract COMMAND test_standings_failures)

# 3.0.12: fixture permutations and strict rematch cooldown.
add_executable(test_calendar test_calendar.c)
target_link_libraries(test_calendar PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_calendar)
set_target_properties(test_calendar PROPERTIES C_EXTENSIONS OFF)
target_compile_definitions(test_calendar PRIVATE
    "CRD_CALENDAR_TSV=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calendar_2026_2027.tsv\"")
add_test(NAME calendar_permutation_contract COMMAND test_calendar)

# White-box validation and allocation-failure atomicity.
add_executable(test_calendar_failures test_calendar_failures.c)
target_include_directories(test_calendar_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_calendar_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_calendar_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_calendar_failures)
set_target_properties(test_calendar_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME calendar_permutation_failures COMMAND test_calendar_failures)

# 3.0.12: empirical seeded-sampling diagnostics, not a uniformity guarantee.
add_executable(test_calendar_sampling test_calendar_sampling.c)
target_link_libraries(test_calendar_sampling PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_calendar_sampling)
set_target_properties(test_calendar_sampling PROPERTIES C_EXTENSIONS OFF)
add_test(NAME calendar_sampling_diagnostics COMMAND test_calendar_sampling)

# 3.1.0: public pure rules and match-by-match scientific regression.
add_executable(test_match test_match.c)
target_link_libraries(test_match PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_match)
set_target_properties(test_match PROPERTIES C_EXTENSIONS OFF)
add_test(NAME match_contract COMMAND test_match)

add_executable(test_season_resolution test_season_resolution.c)
target_link_libraries(test_season_resolution PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_season_resolution)
set_target_properties(test_season_resolution PROPERTIES C_EXTENSIONS OFF)
target_compile_definitions(test_season_resolution PRIVATE
    "CRD_CALENDAR_TSV=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calendar_2026_2027.tsv\"")
add_test(NAME season_resolution_contract COMMAND test_season_resolution)

# Full fault-injection version of the resolver must use the dedicated archive.
add_executable(test_resolution_failures test_resolution_failures.c)
target_include_directories(test_resolution_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_resolution_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_resolution_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_resolution_failures)
set_target_properties(test_resolution_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME season_resolution_failures COMMAND test_resolution_failures)

# 3.1.2 PR A: scalar API compatibility and custom-key reference model.
add_executable(test_ranking_custom test_ranking_custom.c)
target_link_libraries(test_ranking_custom PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_ranking_custom)
set_target_properties(test_ranking_custom PROPERTIES C_EXTENSIONS OFF)
add_test(NAME ranking_custom_differential COMMAND test_ranking_custom)

add_executable(test_ranking_custom_failures test_ranking_custom_failures.c)
target_include_directories(test_ranking_custom_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_ranking_custom_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_ranking_custom_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_ranking_custom_failures)
set_target_properties(test_ranking_custom_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME ranking_custom_failures COMMAND test_ranking_custom_failures)

add_executable(test_ranking_backends test_ranking_backends.c)
target_link_libraries(test_ranking_backends PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_ranking_backends)
set_target_properties(test_ranking_backends PROPERTIES C_EXTENSIONS OFF)
add_test(NAME ranking_backend_equivalence COMMAND test_ranking_backends)

add_executable(test_ranking_backends_failures test_ranking_backends_failures.c)
target_include_directories(test_ranking_backends_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_ranking_backends_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_ranking_backends_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_ranking_backends_failures)
set_target_properties(test_ranking_backends_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME ranking_backend_failures COMMAND test_ranking_backends_failures)

# 3.1.1: reproducible batches, directed prefix uniqueness, CSV, cancellation.
add_executable(test_monte_carlo test_monte_carlo.c)
target_link_libraries(test_monte_carlo PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(test_monte_carlo)
set_target_properties(test_monte_carlo PROPERTIES C_EXTENSIONS OFF)
add_test(NAME monte_carlo_contract COMMAND test_monte_carlo)

add_executable(test_monte_carlo_failures test_monte_carlo_failures.c)
target_include_directories(test_monte_carlo_failures PRIVATE
    ${PROJECT_SOURCE_DIR}/native/include ${PROJECT_SOURCE_DIR}/native/src/internal)
target_compile_definitions(test_monte_carlo_failures PRIVATE CRD_TEST_ALLOCATION_FAULTS)
target_link_libraries(test_monte_carlo_failures PRIVATE crd_fault_test)
crd_apply_dev_options(test_monte_carlo_failures)
set_target_properties(test_monte_carlo_failures PROPERTIES C_EXTENSIONS OFF)
add_test(NAME monte_carlo_failure_contract COMMAND test_monte_carlo_failures)
