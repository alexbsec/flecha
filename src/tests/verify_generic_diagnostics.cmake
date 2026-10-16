file(MAKE_DIRECTORY "${FLECHA_OUTPUT}")
foreach(source
    "T twice<T>(T x) { return x + x; } structure Box { i32 n; }; void main() { twice<Box>(Box::init()); }"
    "void grow<T>() { grow<*T>(); } void main() { grow<i32>(); }")
    file(WRITE "${FLECHA_OUTPUT}/invalid.fl" "${source}\n")
    file(REMOVE "${FLECHA_OUTPUT}/invalid.o")
    execute_process(COMMAND "${FLECHA_COMPILER}" --emit-object "${FLECHA_OUTPUT}/invalid.fl"
        -o "${FLECHA_OUTPUT}/invalid.o" RESULT_VARIABLE result ERROR_VARIABLE errors TIMEOUT 30)
    if(result EQUAL 0 OR EXISTS "${FLECHA_OUTPUT}/invalid.o" OR NOT errors MATCHES "FLECHA-S0054")
        message(FATAL_ERROR "generic error did not prevent emission: ${errors}")
    endif()
endforeach()
