set(FLECHA_CLANG_MIN_MAJOR 17)
set(FLECHA_CLANG_MAX_MAJOR 23)
set(FLECHA_GNU_MIN_MAJOR 13)
set(FLECHA_GNU_MAX_MAJOR 16)
set(FLECHA_APPLECLANG_MIN_MAJOR 16)
set(FLECHA_APPLECLANG_MAX_MAJOR 17)
set(FLECHA_LLVM_MIN_MAJOR 17)
set(FLECHA_LLVM_MAX_MAJOR 23)

# Release-producing CI uses one reproducible baseline from each platform's
# supported range. Patch releases within these major versions are accepted.
set(FLECHA_CI_LINUX_GNU_MAJOR 14)
set(FLECHA_CI_LINUX_LLVM_MAJOR 18)
set(FLECHA_CI_WINDOWS_LLVM_MAJOR 23)
set(FLECHA_CI_WINDOWS_LINKER_MAJOR 23)
set(FLECHA_CI_MACOS_LLVM_MAJOR 18)
set(FLECHA_CI_MACOS_XCODE_VERSION 16.4)
set(FLECHA_CI_MACOS_APPLECLANG_MAJOR 17)
set(FLECHA_CI_MACOS_SDK_MAJOR 15)

function(flecha_require_major_range label version minimum maximum)
    string(REGEX MATCH "^([0-9]+)" _flecha_version_match "${version}")
    if(NOT _flecha_version_match)
        message(FATAL_ERROR
            "Could not determine the ${label} major version from '${version}'."
        )
    endif()
    set(_flecha_major "${CMAKE_MATCH_1}")
    if(_flecha_major LESS minimum OR _flecha_major GREATER maximum)
        message(FATAL_ERROR
            "Unsupported ${label} ${version}. Flecha supports ${label} "
            "${minimum} through ${maximum}."
        )
    endif()
endfunction()
