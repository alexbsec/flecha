# src/std/CMakeLists.txt
file(GLOB STD_SOURCES *.cpp)
add_library(std ${STD_SOURCES})
target_include_directories(std PRIVATE ${PROJECT_SOURCE_DIR}/include)

