# core/CMakeLists.txt
add_library(core STATIC
    Tokenizer.cpp
    core.cpp
)

target_include_directories(core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(core PRIVATE ${GTEST_LIBRARIES})

