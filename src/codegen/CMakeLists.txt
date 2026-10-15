find_package(LLVM REQUIRED CONFIG)

add_library(llvm_codegen STATIC
    llvm_code_generator.cpp
    native_compiler.cpp
)

target_include_directories(llvm_codegen
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
)

target_include_directories(llvm_codegen SYSTEM PUBLIC
        ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(llvm_codegen
    PUBLIC
        lowering
    PRIVATE
        flecha_project_options
        LLVM
)
