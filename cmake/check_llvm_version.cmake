set(FLECHA_LLVM_MIN_MAJOR 17)
set(FLECHA_LLVM_MAX_MAJOR 23)

if(NOT DEFINED LLVM_VERSION_MAJOR)
    message(FATAL_ERROR
        "LLVMConfig.cmake did not provide LLVM_VERSION_MAJOR; use a complete "
        "LLVM development installation."
    )
endif()

if(LLVM_VERSION_MAJOR LESS FLECHA_LLVM_MIN_MAJOR OR
   LLVM_VERSION_MAJOR GREATER FLECHA_LLVM_MAX_MAJOR)
    if(DEFINED LLVM_PACKAGE_VERSION)
        set(_flecha_llvm_detected "${LLVM_PACKAGE_VERSION}")
    else()
        set(_flecha_llvm_detected "${LLVM_VERSION_MAJOR}")
    endif()
    message(FATAL_ERROR
        "Unsupported LLVM ${_flecha_llvm_detected}. Flecha supports LLVM "
        "${FLECHA_LLVM_MIN_MAJOR} through ${FLECHA_LLVM_MAX_MAJOR}. Set "
        "LLVM_DIR to a supported installation's lib/cmake/llvm directory."
    )
endif()
