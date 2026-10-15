add_executable(test_flecha
    test_ast.cpp
    test_diagnostics.cpp
    test_lowering.cpp
    test_main.cpp
    test_parser.cpp
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
        memory
        runtime
        utils
        flecha_std
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_flecha)
