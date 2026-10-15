file(REMOVE "${FLECHA_OUTPUT}")
execute_process(
    COMMAND "${FLECHA_COMPILER}" --emit-object "${FLECHA_SOURCE}" -o "${FLECHA_OUTPUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
)
if(result EQUAL 0 OR EXISTS "${FLECHA_OUTPUT}")
    message(FATAL_ERROR "invalid ownership reached object emission: ${output} ${errors}")
endif()
foreach(code IN ITEMS FLECHA-S0048 FLECHA-S0049 FLECHA-S0050 FLECHA-S0052)
    if(NOT errors MATCHES "${code}:")
        message(FATAL_ERROR "missing ${code}: ${errors}")
    endif()
endforeach()
if(NOT errors MATCHES "allocation or pointer originates here")
    message(FATAL_ERROR "missing allocation source note: ${errors}")
endif()
