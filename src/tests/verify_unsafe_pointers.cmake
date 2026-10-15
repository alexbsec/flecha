execute_process(
    COMMAND "${FLECHA_COMPILER}" --emit-object "${FLECHA_SOURCE}" -o "${FLECHA_OUTPUT}.o"
    RESULT_VARIABLE compileResult ERROR_VARIABLE compileError TIMEOUT 30
)
if(NOT compileResult EQUAL 0)
    message(FATAL_ERROR "native compilation failed: ${compileError}")
endif()
set(sanitizerFlags)
if(FLECHA_SANITIZERS)
    list(APPEND sanitizerFlags -fsanitize=address,undefined)
endif()
execute_process(
    COMMAND "${FLECHA_LINKER}" "${FLECHA_HARNESS}" "${FLECHA_OUTPUT}.o"
        "${FLECHA_RUNTIME}" ${sanitizerFlags} ${FLECHA_LINK_OPTIONS} -o "${FLECHA_OUTPUT}"
    RESULT_VARIABLE linkResult ERROR_VARIABLE linkError TIMEOUT 30
)
if(NOT linkResult EQUAL 0)
    message(FATAL_ERROR "native link failed: ${linkError}")
endif()
execute_process(
    COMMAND "${FLECHA_OUTPUT}"
    RESULT_VARIABLE runResult OUTPUT_VARIABLE actualOutput ERROR_VARIABLE runError TIMEOUT 10
)
if(NOT runResult EQUAL 42)
    message(FATAL_ERROR "native program returned ${runResult}, expected 42: ${runError}")
endif()
if(NOT actualOutput STREQUAL "")
    message(FATAL_ERROR "unexpected native output: ${actualOutput}")
endif()
