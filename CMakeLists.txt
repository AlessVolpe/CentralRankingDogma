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
add_executable(crd_ranking_batch examples/native/ranking_batch.c)
set_target_properties(crd_ranking_batch PROPERTIES OUTPUT_NAME "crd-ranking-batch")
target_link_libraries(crd_ranking_batch PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_ranking_batch)

if (CRD_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif ()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

include(cmake/modules/Packaging.cmake)
