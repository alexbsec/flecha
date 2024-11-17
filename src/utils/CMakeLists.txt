# src/utils/CMakeLists.txt
file(GLOB UTILS_SOURCES *.cpp)
add_library(utils ${UTILS_SOURCES})
target_include_directories(utils PRIVATE ${PROJECT_SOURCE_DIR}/include)

