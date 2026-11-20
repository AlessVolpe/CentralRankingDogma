# Build-tree consumers keep using CentralRankingDogma::central_ranking_dogma; installed consumers
# get the same target via find_package(CentralRankingDogma CONFIG REQUIRED).
# The JSON transport remains a separate executable, not an exported C library.
install(TARGETS crd_json
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS central_ranking_dogma EXPORT CentralRankingDogmaTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/native/include/"
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
