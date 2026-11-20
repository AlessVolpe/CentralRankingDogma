# Check compile AND link for the selected sanitizer toolchain.
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
