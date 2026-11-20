cmake_minimum_required(VERSION 3.16)
project(crd_external_consumer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(CentralRankingDogma 3.1.3 CONFIG REQUIRED)
add_executable(crd_external_consumer main.c)
set_target_properties(crd_external_consumer PROPERTIES
    OUTPUT_NAME "crd-external-consumer")
target_link_libraries(crd_external_consumer PRIVATE CentralRankingDogma::central_ranking_dogma)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(crd_external_consumer PRIVATE -Wall -Wextra -Werror -pedantic-errors)
endif()

enable_testing()
add_test(NAME crd_external_consumer COMMAND crd_external_consumer)

# The installed-consumer CTest must link the same sanitizer runtime when the
# source library was instrumented; ordinary package consumers do not use this.
option(CRD_EXTERNAL_SANITIZERS "Match an instrumented test library" OFF)
if(CRD_EXTERNAL_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(crd_external_consumer PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(crd_external_consumer PRIVATE -fsanitize=address,undefined)
endif()
