execute_process(
    COMMAND "${FLECHA_COMPILER}" build "${FLECHA_SOURCE}/main.fl"
        --root "${FLECHA_SOURCE}" --lib "std=${FLECHA_SOURCE}/library" -o "${FLECHA_OUTPUT}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "module compilation failed: ${output} ${errors}")
endif()
execute_process(COMMAND "${FLECHA_OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 42)
    message(FATAL_ERROR "module program returned ${result}, expected 42")
endif()
# Diagnostics from imported sources must identify their file and prevent emission.
file(MAKE_DIRECTORY "${FLECHA_OUTPUT}_invalid")
file(WRITE "${FLECHA_OUTPUT}_invalid/main.fl" "module main; @imports :: root::helper; void main() {}\n")
file(WRITE "${FLECHA_OUTPUT}_invalid/helper.fl" "module helper; export unsafe void leak(own *i32 p) {}\n")
file(REMOVE "${FLECHA_OUTPUT}_invalid/rejected.o")
execute_process(COMMAND "${FLECHA_COMPILER}" --emit-object "${FLECHA_OUTPUT}_invalid/main.fl"
    --root "${FLECHA_OUTPUT}_invalid" -o "${FLECHA_OUTPUT}_invalid/rejected.o"
    RESULT_VARIABLE result ERROR_VARIABLE errors)
if(result EQUAL 0 OR EXISTS "${FLECHA_OUTPUT}_invalid/rejected.o" OR NOT errors MATCHES "helper.fl:FLECHA-S0048")
    message(FATAL_ERROR "imported ownership violation was not diagnosed correctly: ${errors}")
endif()
