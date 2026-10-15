execute_process(
    COMMAND "${FLECHA_COMPILER}" build "${FLECHA_SOURCE}" -o "${FLECHA_OUTPUT}"
    RESULT_VARIABLE compileResult
    ERROR_VARIABLE compileError
)
if(NOT compileResult EQUAL 0)
    message(FATAL_ERROR "native compilation failed: ${compileError}")
endif()

execute_process(
    COMMAND "${FLECHA_OUTPUT}"
    RESULT_VARIABLE runResult
    OUTPUT_VARIABLE actualOutput
    ERROR_VARIABLE runError
)
if(NOT runResult EQUAL 42)
    message(FATAL_ERROR
        "native program returned ${runResult}, expected 42: ${runError}"
    )
endif()
if(NOT actualOutput STREQUAL "initialized\n")
    message(FATAL_ERROR "native output mismatch: ${actualOutput}")
endif()
