add_executable(fuzz_qlcp_decode fuzz_qlcp_lib_decode.c)
target_link_libraries(fuzz_qlcp_decode PRIVATE qlcp::qlcp)

# run the decode fuzzer with required flags
add_custom_target(run_decode_fuzzer
    COMMAND ${CMAKE_COMMAND}
            -E make_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus
    COMMAND $<TARGET_FILE:fuzz_qlcp_decode>
            -dict="${CMAKE_CURRENT_SOURCE_DIR}"/fuzz_qlcp_lib_decode.dict
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus_seed/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fuzz_qlcp_decode
            ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_qlcp_lib_decode.dict
            ${CMAKE_CURRENT_SOURCE_DIR}/decode_corpus_seed
    COMMENT "Running the decoder fuzzer"
    USES_TERMINAL
)

add_executable(fuzz_qlcp_roundtrip fuzz_qlcp_lib_roundtrip.c)
target_link_libraries(fuzz_qlcp_roundtrip PRIVATE qlcp::qlcp)

# run the roundtrip fuzzer with required flags
add_custom_target(run_roundtrip_fuzzer
    COMMAND ${CMAKE_COMMAND}
            -E make_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/roundtrip_corpus
    COMMAND $<TARGET_FILE:fuzz_qlcp_roundtrip>
            ${CMAKE_CURRENT_SOURCE_DIR}/roundtrip_corpus
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fuzz_qlcp_roundtrip
    COMMENT "Running the roundtrip fuzzer"
    USES_TERMINAL
)