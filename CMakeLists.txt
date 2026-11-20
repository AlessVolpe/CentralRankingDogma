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

# Check compile AND link, because some GCC distributions accept sanitizer
# flags but do not ship the corresponding runtime libraries (notably UCRT64).
if (CRD_ENABLE_SANITIZERS)
    if (NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "CRD_ENABLE_SANITIZERS requires GCC or Clang")
    endif ()
    include(CheckCSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_c_source_compiles("int main(void) { return 0; }"
        CRD_ASAN_UBSAN_LINK_SUPPORTED)
    cmake_pop_check_state()
    if (NOT CRD_ASAN_UBSAN_LINK_SUPPORTED)
        message(FATAL_ERROR
            "The selected toolchain cannot compile and link ASan/UBSan. "
            "Use -DCRD_ENABLE_SANITIZERS=OFF or a supported "
            "Linux/WSL GCC or Clang toolchain.")
    endif ()
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Native modules and the new named typed hashmap API use ISO C11.
# Deprecated HASHMAP(...) macro clients still require GNU C extensions.
set(CMAKE_C_EXTENSIONS OFF)

function(crd_apply_dev_options target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if (CRD_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif ()
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
        if (CRD_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif ()

        if (CRD_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif ()
    endif ()
endfunction()

# Compile modules independently while preserving the original single static
# archive and its CentralRankingDogma::central_ranking_dogma link target.
add_library(crd_containers_obj OBJECT
        src/hashmap.c
        src/bostree.c
        src/alloc.c
        src/research_profile.c
)
add_library(crd_ranking_obj OBJECT
        src/ranking.c
)
add_library(crd_legacy_obj OBJECT
        src/mtwister.c
        src/commons.c
)
add_library(crd_domain_obj OBJECT
        src/team.c
        src/competition.c
        src/standings.c
        src/calendar.c
        src/calendar_shuffle.c
        src/calendar_recombine.c
        src/match.c
        src/season_resolution.c
        src/monte_carlo.c
)

foreach (component IN ITEMS containers ranking legacy domain)
    target_include_directories(crd_${component}_obj PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
    crd_apply_dev_options(crd_${component}_obj)
    set_target_properties(crd_${component}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach ()
# Only BOSTree and structural invariant tests can see the representation.
target_include_directories(crd_containers_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
target_include_directories(crd_ranking_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
target_include_directories(crd_domain_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
if (CRD_RESEARCH_PROFILE)
    target_compile_definitions(crd_containers_obj PRIVATE CRD_RESEARCH_PROFILE)
    target_compile_definitions(crd_domain_obj PRIVATE CRD_RESEARCH_PROFILE)
endif ()

add_library(central_ranking_dogma STATIC
        $<TARGET_OBJECTS:crd_containers_obj>
        $<TARGET_OBJECTS:crd_ranking_obj>
        $<TARGET_OBJECTS:crd_legacy_obj>
        $<TARGET_OBJECTS:crd_domain_obj>
)
add_library(CentralRankingDogma::central_ranking_dogma ALIAS central_ranking_dogma)

set_target_properties(central_ranking_dogma PROPERTIES
        OUTPUT_NAME central_ranking_dogma
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(central_ranking_dogma PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
crd_apply_dev_options(central_ranking_dogma)

add_executable(crd_cli
        examples/crd_cli.c
)
set_target_properties(crd_cli PROPERTIES OUTPUT_NAME "crd-cli")
target_link_libraries(crd_cli PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_cli)

# 3.1.3 private JSON subprocess adapter. JSON parsing is NOT linked into
# CentralRankingDogma::central_ranking_dogma or its exported target.
add_executable(crd_json
        src/json_cli/main.c
        src/json_cli/simulation_cli.c
        src/json_cli/json_value.c)
set_target_properties(crd_json PROPERTIES
        OUTPUT_NAME "crd-json" C_EXTENSIONS OFF)
target_include_directories(crd_json PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src/json_cli")
target_link_libraries(crd_json PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_json)

# Replay identifiers incorporate native algorithm sources and the actual
# compiler/toolchain. An old replay recipe cannot silently move to a new build.
set(json_engine_fingerprints "${PROJECT_VERSION}|${CMAKE_C_COMPILER_ID}|
    ${CMAKE_C_COMPILER_VERSION}|${CMAKE_SYSTEM_NAME}|${CMAKE_SYSTEM_PROCESSOR}|
    ${CMAKE_C_FLAGS}|${CMAKE_C_FLAGS_DEBUG}|${CMAKE_C_FLAGS_RELEASE}|
    ${CMAKE_C_FLAGS_RELWITHDEBINFO}|${CMAKE_BUILD_TYPE}")
file(GLOB_RECURSE json_engine_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.c"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/include/*.h")
list(SORT json_engine_sources)
foreach (path IN LISTS json_engine_sources)
    file(SHA256 "${path}" file_fingerprint)
    string(APPEND json_engine_fingerprints "|${file_fingerprint}")
endforeach ()
string(SHA256 json_engine_hash "${json_engine_fingerprints}")
string(SUBSTRING "${json_engine_hash}" 0 16 json_engine_short)
target_compile_definitions(crd_json PRIVATE
        "CRD_ENGINE_VERSION=\"${PROJECT_VERSION}\""
        "CRD_ENGINE_ID=\"${PROJECT_VERSION}-${json_engine_short}\"")

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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
    endif ()
endif ()

if (BUILD_TESTING)
    add_subdirectory(tests)
    if (CRD_RESEARCH_PROFILE)
        add_executable(test_research_profile tests/test_research_profile.c)
        target_include_directories(test_research_profile PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
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

# Build-tree consumers keep using CentralRankingDogma::central_ranking_dogma; installed consumers
# get the same target via find_package(CentralRankingDogma CONFIG REQUIRED).
# The JSON transport remains a separate executable, not an exported C library.
install(TARGETS crd_json
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS central_ranking_dogma EXPORT CentralRankingDogmaTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.h")
set(package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/CentralRankingDogma")
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/CentralRankingDogmaConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/CentralRankingDogmaConfig.cmake"
    INSTALL_DESTINATION "${package_dir}")
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/CentralRankingDogmaConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES "${PROJECT_BINARY_DIR}/CentralRankingDogmaConfig.cmake"
              "${PROJECT_BINARY_DIR}/CentralRankingDogmaConfigVersion.cmake"
        DESTINATION "${package_dir}")
install(EXPORT CentralRankingDogmaTargets NAMESPACE CentralRankingDogma::
        DESTINATION "${package_dir}")
