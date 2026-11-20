cmake_minimum_required(VERSION 3.16)
project(leaderboardsystem VERSION 3.1.2 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(LEADERBOARD_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(LEADERBOARD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LEADERBOARD_BUILD_FUZZERS "Build Clang/libFuzzer harnesses (requires BUILD_TESTING)" OFF)
option(LEADERBOARD_BUILD_BENCHMARKS "Build deterministic performance benchmarks" OFF)
option(LEADERBOARD_RESEARCH_PROFILE "Enable opt-in internal research timing and wrapper-allocation statistics" OFF)
if (LEADERBOARD_RESEARCH_PROFILE AND NOT LEADERBOARD_BUILD_BENCHMARKS)
    message(FATAL_ERROR "LEADERBOARD_RESEARCH_PROFILE requires LEADERBOARD_BUILD_BENCHMARKS=ON")
endif ()

# Check compile AND link, because some GCC distributions accept sanitizer
# flags but do not ship the corresponding runtime libraries (notably UCRT64).
if (LEADERBOARD_ENABLE_SANITIZERS)
    if (NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "LEADERBOARD_ENABLE_SANITIZERS requires GCC or Clang")
    endif ()
    include(CheckCSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_c_source_compiles("int main(void) { return 0; }"
        LEADERBOARD_ASAN_UBSAN_LINK_SUPPORTED)
    cmake_pop_check_state()
    if (NOT LEADERBOARD_ASAN_UBSAN_LINK_SUPPORTED)
        message(FATAL_ERROR
            "The selected toolchain cannot compile and link ASan/UBSan. "
            "Use -DLEADERBOARD_ENABLE_SANITIZERS=OFF or a supported "
            "Linux/WSL GCC or Clang toolchain.")
    endif ()
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Native modules and the new named typed hashmap API use ISO C11.
# Deprecated HASHMAP(...) macro clients still require GNU C extensions.
set(CMAKE_C_EXTENSIONS OFF)

function(leaderboard_apply_dev_options target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if (LEADERBOARD_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif ()
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
        if (LEADERBOARD_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif ()

        if (LEADERBOARD_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif ()
    endif ()
endfunction()

# Compile modules independently while preserving the original single static
# archive and its Leaderboard::leaderboard link target for existing consumers.
add_library(leaderboard_containers_obj OBJECT
        src/hashmap.c
        src/bostree.c
        src/alloc.c
        src/research_profile.c
)
add_library(leaderboard_ranking_obj OBJECT
        src/ranking.c
)
add_library(leaderboard_legacy_obj OBJECT
        src/mtwister.c
        src/commons.c
)
add_library(leaderboard_domain_obj OBJECT
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
    target_include_directories(leaderboard_${component}_obj PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
    leaderboard_apply_dev_options(leaderboard_${component}_obj)
    set_target_properties(leaderboard_${component}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach ()
# Only BOSTree and structural invariant tests can see the representation.
target_include_directories(leaderboard_containers_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
target_include_directories(leaderboard_ranking_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
target_include_directories(leaderboard_domain_obj PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
if (LEADERBOARD_RESEARCH_PROFILE)
    target_compile_definitions(leaderboard_containers_obj PRIVATE LEADERBOARD_RESEARCH_PROFILE)
    target_compile_definitions(leaderboard_domain_obj PRIVATE LEADERBOARD_RESEARCH_PROFILE)
endif ()

add_library(leaderboard STATIC
        $<TARGET_OBJECTS:leaderboard_containers_obj>
        $<TARGET_OBJECTS:leaderboard_ranking_obj>
        $<TARGET_OBJECTS:leaderboard_legacy_obj>
        $<TARGET_OBJECTS:leaderboard_domain_obj>
)
add_library(Leaderboard::leaderboard ALIAS leaderboard)

set_target_properties(leaderboard PROPERTIES
        OUTPUT_NAME leaderboard
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(leaderboard PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
leaderboard_apply_dev_options(leaderboard)

add_executable(leaderboard_cli
        examples/leaderboard_cli.c
)
target_link_libraries(leaderboard_cli PRIVATE Leaderboard::leaderboard)
leaderboard_apply_dev_options(leaderboard_cli)

# 3.1.3 private JSON subprocess adapter. JSON parsing is NOT linked into
# Leaderboard::leaderboard or its exported target.
add_executable(leaderboard_json
        src/json_cli/main.c
        src/json_cli/json_value.c)
set_target_properties(leaderboard_json PROPERTIES
        OUTPUT_NAME "leaderboard-json" C_EXTENSIONS OFF)
target_include_directories(leaderboard_json PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src/json_cli")
target_link_libraries(leaderboard_json PRIVATE Leaderboard::leaderboard)
leaderboard_apply_dev_options(leaderboard_json)

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
target_compile_definitions(leaderboard_json PRIVATE
        "LEADERBOARD_ENGINE_VERSION=\"${PROJECT_VERSION}\""
        "LEADERBOARD_ENGINE_ID=\"${PROJECT_VERSION}-${json_engine_short}\"")

# Deterministic, non-interactive public ranking API example.
add_executable(leaderboard_ranking_batch examples/ranking_batch.c)
target_link_libraries(leaderboard_ranking_batch PRIVATE Leaderboard::leaderboard)
leaderboard_apply_dev_options(leaderboard_ranking_batch)

if (LEADERBOARD_BUILD_BENCHMARKS)
    add_executable(leaderboard_bench benchmarks/benchmark.c)
    target_link_libraries(leaderboard_bench PRIVATE Leaderboard::leaderboard)
    leaderboard_apply_dev_options(leaderboard_bench)
    set_target_properties(leaderboard_bench PROPERTIES C_EXTENSIONS OFF)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(leaderboard_bench PRIVATE -pedantic-errors)
    endif ()

    # 3.1.2 research tools are optional, C11-only public-API consumers.
    foreach (name IN ITEMS ranking_compare season_experiment memory_compare)
        add_executable(leaderboard_${name} benchmarks/${name}.c)
        target_link_libraries(leaderboard_${name} PRIVATE Leaderboard::leaderboard)
        leaderboard_apply_dev_options(leaderboard_${name})
        set_target_properties(leaderboard_${name} PROPERTIES C_EXTENSIONS OFF)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(leaderboard_${name} PRIVATE -pedantic-errors)
        endif ()
    endforeach ()
    if (LEADERBOARD_RESEARCH_PROFILE)
        target_compile_definitions(leaderboard_season_experiment PRIVATE LEADERBOARD_RESEARCH_PROFILE)
        target_include_directories(leaderboard_season_experiment PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
    endif ()
endif ()

if (BUILD_TESTING)
    add_subdirectory(tests)
    if (LEADERBOARD_RESEARCH_PROFILE)
        add_executable(test_research_profile tests/test_research_profile.c)
        target_include_directories(test_research_profile PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/internal)
        target_compile_definitions(test_research_profile PRIVATE LEADERBOARD_RESEARCH_PROFILE)
        target_link_libraries(test_research_profile PRIVATE Leaderboard::leaderboard)
        leaderboard_apply_dev_options(test_research_profile)
        add_test(NAME research_profile_contract COMMAND test_research_profile)
    endif ()
    if (LEADERBOARD_BUILD_BENCHMARKS)
        # Correctness-only smoke check. Never gate CI on wall/CPU times.
        add_test(NAME benchmark_smoke COMMAND leaderboard_bench 1000 1)
        add_test(NAME ranking_compare_smoke
                 COMMAND leaderboard_ranking_compare 8 1 5489 3)
        add_test(NAME memory_compare_heap_smoke
                 COMMAND leaderboard_memory_compare 8 heap 5489)
        add_test(NAME memory_compare_avl_smoke
                 COMMAND leaderboard_memory_compare 8 avl 5489)
        add_test(NAME season_experiment_heap_smoke
                 COMMAND leaderboard_season_experiment 8 5489 3 heap shuffle independent
                 "${CMAKE_CURRENT_BINARY_DIR}/research_smoke_heap_obs.csv"
                 "${CMAKE_CURRENT_BINARY_DIR}/research_smoke_heap_summary.csv")
        add_test(NAME season_experiment_avl_smoke
                 COMMAND leaderboard_season_experiment 8 5489 3 avl shuffle independent
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
                 "LEADERBOARD_JSON_BINARY=$<TARGET_FILE:leaderboard_json>"
                 "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_json_cli_native.py")
    endif ()
endif ()

# Build-tree consumers keep using Leaderboard::leaderboard; installed consumers
# get the same target via find_package(Leaderboard CONFIG REQUIRED).
install(TARGETS leaderboard EXPORT LeaderboardTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.h")
set(package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Leaderboard")
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/LeaderboardConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/LeaderboardConfig.cmake"
    INSTALL_DESTINATION "${package_dir}")
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/LeaderboardConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES "${PROJECT_BINARY_DIR}/LeaderboardConfig.cmake"
              "${PROJECT_BINARY_DIR}/LeaderboardConfigVersion.cmake"
        DESTINATION "${package_dir}")
install(EXPORT LeaderboardTargets NAMESPACE Leaderboard::
        DESTINATION "${package_dir}")
