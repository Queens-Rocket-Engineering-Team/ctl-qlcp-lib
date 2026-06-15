add_executable(fuzz_qlcp_decode qlcp_lib_fuzz_decode.c)
target_link_libraries(fuzz_qlcp_decode PRIVATE qlcp)

# copy the dictionary file from source to the binary build folder
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/qlcp_fuzz.dict
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/qlcp_fuzz.dict
            ${CMAKE_BINARY_DIR}/qlcp_fuzz.dict
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/qlcp_fuzz.dict
    COMMENT "Copying fuzzer dict to build directory"
)

target_sources(fuzz_qlcp_decode PRIVATE ${CMAKE_BINARY_DIR}/qlcp_fuzz.dict)

add_custom_target(RUN_DECODE_FUZZER
    COMMAND $<TARGET_FILE:fuzz_qlcp_decode> -dict=qlcp_fuzz.dict
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running the decoder fuzzer"
    USES_TERMINAL
)

add_dependencies(RUN_DECODE_FUZZER fuzz_qlcp_decode)