add_executable(test_qlcp qlcp_lib_tests.c)

target_link_libraries(test_qlcp PRIVATE
    qlcp::qlcp
    unity
)

add_test(NAME qlcp_unit_tests COMMAND test_qlcp)

add_custom_target(run_unit_tests
    COMMAND $<TARGET_FILE:test_qlcp>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS test_qlcp
    COMMENT "Running unit tests"
    USES_TERMINAL
)
