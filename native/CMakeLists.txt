# Compile modules independently while preserving the original single static
# archive and its CentralRankingDogma::central_ranking_dogma link target.
add_library(crd_containers_obj OBJECT
        src/containers/hashmap.c
        src/containers/bostree.c
        src/support/alloc.c
        src/support/research_profile.c
)
add_library(crd_ranking_obj OBJECT
        src/ranking/ranking.c
)
add_library(crd_legacy_obj OBJECT
        src/random/mtwister.c
        src/support/commons.c
)
add_library(crd_domain_obj OBJECT
        src/domain/team.c
        src/domain/competition.c
        src/domain/standings.c
        src/calendar/calendar.c
        src/calendar/calendar_shuffle.c
        src/calendar/calendar_recombine.c
        src/resolution/match.c
        src/resolution/season_resolution.c
        src/simulation/monte_carlo.c
)

foreach (component IN ITEMS containers ranking legacy domain)
    target_include_directories(crd_${component}_obj PRIVATE
            ${PROJECT_SOURCE_DIR}/native/include)
    crd_apply_dev_options(crd_${component}_obj)
    set_target_properties(crd_${component}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach ()
# Only BOSTree and structural invariant tests can see the representation.
target_include_directories(crd_containers_obj PRIVATE
        ${PROJECT_SOURCE_DIR}/native/src/internal)
target_include_directories(crd_ranking_obj PRIVATE
        ${PROJECT_SOURCE_DIR}/native/src/internal)
target_include_directories(crd_domain_obj PRIVATE
        ${PROJECT_SOURCE_DIR}/native/src/internal)
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
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

target_include_directories(central_ranking_dogma PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/native/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
crd_apply_dev_options(central_ranking_dogma)

add_executable(crd_cli
        cli/educational/crd_cli.c
)
set_target_properties(crd_cli PROPERTIES
        OUTPUT_NAME "crd-cli" RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_link_libraries(crd_cli PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_cli)

# 3.1.3 private JSON subprocess adapter. JSON parsing is NOT linked into
# CentralRankingDogma::central_ranking_dogma or its exported target.
add_executable(crd_json
        cli/json/main.c
        cli/json/simulation_cli.c
        cli/json/json_value.c)
set_target_properties(crd_json PROPERTIES
        OUTPUT_NAME "crd-json" C_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(crd_json PRIVATE
        "${PROJECT_SOURCE_DIR}/native/include"
        "${PROJECT_SOURCE_DIR}/native/cli/json")
target_link_libraries(crd_json PRIVATE CentralRankingDogma::central_ranking_dogma)
crd_apply_dev_options(crd_json)

# Replay identifiers incorporate native algorithm sources and the actual
# compiler/toolchain. An old replay recipe cannot silently move to a new build.
set(json_engine_fingerprints "${PROJECT_VERSION}|${CMAKE_C_COMPILER_ID}|
    ${CMAKE_C_COMPILER_VERSION}|${CMAKE_SYSTEM_NAME}|${CMAKE_SYSTEM_PROCESSOR}|
    ${CMAKE_C_FLAGS}|${CMAKE_C_FLAGS_DEBUG}|${CMAKE_C_FLAGS_RELEASE}|
    ${CMAKE_C_FLAGS_RELWITHDEBINFO}|${CMAKE_BUILD_TYPE}")
file(GLOB_RECURSE json_engine_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/native/src/*.c"
        "${PROJECT_SOURCE_DIR}/native/src/*.h"
        "${PROJECT_SOURCE_DIR}/native/cli/json/*.c"
        "${PROJECT_SOURCE_DIR}/native/cli/json/*.h"
        "${PROJECT_SOURCE_DIR}/native/include/*.h")
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
