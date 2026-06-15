add_executable(test_qlcp qlcp_lib_tests.c)

target_link_libraries(test_qlcp PRIVATE
    qlcp::qlcp
    unity
)

add_test(NAME qlcp_unit_tests COMMAND test_qlcp)

add_custom_target(RUN_UNIT_TESTS
    COMMAND $<TARGET_FILE:test_qlcp>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running unit tests"
    USES_TERMINAL
)

add_dependencies(RUN_UNIT_TESTS test_qlcp)