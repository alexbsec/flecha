foreach(variable IN ITEMS FLECHA_COMPILER RELEASE_TAG SOURCE_ROOT)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

if(NOT RELEASE_TAG MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
    message(FATAL_ERROR "release tag must use the form v0.1.0")
endif()
set(expectedVersion "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${FLECHA_COMPILER}" --version
    RESULT_VARIABLE compilerResult
    OUTPUT_VARIABLE compilerOutput
    ERROR_VARIABLE compilerError
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT compilerResult EQUAL 0)
    message(FATAL_ERROR "compiler version probe failed: ${compilerError}")
endif()
if(NOT compilerOutput STREQUAL "flecha ${expectedVersion}")
    message(FATAL_ERROR
        "release tag ${RELEASE_TAG} does not match ${compilerOutput}")
endif()

file(READ "${SOURCE_ROOT}/editors/vscode/package.json" vscodeManifest)
string(JSON vscodeVersion GET "${vscodeManifest}" version)
if(NOT vscodeVersion STREQUAL "${expectedVersion}")
    message(FATAL_ERROR
        "VS Code extension version ${vscodeVersion} does not match ${expectedVersion}")
endif()
