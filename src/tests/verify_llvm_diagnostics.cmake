foreach(variable IN ITEMS SOURCE_ROOT OUTPUT_ROOT GENERATOR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_ROOT}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${SOURCE_ROOT}"
        -B "${OUTPUT_ROOT}"
        -G "${GENERATOR}"
        -DBUILD_TESTING=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE
    RESULT_VARIABLE missingResult
    OUTPUT_VARIABLE missingOutput
    ERROR_VARIABLE missingError
)
set(missingText "${missingOutput}${missingError}")
if(missingResult EQUAL 0 OR
   NOT missingText MATCHES "LLVM development package was not found" OR
   NOT missingText MATCHES "LLVM_DIR")
    message(FATAL_ERROR
        "missing LLVM diagnostic was not actionable: ${missingText}")
endif()

foreach(unsupportedMajor IN ITEMS 16 24)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DLLVM_VERSION_MAJOR=${unsupportedMajor}
            -DLLVM_PACKAGE_VERSION=${unsupportedMajor}.0.0
            -P "${SOURCE_ROOT}/cmake/check_llvm_version.cmake"
        RESULT_VARIABLE unsupportedResult
        OUTPUT_VARIABLE unsupportedOutput
        ERROR_VARIABLE unsupportedError
    )
    set(unsupportedText "${unsupportedOutput}${unsupportedError}")
    if(unsupportedResult EQUAL 0 OR
       NOT unsupportedText MATCHES "Unsupported LLVM ${unsupportedMajor}" OR
       NOT unsupportedText MATCHES "supports LLVM 17 through 23" OR
       NOT unsupportedText MATCHES "LLVM_DIR")
        message(FATAL_ERROR
            "unsupported LLVM diagnostic was not actionable: ${unsupportedText}")
    endif()
endforeach()
