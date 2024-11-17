# src/runtime/CMakeLists.txt
file(GLOB RUNTIME_SOURCES *.cpp)
add_library(runtime ${RUNTIME_SOURCES})
target_include_directories(runtime PRIVATE ${PROJECT_SOURCE_DIR}/include)

