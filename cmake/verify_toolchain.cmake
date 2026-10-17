foreach(variable IN ITEMS POLICY_ROOT CXX_COMPILER EXPECTED_CXX_FAMILY
                          EXPECTED_CXX_MAJOR PLATFORM)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

include(${POLICY_ROOT}/toolchain_policy.cmake)

function(run_tool output_variable label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "Could not query ${label} with '${ARGN}': ${output}${error}"
        )
    endif()
    set(combined "${output}")
    if(NOT error STREQUAL "")
        string(APPEND combined "\n${error}")
    endif()
    message(STATUS "${label}:\n${combined}")
    set(${output_variable} "${combined}" PARENT_SCOPE)
endfunction()

run_tool(compilerText "C++ compiler" "${CXX_COMPILER}" --version)
run_tool(compilerVersion "C++ compiler numeric version"
         "${CXX_COMPILER}" -dumpversion)
run_tool(compilerTarget "C++ compiler target"
         "${CXX_COMPILER}" -dumpmachine)

if(EXPECTED_CXX_FAMILY STREQUAL "Clang")
    if(NOT compilerText MATCHES "clang version")
        message(FATAL_ERROR "Expected Clang, got: ${compilerText}")
    endif()
    set(compilerMinimum ${FLECHA_CLANG_MIN_MAJOR})
    set(compilerMaximum ${FLECHA_CLANG_MAX_MAJOR})
elseif(EXPECTED_CXX_FAMILY STREQUAL "GNU")
    if(NOT compilerText MATCHES "(g\\+\\+|GCC|Free Software Foundation)")
        message(FATAL_ERROR "Expected GCC, got: ${compilerText}")
    endif()
    set(compilerMinimum ${FLECHA_GNU_MIN_MAJOR})
    set(compilerMaximum ${FLECHA_GNU_MAX_MAJOR})
elseif(EXPECTED_CXX_FAMILY STREQUAL "AppleClang")
    if(NOT compilerText MATCHES "Apple clang version")
        message(FATAL_ERROR "Expected Apple Clang, got: ${compilerText}")
    endif()
    set(compilerMinimum ${FLECHA_APPLECLANG_MIN_MAJOR})
    set(compilerMaximum ${FLECHA_APPLECLANG_MAX_MAJOR})
else()
    message(FATAL_ERROR
        "Unknown EXPECTED_CXX_FAMILY '${EXPECTED_CXX_FAMILY}'"
    )
endif()

flecha_require_major_range(
    "${EXPECTED_CXX_FAMILY}"
    "${compilerVersion}"
    ${compilerMinimum}
    ${compilerMaximum}
)
string(REGEX MATCH "^([0-9]+)" compilerVersionMatch "${compilerVersion}")
if(NOT CMAKE_MATCH_1 EQUAL EXPECTED_CXX_MAJOR)
    message(FATAL_ERROR
        "Expected ${EXPECTED_CXX_FAMILY} ${EXPECTED_CXX_MAJOR}, got "
        "${compilerVersion}."
    )
endif()

run_tool(linkerPath "linker selected by the C++ driver"
         "${CXX_COMPILER}" -print-prog-name=ld)
if(PLATFORM STREQUAL "macOS")
    run_tool(linkerText "native linker" xcrun ld -v)
else()
    run_tool(linkerText "native linker" "${linkerPath}" --version)
endif()

if(DEFINED LLVM_CONFIG AND NOT "${LLVM_CONFIG}" STREQUAL "")
    if(NOT DEFINED EXPECTED_LLVM_MAJOR OR
       "${EXPECTED_LLVM_MAJOR}" STREQUAL "")
        message(FATAL_ERROR
            "EXPECTED_LLVM_MAJOR is required when LLVM_CONFIG is provided"
        )
    endif()
    run_tool(llvmVersion "LLVM" "${LLVM_CONFIG}" --version)
    flecha_require_major_range(
        "LLVM"
        "${llvmVersion}"
        ${FLECHA_LLVM_MIN_MAJOR}
        ${FLECHA_LLVM_MAX_MAJOR}
    )
    string(REGEX MATCH "^([0-9]+)" llvmVersionMatch "${llvmVersion}")
    if(NOT CMAKE_MATCH_1 EQUAL EXPECTED_LLVM_MAJOR)
        message(FATAL_ERROR
            "Expected LLVM ${EXPECTED_LLVM_MAJOR}, got ${llvmVersion}."
        )
    endif()
endif()

if(PLATFORM STREQUAL "macOS")
    foreach(variable IN ITEMS EXPECTED_XCODE_VERSION EXPECTED_SDK_MAJOR)
        if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
            message(FATAL_ERROR "${variable} is required on macOS")
        endif()
    endforeach()
    run_tool(xcodeVersion "Xcode" xcodebuild -version)
    string(REPLACE "." "\\." expectedXcodeRegex
           "${EXPECTED_XCODE_VERSION}")
    if(NOT xcodeVersion MATCHES
       "^Xcode ${expectedXcodeRegex}([^0-9]|$)")
        message(FATAL_ERROR
            "Expected Xcode ${EXPECTED_XCODE_VERSION}, got: ${xcodeVersion}"
        )
    endif()
    run_tool(sdkVersion "macOS SDK version" xcrun --sdk macosx
             --show-sdk-version)
    if(NOT sdkVersion MATCHES "^${EXPECTED_SDK_MAJOR}\\.")
        message(FATAL_ERROR
            "Expected macOS SDK ${EXPECTED_SDK_MAJOR}.x, got ${sdkVersion}."
        )
    endif()
    run_tool(sdkPath "macOS SDK path" xcrun --sdk macosx --show-sdk-path)
elseif(PLATFORM STREQUAL "Linux")
    run_tool(libcVersion "Linux C runtime" ldd --version)
elseif(PLATFORM STREQUAL "Windows")
    run_tool(ucrtVersion "MSYS2 UCRT runtime"
             msys2 -c "pacman -Q mingw-w64-ucrt-x86_64-crt")
else()
    message(FATAL_ERROR "Unsupported PLATFORM '${PLATFORM}'")
endif()
