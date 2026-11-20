cmake_minimum_required(VERSION 3.16)
project(CentralRankingDogma VERSION 3.1.3 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CRD_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CRD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CRD_BUILD_FUZZERS "Build Clang/libFuzzer harnesses (requires BUILD_TESTING)" OFF)
option(CRD_BUILD_BENCHMARKS "Build deterministic performance benchmarks" OFF)
option(CRD_RESEARCH_PROFILE "Enable opt-in internal research timing and wrapper-allocation statistics" OFF)
if (CRD_RESEARCH_PROFILE AND NOT CRD_BUILD_BENCHMARKS)
    message(FATAL_ERROR "CRD_RESEARCH_PROFILE requires CRD_BUILD_BENCHMARKS=ON")
endif ()

include(cmake/modules/Sanitizers.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Native modules and the new named typed hashmap API use ISO C11.
# Deprecated HASHMAP(...) macro clients still require GNU C extensions.
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/modules/CompilerOptions.cmake)

add_subdirectory(native)

# Deterministic, non-interactive public ranking API example.
add_executable(crd_ranking_batch examples/ranking_batch.c)
set_target_properties(crd_ranking_batch PROPERTIES OUTPUT_NAME "crd-ranking-batch")
target_link_libraries(crd_ranking_batch PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_ranking_batch)

if (CRD_BUILD_BENCHMARKS)
    add_executable(crd_bench benchmarks/benchmark.c)
    set_target_properties(crd_bench PROPERTIES OUTPUT_NAME "crd-bench")
    target_link_libraries(crd_bench PRIVATE CentralRankingDogma::central_ranking_dogma)
    crd_apply_dev_options(crd_bench)
    set_target_properties(crd_bench PROPERTIES C_EXTENSIONS OFF)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(crd_bench PRIVATE -pedantic-errors)
    endif ()

    # 3.1.2 research tools are optional, C11-only public-API consumers.
    foreach (name IN ITEMS ranking_compare season_experiment memory_compare)
        add_executable(crd_${name} benchmarks/${name}.c)
        string(REPLACE "_" "-" crd_executable_suffix "${name}")
        set_target_properties(crd_${name} PROPERTIES
            OUTPUT_NAME "crd-${crd_executable_suffix}")
        target_link_libraries(crd_${name} PRIVATE CentralRankingDogma::central_ranking_dogma)
        crd_apply_dev_options(crd_${name})
        set_target_properties(crd_${name} PROPERTIES C_EXTENSIONS OFF)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(crd_${name} PRIVATE -pedantic-errors)
        endif ()
    endforeach ()
    if (CRD_RESEARCH_PROFILE)
        target_compile_definitions(crd_season_experiment PRIVATE CRD_RESEARCH_PROFILE)
        target_include_directories(crd_season_experiment PRIVATE
            ${PROJECT_SOURCE_DIR}/native/src/internal)
    endif ()
endif ()

if (BUILD_TESTING)
    add_subdirectory(tests)
    if (CRD_RESEARCH_PROFILE)
        add_executable(test_research_profile tests/test_research_profile.c)
        target_include_directories(test_research_profile PRIVATE
            ${PROJECT_SOURCE_DIR}/native/src/internal)
        target_compile_definitions(test_research_profile PRIVATE CRD_RESEARCH_PROFILE)
        target_link_libraries(test_research_profile PRIVATE CentralRankingDogma::central_ranking_dogma)
        crd_apply_dev_options(test_research_profile)
        add_test(NAME research_profile_contract COMMAND test_research_profile)
    endif ()
    if (CRD_BUILD_BENCHMARKS)
        # Correctness-only smoke check. Never gate CI on wall/CPU times.
        add_test(NAME benchmark_smoke COMMAND crd_bench 1000 1)
        add_test(NAME ranking_compare_smoke
                 COMMAND crd_ranking_compare 8 1 5489 3)
        add_test(NAME memory_compare_heap_smoke
                 COMMAND crd_memory_compare 8 heap 5489)
        add_test(NAME memory_compare_avl_smoke
                 COMMAND crd_memory_compare 8 avl 5489)
        add_test(NAME season_experiment_heap_smoke
                 COMMAND crd_season_experiment 8 5489 3 heap shuffle independent
                 "${CMAKE_CURRENT_BINARY_DIR}/research_smoke_heap_obs.csv"
                 "${CMAKE_CURRENT_BINARY_DIR}/research_smoke_heap_summary.csv")
        add_test(NAME season_experiment_avl_smoke
                 COMMAND crd_season_experiment 8 5489 3 avl shuffle independent
                 "${CMAKE_CURRENT_BINARY_DIR}/research_smoke_avl_obs.csv"
                 "${CMAKE_CURRENT_BINARY_DIR}/research_smoke_avl_summary.csv")
    endif ()
endif ()

# Exercise the linked native executable (not the mock API) when Python is
# available. The old contract-schema tests remain separate documentation CI.
if (BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if (Python3_Interpreter_FOUND)
        add_test(NAME json_cli_native_contract
                 COMMAND "${CMAKE_COMMAND}" -E env
                 "CRD_JSON_BINARY=$<TARGET_FILE:crd_json>"
                 "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_json_cli_native.py")
    endif ()
endif ()

include(cmake/modules/Packaging.cmake)
