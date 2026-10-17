foreach(variable IN ITEMS FLECHA_COMPILER LONGBOW OUTPUT_ROOT)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

if(NOT DEFINED EXECUTABLE_SUFFIX)
    set(EXECUTABLE_SUFFIX "")
endif()

if(DEFINED STANDARD_LIBRARY)
    get_filename_component(STANDARD_LIBRARY "${STANDARD_LIBRARY}" ABSOLUTE)
    if(NOT IS_DIRECTORY "${STANDARD_LIBRARY}")
        message(FATAL_ERROR
            "STANDARD_LIBRARY is not a directory: ${STANDARD_LIBRARY}")
    endif()
    set(ENV{FLECHA_STANDARD_LIBRARY} "${STANDARD_LIBRARY}")
endif()

execute_process(
    COMMAND "${FLECHA_COMPILER}" --version
    RESULT_VARIABLE versionResult
    OUTPUT_VARIABLE versionOutput
    ERROR_VARIABLE versionError
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT versionResult EQUAL 0 OR NOT versionOutput MATCHES
   "^flecha [0-9]+\\.[0-9]+\\.[0-9]+(-([0-9]+|dev)-g[0-9a-f]+(-dirty)?)?$")
    message(FATAL_ERROR
        "compiler version probe failed: ${versionOutput}${versionError}")
endif()
string(REGEX REPLACE "^flecha " "" expectedVersion "${versionOutput}")

execute_process(
    COMMAND "${LONGBOW}" --version
    RESULT_VARIABLE longbowVersionResult
    OUTPUT_VARIABLE longbowVersionOutput
    ERROR_VARIABLE longbowVersionError
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT longbowVersionResult EQUAL 0 OR
   NOT longbowVersionOutput STREQUAL "longbow ${expectedVersion}")
    message(FATAL_ERROR
        "Longbow version probe failed: ${longbowVersionOutput}${longbowVersionError}")
endif()

file(REMOVE_RECURSE "${OUTPUT_ROOT}")
file(MAKE_DIRECTORY "${OUTPUT_ROOT}")
set(smokeSource "${OUTPUT_ROOT}/native-smoke.fl")
file(WRITE "${smokeSource}"
    "i32 add(i32 left, i32 right) { return left + right; }\n"
    "i32 main() { return add(20, 22); }\n")

execute_process(
    COMMAND "${FLECHA_COMPILER}" check "${smokeSource}"
    RESULT_VARIABLE checkResult
    ERROR_VARIABLE checkError
)
if(NOT checkResult EQUAL 0)
    message(FATAL_ERROR "compiler check failed: ${checkError}")
endif()

set(objectOutput "${OUTPUT_ROOT}/native-smoke.o")
execute_process(
    COMMAND "${FLECHA_COMPILER}" --emit-object "${smokeSource}"
        -o "${objectOutput}"
    RESULT_VARIABLE objectResult
    ERROR_VARIABLE objectError
)
if(NOT objectResult EQUAL 0 OR NOT EXISTS "${objectOutput}")
    message(FATAL_ERROR "object emission failed: ${objectError}")
endif()

set(nativeOutput "${OUTPUT_ROOT}/native-smoke${EXECUTABLE_SUFFIX}")
execute_process(
    COMMAND "${FLECHA_COMPILER}" build
        "${smokeSource}" -o "${nativeOutput}"
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
file(WRITE "${OUTPUT_ROOT}/hello/src/main.fl"
    "module main;\n"
    "@imports :: lib::std::containers::array;\n"
    "void main() { stdoutln(\"Hello from Flecha!\"); }\n")
execute_process(
    COMMAND "${LONGBOW}" check
    WORKING_DIRECTORY "${OUTPUT_ROOT}/hello"
    RESULT_VARIABLE longbowCheckResult
    OUTPUT_VARIABLE longbowCheckOutput
    ERROR_VARIABLE longbowCheckError
)
if(NOT longbowCheckResult EQUAL 0)
    message(FATAL_ERROR
        "Longbow check failed: ${longbowCheckOutput}${longbowCheckError}")
endif()
execute_process(
    COMMAND "${LONGBOW}" build
    WORKING_DIRECTORY "${OUTPUT_ROOT}/hello"
    RESULT_VARIABLE longbowBuildResult
    OUTPUT_VARIABLE longbowBuildOutput
    ERROR_VARIABLE longbowBuildError
)
if(NOT longbowBuildResult EQUAL 0)
    message(FATAL_ERROR
        "Longbow build failed: ${longbowBuildOutput}${longbowBuildError}")
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
