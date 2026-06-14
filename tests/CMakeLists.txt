add_executable(test_qlcp qlcp_lib_tests.c)

target_link_libraries(test_qlcp PRIVATE
    qlcp::qlcp
    unity
)

add_test(NAME qlcp_unit_tests COMMAND test_qlcp)