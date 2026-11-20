# Optional native benchmark and research executables (CRD_BUILD_BENCHMARKS=ON).
add_executable(crd_bench benchmark.c)
set_target_properties(crd_bench PROPERTIES OUTPUT_NAME "crd-bench" RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_link_libraries(crd_bench PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_bench)
set_target_properties(crd_bench PROPERTIES C_EXTENSIONS OFF)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(crd_bench PRIVATE -pedantic-errors)
endif ()

# 3.1.2 research tools are optional, C11-only public-API consumers.
foreach (name IN ITEMS ranking_compare season_experiment memory_compare)
    add_executable(crd_${name} ${name}.c)
    string(REPLACE "_" "-" crd_executable_suffix "${name}")
    set_target_properties(crd_${name} PROPERTIES
        OUTPUT_NAME "crd-${crd_executable_suffix}"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
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

if (BUILD_TESTING)
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
             "${PROJECT_BINARY_DIR}/research_smoke_heap_obs.csv"
             "${PROJECT_BINARY_DIR}/research_smoke_heap_summary.csv")
    add_test(NAME season_experiment_avl_smoke
             COMMAND crd_season_experiment 8 5489 3 avl shuffle independent
             "${PROJECT_BINARY_DIR}/research_smoke_avl_obs.csv"
             "${PROJECT_BINARY_DIR}/research_smoke_avl_summary.csv")
endif ()
