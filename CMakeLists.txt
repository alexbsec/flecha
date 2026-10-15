cmake_minimum_required(VERSION 3.20)

project(Flecha LANGUAGES C CXX)

include(CTest)

option(FLECHA_ENABLE_SANITIZERS "Build Flecha with AddressSanitizer" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep language and compiler requirements in one target so every Flecha target
# is built consistently without modifying global compiler flags.
add_library(flecha_project_options INTERFACE)
target_compile_features(flecha_project_options INTERFACE cxx_std_23)

if(FLECHA_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(flecha_project_options INTERFACE
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        target_link_options(flecha_project_options INTERFACE
            -fsanitize=address
        )
    else()
        message(WARNING
            "FLECHA_ENABLE_SANITIZERS is only configured for Clang and GCC"
        )
    endif()
endif()

add_subdirectory(src/diagnostics)
add_subdirectory(src/core)
add_subdirectory(src/semantic)
add_subdirectory(src/lowering)
add_subdirectory(src/runtime)
add_subdirectory(src/codegen)
add_subdirectory(src/memory)
add_subdirectory(src/utils)
add_subdirectory(src/std)

add_executable(flecha src/main.cpp)
target_link_libraries(flecha
    PRIVATE
        flecha_project_options
        diagnostics
        core
        semantic
        lowering
        llvm_codegen
        memory
        runtime
        utils
        flecha_std
)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(src/tests)
endif()
