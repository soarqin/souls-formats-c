# Cross-compile toolchain for WSL2 Ubuntu → x86_64 Windows PE
# Use:
#   cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake
# The produced .exe runs natively on the Windows host via WSL interop
# (binfmt_misc), so ctest can launch tests directly from WSL.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Prefer the POSIX-thread MinGW build; needed for some C++ deps and harmless
# for pure C. apt: gcc-mingw-w64-x86-64-posix / g++-mingw-w64-x86-64-posix.
set(_mingw_prefix x86_64-w64-mingw32)
foreach(_suffix "-posix" "")
    find_program(_cc  ${_mingw_prefix}-gcc${_suffix})
    find_program(_cxx ${_mingw_prefix}-g++${_suffix})
    find_program(_rc  ${_mingw_prefix}-windres)
    if(_cc AND _cxx AND _rc)
        set(CMAKE_C_COMPILER   ${_cc})
        set(CMAKE_CXX_COMPILER ${_cxx})
        set(CMAKE_RC_COMPILER  ${_rc})
        break()
    endif()
endforeach()

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "MinGW-w64 not found. Install via:\n"
        "  sudo apt install mingw-w64 mingw-w64-tools mingw-w64-x86-64-dev "
        "gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix")
endif()

set(CMAKE_FIND_ROOT_PATH /usr/${_mingw_prefix})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# WSL2 binfmt_misc auto-routes .exe to Windows; no wine emulator is needed.
set(CMAKE_CROSSCOMPILING_EMULATOR "")

# Static-link the MinGW C runtime to avoid users needing libgcc_s_seh-1.dll
# / libwinpthread-1.dll / libstdc++-6.dll alongside our binaries.
add_link_options(-static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive
                 -lwinpthread -Wl,--no-whole-archive -Wl,-Bdynamic)
