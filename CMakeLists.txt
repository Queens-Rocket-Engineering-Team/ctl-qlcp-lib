cmake_minimum_required(VERSION 3.15)
project(qlcp LANGUAGES C)

add_library(qlcp src/qlcp_lib.c)
add_library(qlcp::qlcp ALIAS qlcp)

set_target_properties(qlcp
    PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(qlcp
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# only run this when building the project, to allow use as subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

    # set up library installation
    include(GNUInstallDirs)

    install(TARGETS qlcp
        EXPORT qlcpTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/qlcp_lib.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT qlcpTargets
        FILE qlcpTargets.cmake
        NAMESPACE qlcp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qlcp
    )

    # generate cmake package config file
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/qlcpConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/qlcpConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qlcp
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/qlcpConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qlcp
    )

endif()