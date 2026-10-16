cmake_minimum_required(VERSION 3.20)

project(Flecha VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)

configure_file(
    src/include/flecha/version.hpp.in
    ${PROJECT_BINARY_DIR}/generated/flecha/version.hpp
    @ONLY
)

option(FLECHA_ENABLE_SANITIZERS
    "Build Flecha with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep language and compiler requirements in one target so every Flecha target
# is built consistently without modifying global compiler flags.
add_library(flecha_project_options INTERFACE)
target_compile_features(flecha_project_options INTERFACE cxx_std_23)

if(FLECHA_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(flecha_project_options INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(flecha_project_options INTERFACE
            -fsanitize=address,undefined
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
target_include_directories(flecha PRIVATE ${PROJECT_BINARY_DIR}/generated)
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

add_executable(longbow src/longbow/main.cpp)
target_include_directories(longbow PRIVATE ${PROJECT_BINARY_DIR}/generated)
target_link_libraries(longbow PRIVATE flecha_project_options llvm_codegen LLVM)
target_compile_definitions(longbow PRIVATE
    FLECHA_EXECUTABLE_SUFFIX="${CMAKE_EXECUTABLE_SUFFIX}"
    FLECHA_OBJECT_SUFFIX="${CMAKE_CXX_OUTPUT_EXTENSION}"
)
add_dependencies(longbow flecha)

install(TARGETS flecha longbow
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/flecha
)
install(DIRECTORY std/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flecha/std
    FILES_MATCHING PATTERN "*.fl"
)
install(DIRECTORY editors/neovim/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flecha/editors/neovim
    PATTERN "tests" EXCLUDE
)
install(DIRECTORY editors/vscode/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/flecha/editors/vscode
)
install(FILES README.md SPEC.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/reference
    FILES_MATCHING PATTERN "*.md"
)

if(BUILD_TESTING)
    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "Do not install Flecha's test dependency" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(src/tests)
endif()

set(CPACK_PACKAGE_NAME "Flecha")
set(CPACK_PACKAGE_VENDOR "Flecha")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME
    "flecha-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)
