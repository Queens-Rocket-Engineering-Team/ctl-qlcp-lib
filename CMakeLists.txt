cmake_minimum_required(VERSION 3.15)
project(qlcp VERSION 3.0.0 LANGUAGES C)

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
    # turn tests off by default (must enable with flag -DBUILD_TESTING=ON)
    set(BUILD_TESTING OFF CACHE BOOL "Enable testing for QLCP")

    # testing setup
    include(CTest)
    if(BUILD_TESTING)
        include(FetchContent)
        # download unity testing framework if and only if running tests
        FetchContent_Declare(
            Unity
            GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
            GIT_TAG        v2.6.1
        )
        FetchContent_MakeAvailable(Unity)

        add_subdirectory(tests/unit)
    endif()

    # fuzzing setup
    option(ENABLE_FUZZING "Enable libFuzzer" OFF)

    if(ENABLE_FUZZING)
        add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -g -O1)
        add_link_options(-fsanitize=fuzzer,address,undefined)

        target_compile_options(qlcp PRIVATE -fsanitize=fuzzer-no-link,address,undefined -g -O1)
        target_link_options(qlcp PRIVATE -fsanitize=fuzzer-no-link,address,undefined)

        add_subdirectory(tests/fuzz)
    endif()

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

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/qlcpConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/qlcpConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/qlcpConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qlcp
    )

endif()