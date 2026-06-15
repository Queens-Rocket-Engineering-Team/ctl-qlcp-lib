add_executable(fuzz_qlcp_decode qlcp_lib_fuzz_decode.c)
target_link_libraries(fuzz_qlcp_decode PRIVATE qlcp)

# copy the dictionary file and corpus dir from source to the binary build folder
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/qlcp_fuzz.dict
           ${CMAKE_BINARY_DIR}/corpus
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/qlcp_fuzz.dict
            ${CMAKE_BINARY_DIR}/qlcp_fuzz.dict
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/corpus
            ${CMAKE_BINARY_DIR}/corpus
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/qlcp_fuzz.dict
            ${CMAKE_CURRENT_SOURCE_DIR}/corpus
    COMMENT "Copying fuzzer dictionary and corpus to build directory"
)

# run the decode fuzzer with required flags
add_custom_target(RUN_DECODE_FUZZER
    COMMAND $<TARGET_FILE:fuzz_qlcp_decode> -dict=qlcp_fuzz.dict corpus/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fuzz_qlcp_decode
            ${CMAKE_BINARY_DIR}/qlcp_fuzz.dict
            ${CMAKE_BINARY_DIR}/corpus
    COMMENT "Running the decoder fuzzer"
    USES_TERMINAL
)