# CMakeLists.txt

cmake_minimum_required(VERSION 3.14)
project(Flecha LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Generates compile_commands.json for clangd

# Enable testing
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

# Set Google Test targets explicitly
set(GTEST_LIBRARIES gtest gtest_main)

# Include directories for header files (mirroring the structure)
include_directories(${PROJECT_SOURCE_DIR}/src/include)

# Core source files
add_subdirectory(src/core)
add_subdirectory(src/memory)
add_subdirectory(src/runtime)
add_subdirectory(src/utils)
add_subdirectory(src/std)

# Main executable
add_executable(flecha src/main.cpp)

# Link each module to the main executable
target_link_libraries(flecha PRIVATE core memory runtime utils std)

# Testing (GoogleTest)
add_subdirectory(src/tests)

# Print message about how to build
message(STATUS "Build with 'make' or 'cmake --build .'")

