if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include(${SOURCE_ROOT}/cmake/toolchain_policy.cmake)

file(READ "${SOURCE_ROOT}/.github/workflows/ci.yml" ciWorkflow)
file(READ "${SOURCE_ROOT}/.github/workflows/release.yml" releaseWorkflow)
file(READ "${SOURCE_ROOT}/docs/toolchains.md" toolchainDocumentation)
file(READ "${SOURCE_ROOT}/README.md" readme)

function(require_text textVariable expected label)
    string(FIND "${${textVariable}}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${label} is missing '${expected}'")
    endif()
endfunction()

foreach(workflow IN ITEMS ciWorkflow releaseWorkflow)
    require_text(
        ${workflow}
        "llvm_major: ${FLECHA_CI_LINUX_LLVM_MAJOR}"
        "workflow Linux LLVM baseline"
    )
    require_text(
        ${workflow}
        "llvm_major: ${FLECHA_CI_WINDOWS_LLVM_MAJOR}"
        "workflow Windows LLVM baseline"
    )
    require_text(
        ${workflow}
        "xcode_version: \"${FLECHA_CI_MACOS_XCODE_VERSION}\""
        "workflow Xcode baseline"
    )
    require_text(
        ${workflow}
        "compiler_major: ${FLECHA_CI_WINDOWS_LINKER_MAJOR}"
        "workflow Windows linker baseline"
    )
    require_text(
        ${workflow}
        "compiler_major: ${FLECHA_CI_MACOS_APPLECLANG_MAJOR}"
        "workflow Apple Clang baseline"
    )
    require_text(
        ${workflow}
        "sdk_major: ${FLECHA_CI_MACOS_SDK_MAJOR}"
        "workflow macOS SDK baseline"
    )
    require_text(
        ${workflow}
        "cmake/verify_toolchain.cmake"
        "workflow toolchain verification"
    )
endforeach()

require_text(
    ciWorkflow
    "gcc-${FLECHA_CI_LINUX_GNU_MAJOR} g++-${FLECHA_CI_LINUX_GNU_MAJOR}"
    "CI GCC baseline"
)
foreach(workflow IN ITEMS ciWorkflow releaseWorkflow)
    require_text(
        ${workflow}
        "sudo apt-get install -y binutils clang-"
        "workflow Linux clean-install linker packages"
    )
    require_text(
        ${workflow}
        "libc6-dev"
        "workflow Linux clean-install development package"
    )
endforeach()

require_text(
    toolchainDocumentation
    "LLVM development package | ${FLECHA_LLVM_MIN_MAJOR} through ${FLECHA_LLVM_MAX_MAJOR}"
    "LLVM documentation"
)
require_text(
    toolchainDocumentation
    "Clang C/C++ compiler | ${FLECHA_CLANG_MIN_MAJOR} through ${FLECHA_CLANG_MAX_MAJOR}"
    "Clang documentation"
)
require_text(
    toolchainDocumentation
    "GCC C/C++ compiler | ${FLECHA_GNU_MIN_MAJOR} through ${FLECHA_GNU_MAX_MAJOR}"
    "GCC documentation"
)
require_text(
    toolchainDocumentation
    "Apple Clang | ${FLECHA_APPLECLANG_MIN_MAJOR} through ${FLECHA_APPLECLANG_MAX_MAJOR}"
    "Apple Clang documentation"
)
require_text(
    toolchainDocumentation
    "Clang ${FLECHA_CI_WINDOWS_LLVM_MAJOR}, LLVM ${FLECHA_CI_WINDOWS_LLVM_MAJOR}"
    "Windows release baseline documentation"
)
require_text(
    toolchainDocumentation
    "binutils clang-${FLECHA_CI_LINUX_LLVM_MAJOR} libc6-dev"
    "Linux clean-install package documentation"
)
require_text(
    toolchainDocumentation
    "Xcode ${FLECHA_CI_MACOS_XCODE_VERSION}, Apple Clang ${FLECHA_CI_MACOS_APPLECLANG_MAJOR}, macOS SDK ${FLECHA_CI_MACOS_SDK_MAJOR}"
    "macOS release baseline documentation"
)
require_text(
    readme
    "Clang ${FLECHA_CLANG_MIN_MAJOR}-${FLECHA_CLANG_MAX_MAJOR}, GCC ${FLECHA_GNU_MIN_MAJOR}-${FLECHA_GNU_MAX_MAJOR}, or Apple Clang ${FLECHA_APPLECLANG_MIN_MAJOR}-${FLECHA_APPLECLANG_MAX_MAJOR}"
    "README compiler policy"
)
