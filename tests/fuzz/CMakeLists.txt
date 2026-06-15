add_executable(fuzz_qlcp_decode qlcp_lib_fuzz_decode.c)
target_link_libraries(fuzz_qlcp_decode PRIVATE qlcp)

add_custom_target(RUN_DECODE_FUZZER
    COMMAND $<TARGET_FILE:fuzz_qlcp_decode>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running the decoder fuzzer"
    USES_TERMINAL
)

add_dependencies(RUN_DECODE_FUZZER fuzz_qlcp_decode)