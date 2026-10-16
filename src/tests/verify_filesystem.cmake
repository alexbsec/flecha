set(dataDirectory "${FLECHA_OUTPUT}_data")
file(MAKE_DIRECTORY "${dataDirectory}")
set(textPath "${dataDirectory}/text.txt")
set(emptyPath "${dataDirectory}/empty.bin")
set(missingPath "${dataDirectory}/missing.txt")
file(WRITE "${textPath}" "flecha\n")
file(WRITE "${emptyPath}" "")

set(sourceDirectory "${FLECHA_OUTPUT}_project")
file(MAKE_DIRECTORY "${sourceDirectory}")
set(source "${sourceDirectory}/main.fl")
file(WRITE "${source}"
"module main;

@imports
    :: lib::std::io::filesystem
    :: lib::std::io::errors;

i32 main() {
    text, text_error := filesystem::read_file(\"${textPath}\");
    if text_error != none or text != \"flecha\\n\" {
        return 1;
    }

    empty, empty_error := filesystem::read_file(\"${emptyPath}\");
    if empty_error != none or empty.len() != 0 {
        return 2;
    }

    missing, missing_error := filesystem::read_file(\"${missingPath}\");
    if missing_error != errors::IoError.NotFound or missing.len() != 0 {
        return 3;
    }

    string invalid_path = \"bad\" + '\\0'.string() + \"path\";
    invalid, invalid_error := filesystem::read_file(invalid_path.view());
    if invalid_error != errors::IoError.InvalidPath or invalid.len() != 0 {
        return 4;
    }

    return 42;
}
")
execute_process(
    COMMAND "${FLECHA_COMPILER}" build "${source}" -o "${FLECHA_OUTPUT}"
        --lib "std=${FLECHA_STD}"
    RESULT_VARIABLE compileResult ERROR_VARIABLE compileError TIMEOUT 30
)
if(NOT compileResult EQUAL 0)
    message(FATAL_ERROR "filesystem compilation failed: ${compileError}")
endif()
execute_process(COMMAND "${FLECHA_OUTPUT}" RESULT_VARIABLE runResult TIMEOUT 10)
if(NOT runResult EQUAL 42)
    message(FATAL_ERROR "filesystem program returned ${runResult}, expected 42")
endif()
