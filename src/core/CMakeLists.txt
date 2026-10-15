add_library(core STATIC
    parser.cpp
    tokenizer.cpp
    core.cpp
)

target_include_directories(core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
)

target_link_libraries(core
    PUBLIC
        diagnostics
    PRIVATE
        flecha_project_options
)
