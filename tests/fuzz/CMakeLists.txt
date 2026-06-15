add_executable(fuzz_qlcp_decode qlcp_lib_fuzz_decode.c)
target_link_libraries(fuzz_qlcp_decode PRIVATE qlcp)

# run the decode fuzzer with required flags
add_custom_target(RUN_DECODE_FUZZER
    COMMAND ${CMAKE_COMMAND}
            -E make_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus
    COMMAND $<TARGET_FILE:fuzz_qlcp_decode>
            -dict="${CMAKE_CURRENT_SOURCE_DIR}"/qlcp_lib_fuzz.dict
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus_seed/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fuzz_qlcp_decode
            ${CMAKE_BINARY_DIR}/qlcp_lib_fuzz.dict
            ${CMAKE_BINARY_DIR}/decode_corpus_seed
    COMMENT "Running the decoder fuzzer"
    USES_TERMINAL
)