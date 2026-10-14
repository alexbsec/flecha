add_library(core STATIC
    tokenizer.cpp
    core.cpp
)

target_include_directories(core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
)

target_link_libraries(core
    PRIVATE
        flecha_project_options
)
