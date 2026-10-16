foreach(variable IN ITEMS FLECHA_COMPILER LONGBOW SOURCE_ROOT OUTPUT_ROOT)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

if(NOT DEFINED EXECUTABLE_SUFFIX)
    set(EXECUTABLE_SUFFIX "")
endif()

execute_process(
    COMMAND "${FLECHA_COMPILER}" --version
    RESULT_VARIABLE versionResult
    OUTPUT_VARIABLE versionOutput
    ERROR_VARIABLE versionError
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT versionResult EQUAL 0 OR
   NOT versionOutput MATCHES "^flecha [0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "compiler version probe failed: ${versionOutput}${versionError}")
endif()

file(REMOVE_RECURSE "${OUTPUT_ROOT}")
file(MAKE_DIRECTORY "${OUTPUT_ROOT}")
set(nativeOutput "${OUTPUT_ROOT}/native-smoke${EXECUTABLE_SUFFIX}")
execute_process(
    COMMAND "${FLECHA_COMPILER}" build
        "${SOURCE_ROOT}/examples/llvm_poc.fl" -o "${nativeOutput}"
    RESULT_VARIABLE compileResult
    ERROR_VARIABLE compileError
)
if(NOT compileResult EQUAL 0)
    message(FATAL_ERROR "native smoke compilation failed: ${compileError}")
endif()
execute_process(COMMAND "${nativeOutput}" RESULT_VARIABLE nativeResult)
if(NOT nativeResult EQUAL 42)
    message(FATAL_ERROR
        "native smoke returned ${nativeResult} instead of 42")
endif()

execute_process(
    COMMAND "${LONGBOW}" new hello
    WORKING_DIRECTORY "${OUTPUT_ROOT}"
    RESULT_VARIABLE createResult
    OUTPUT_VARIABLE createOutput
    ERROR_VARIABLE createError
)
if(NOT createResult EQUAL 0)
    message(FATAL_ERROR
        "Longbow project creation failed: ${createOutput}${createError}")
endif()
execute_process(
    COMMAND "${LONGBOW}" run
    WORKING_DIRECTORY "${OUTPUT_ROOT}/hello"
    RESULT_VARIABLE runResult
    OUTPUT_VARIABLE runOutput
    ERROR_VARIABLE runError
)
string(REPLACE "\r\n" "\n" runOutput "${runOutput}")
if(NOT runResult EQUAL 0 OR NOT runOutput STREQUAL "Hello from Flecha!\n")
    message(FATAL_ERROR
        "Longbow smoke failed: ${runOutput}${runError}")
endif()
