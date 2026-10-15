add_library(diagnostics STATIC
    diagnostic.cpp
    diagnostic_bag.cpp
)

target_include_directories(diagnostics
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
)

target_link_libraries(diagnostics PRIVATE flecha_project_options)
