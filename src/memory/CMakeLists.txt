# src/memory/CMakeLists.txt
file(GLOB MEMORY_SOURCES *.cpp)
add_library(memory ${MEMORY_SOURCES})
target_include_directories(memory PRIVATE ${PROJECT_SOURCE_DIR}/include)

