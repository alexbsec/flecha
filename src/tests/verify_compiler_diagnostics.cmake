if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

function(expect_compiler_result compilerId compilerVersion shouldPass pattern)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DCMAKE_C_COMPILER_ID=${compilerId}
            -DCMAKE_C_COMPILER_VERSION=${compilerVersion}
            -DCMAKE_CXX_COMPILER_ID=${compilerId}
            -DCMAKE_CXX_COMPILER_VERSION=${compilerVersion}
            -P "${SOURCE_ROOT}/cmake/check_compiler_version.cmake"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(text "${output}${error}")
    if(shouldPass)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR
                "${compilerId} ${compilerVersion} should be supported: ${text}"
            )
        endif()
    elseif(result EQUAL 0 OR NOT text MATCHES "${pattern}")
        message(FATAL_ERROR
            "${compilerId} ${compilerVersion} did not produce '${pattern}': "
            "${text}"
        )
    endif()
endfunction()

expect_compiler_result(Clang 17.0.0 TRUE "")
expect_compiler_result(Clang 23.1.0 TRUE "")
expect_compiler_result(
    Clang 16.0.0 FALSE "Unsupported Clang C compiler 16"
)
expect_compiler_result(
    Clang 24.0.0 FALSE "Unsupported Clang C compiler 24"
)

expect_compiler_result(GNU 13.1.0 TRUE "")
expect_compiler_result(GNU 16.2.0 TRUE "")
expect_compiler_result(
    GNU 12.4.0 FALSE "Unsupported GCC C compiler 12"
)
expect_compiler_result(
    GNU 17.0.0 FALSE "Unsupported GCC C compiler 17"
)

expect_compiler_result(AppleClang 16.0.0 TRUE "")
expect_compiler_result(AppleClang 17.0.0 TRUE "")
expect_compiler_result(
    AppleClang 15.0.0 FALSE "Unsupported Apple Clang C compiler 15"
)
expect_compiler_result(
    AppleClang 18.0.0 FALSE "Unsupported Apple Clang C compiler 18"
)

expect_compiler_result(MSVC 19.40 FALSE "Unsupported C/C[+][+] compiler")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DCMAKE_C_COMPILER_ID=GNU
        -DCMAKE_C_COMPILER_VERSION=14.2
        -DCMAKE_CXX_COMPILER_ID=Clang
        -DCMAKE_CXX_COMPILER_VERSION=18.1
        -P "${SOURCE_ROOT}/cmake/check_compiler_version.cmake"
    RESULT_VARIABLE mixedResult
    OUTPUT_VARIABLE mixedOutput
    ERROR_VARIABLE mixedError
)
set(mixedText "${mixedOutput}${mixedError}")
if(mixedResult EQUAL 0 OR
   NOT mixedText MATCHES "must come from the same supported toolchain")
    message(FATAL_ERROR
        "mixed compiler diagnostic was not actionable: ${mixedText}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DCMAKE_C_COMPILER_ID=Clang
        -DCMAKE_C_COMPILER_VERSION=17.0
        -DCMAKE_CXX_COMPILER_ID=Clang
        -DCMAKE_CXX_COMPILER_VERSION=18.1
        -P "${SOURCE_ROOT}/cmake/check_compiler_version.cmake"
    RESULT_VARIABLE versionMismatchResult
    OUTPUT_VARIABLE versionMismatchOutput
    ERROR_VARIABLE versionMismatchError
)
set(versionMismatchText "${versionMismatchOutput}${versionMismatchError}")
if(versionMismatchResult EQUAL 0 OR
   NOT versionMismatchText MATCHES "must use the same major version")
    message(FATAL_ERROR
        "mismatched compiler versions were not rejected: "
        "${versionMismatchText}"
    )
endif()
