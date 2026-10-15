add_executable(test_flecha
    test_ast.cpp
    test_codegen.cpp
    test_diagnostics.cpp
    test_lowering.cpp
    test_main.cpp
    test_parser.cpp
    test_runtime.cpp
    test_semantic.cpp
    test_tokenizer.cpp
)

target_link_libraries(test_flecha
    PRIVATE
        flecha_project_options
        diagnostics
        core
        semantic
        lowering
        llvm_codegen
        memory
        runtime
        utils
        flecha_std
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_flecha)

add_test(
    NAME NativeRuntimeTests.PrintsEveryPrintableType
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/stdout.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/runtime_stdout_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_stdout.cmake
)

add_test(
    NAME NativeRuntimeTests.StringViewOperations
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/string_view_operations.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/string_view_operations_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_string_view_operations.cmake
)
