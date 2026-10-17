include(${CMAKE_CURRENT_LIST_DIR}/toolchain_policy.cmake)

foreach(language IN ITEMS C CXX)
    if(NOT DEFINED CMAKE_${language}_COMPILER_ID OR
       NOT DEFINED CMAKE_${language}_COMPILER_VERSION)
        message(FATAL_ERROR
            "CMake did not identify the ${language} compiler and version. "
            "Flecha requires Clang ${FLECHA_CLANG_MIN_MAJOR}-"
            "${FLECHA_CLANG_MAX_MAJOR}, GCC ${FLECHA_GNU_MIN_MAJOR}-"
            "${FLECHA_GNU_MAX_MAJOR}, or Apple Clang "
            "${FLECHA_APPLECLANG_MIN_MAJOR}-"
            "${FLECHA_APPLECLANG_MAX_MAJOR}."
        )
    endif()
endforeach()

if(NOT CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID)
    message(FATAL_ERROR
        "The C and C++ compilers must come from the same supported toolchain; "
        "got ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} and "
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}."
    )
endif()

string(REGEX MATCH "^([0-9]+)" _flecha_c_version
       "${CMAKE_C_COMPILER_VERSION}")
set(_flecha_c_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "^([0-9]+)" _flecha_cxx_version
       "${CMAKE_CXX_COMPILER_VERSION}")
set(_flecha_cxx_major "${CMAKE_MATCH_1}")
if(NOT _flecha_c_major STREQUAL _flecha_cxx_major)
    message(FATAL_ERROR
        "The C and C++ compilers must use the same major version; got "
        "${CMAKE_C_COMPILER_VERSION} and ${CMAKE_CXX_COMPILER_VERSION}."
    )
endif()

function(check_compiler_language language label minimum maximum)
    flecha_require_major_range(
        "${label} ${language} compiler"
        "${CMAKE_${language}_COMPILER_VERSION}"
        ${minimum}
        ${maximum}
    )
endfunction()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    foreach(language IN ITEMS C CXX)
        check_compiler_language(
            ${language}
            "Clang"
            ${FLECHA_CLANG_MIN_MAJOR}
            ${FLECHA_CLANG_MAX_MAJOR}
        )
    endforeach()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(language IN ITEMS C CXX)
        check_compiler_language(
            ${language}
            "GCC"
            ${FLECHA_GNU_MIN_MAJOR}
            ${FLECHA_GNU_MAX_MAJOR}
        )
    endforeach()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    foreach(language IN ITEMS C CXX)
        check_compiler_language(
            ${language}
            "Apple Clang"
            ${FLECHA_APPLECLANG_MIN_MAJOR}
            ${FLECHA_APPLECLANG_MAX_MAJOR}
        )
    endforeach()
else()
    message(FATAL_ERROR
        "Unsupported C/C++ compiler '${CMAKE_CXX_COMPILER_ID}' "
        "${CMAKE_CXX_COMPILER_VERSION}. Flecha supports Clang "
        "${FLECHA_CLANG_MIN_MAJOR}-${FLECHA_CLANG_MAX_MAJOR}, GCC "
        "${FLECHA_GNU_MIN_MAJOR}-${FLECHA_GNU_MAX_MAJOR}, and Apple Clang "
        "${FLECHA_APPLECLANG_MIN_MAJOR}-${FLECHA_APPLECLANG_MAX_MAJOR}."
    )
endif()

message(STATUS
    "Using ${CMAKE_C_COMPILER_ID} C ${CMAKE_C_COMPILER_VERSION} at "
    "${CMAKE_C_COMPILER}"
)
message(STATUS
    "Using ${CMAKE_CXX_COMPILER_ID} C++ ${CMAKE_CXX_COMPILER_VERSION} at "
    "${CMAKE_CXX_COMPILER}"
)
