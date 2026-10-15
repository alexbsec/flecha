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
gtest_discover_tests(test_flecha DISCOVERY_MODE PRE_TEST)

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

add_test(
    NAME NativeRuntimeTests.OwningStrings
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/owning_string.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/owning_string_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_owning_string.cmake
)

add_test(
    NAME NativeRuntimeTests.OwningStringScalarFormatting
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/owning_string_scalars.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/owning_string_scalars_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_owning_string_scalars.cmake
)

add_test(
    NAME NativeSemanticTests.DefiniteAssignment
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/definite_assignment.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/definite_assignment_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_definite_assignment.cmake
)

add_test(
    NAME NativeSemanticTests.ControlFlowDefiniteAssignment
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/control_flow_definite_assignment.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/control_flow_assignment_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_control_flow_assignment.cmake
)

# This fixture shares the control-flow fixture's output/exit contract and
# instrumented-runtime linking, so it can use the same native verifier.
add_test(
    NAME NativeSemanticTests.ConstDeclarations
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/const_declarations.fl
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/const_declarations_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_control_flow_assignment.cmake
)

add_test(
    NAME NativeSemanticTests.UnsafePointers
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/unsafe_pointer_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/unsafe_pointer_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/unsafe_pointers_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)

add_test(
    NAME NativeSemanticTests.CheckedReferences
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/checked_reference_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/checked_reference_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/checked_references_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)

add_test(
    NAME NativeSemanticTests.FixedArrays
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fixed_array_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fixed_array_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed_arrays_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_arrays.cmake
)

add_test(
    NAME NativeSemanticTests.Iteration
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/iteration_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/iteration_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/iteration_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)

add_test(
    NAME NativeSemanticTests.CheckedSlices
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/checked_slice_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/checked_slice_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/checked_slices_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        "-DFLECHA_TRAP_MODES=r;w;b;o;e;n;f;k;u;m"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_arrays.cmake
)

add_test(
    NAME NativeSemanticTests.Defer
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/defer_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/defer_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/defer_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        "-DFLECHA_TRAP_MODES=t;a;c"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_arrays.cmake
)

add_test(
    NAME NativeSemanticTests.Tuples
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tuple_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tuple_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tuples_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)

add_test(
    NAME NativeSemanticTests.ErrorValues
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/error_values_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/error_values_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/error_values_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)

add_test(
    NAME NativeSemanticTests.Structures
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/structure_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/structure_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/structures_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)

add_test(
    NAME NativeSemanticTests.RawAllocation
    COMMAND ${CMAKE_COMMAND}
        -DFLECHA_COMPILER=$<TARGET_FILE:flecha>
        -DFLECHA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/allocation_api.fl
        -DFLECHA_HARNESS=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/allocation_harness.cpp
        -DFLECHA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/allocation_test
        -DFLECHA_RUNTIME=$<TARGET_FILE:runtime>
        -DFLECHA_LINKER=${CMAKE_CXX_COMPILER}
        -DFLECHA_SANITIZERS=${FLECHA_ENABLE_SANITIZERS}
        -DFLECHA_LINK_OPTIONS=-Wl,--wrap=flecha_runtime_v1_alloc
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_unsafe_pointers.cmake
)
