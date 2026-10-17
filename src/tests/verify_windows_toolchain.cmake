foreach(variable IN ITEMS SOURCE_ROOT OUTPUT_ROOT)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_ROOT}")
set(fakeBin "${OUTPUT_ROOT}/ucrt64/bin")
set(fakeLib "${OUTPUT_ROOT}/ucrt64/lib")
set(fakeUsrBin "${OUTPUT_ROOT}/usr/bin")
file(MAKE_DIRECTORY "${fakeBin}" "${fakeLib}" "${fakeUsrBin}")
file(WRITE "${fakeLib}/crt2.o" "fake UCRT startup object")

file(WRITE "${fakeBin}/clang++.exe" [=[#!/bin/sh
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$1" in
  --version) printf '%s\n' 'clang version 23.1.3' ;;
  -dumpversion) printf '%s\n' '23.1.3' ;;
  -dumpmachine) printf '%s\n' 'x86_64-w64-windows-gnu' ;;
  -print-prog-name=ld) printf '%s\n' "$script_dir/ld" ;;
  -print-file-name=crt2.o) printf '%s\n' "$script_dir/../lib/crt2.o" ;;
  *) exit 2 ;;
esac
]=])
file(WRITE "${fakeBin}/ld" [=[#!/bin/sh
printf '%s\n' 'GNU ld (GNU Binutils) 2.47'
]=])
file(WRITE "${fakeUsrBin}/pacman.exe" [=[#!/bin/sh
if [ "$1" != "-Q" ] || [ "$2" != "mingw-w64-ucrt-x86_64-crt" ]; then
  exit 2
fi
printf '%s\n' 'mingw-w64-ucrt-x86_64-crt 13.0.0-1'
]=])

foreach(executable IN ITEMS
        "${fakeBin}/clang++.exe"
        "${fakeBin}/ld"
        "${fakeUsrBin}/pacman.exe")
    file(CHMOD "${executable}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DPOLICY_ROOT=${SOURCE_ROOT}/cmake
        -DCXX_COMPILER=${fakeBin}/clang++.exe
        -DEXPECTED_CXX_FAMILY=Clang
        -DEXPECTED_CXX_MAJOR=23
        -DPLATFORM=Windows
        -P ${SOURCE_ROOT}/cmake/verify_toolchain.cmake
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Windows toolchain verification failed: ${output}${error}"
    )
endif()

set(text "${output}${error}")
foreach(expected IN ITEMS
        "mingw-w64-ucrt-x86_64-crt 13.0.0-1"
        "ucrt64/bin/../lib/crt2.o")
    string(FIND "${text}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "Windows toolchain verification did not report '${expected}': "
            "${text}"
        )
    endif()
endforeach()
