# Toolchain file: Windows 64-bit executables built on Linux with MinGW-w64,
# used by the windows-mingw preset.
#
# The -posix thread model of the compilers is selected, so that std::thread
# and std::mutex rest on winpthreads as they rest on pthreads under Linux;
# the default -win32 model maps them onto native Windows primitives. Its
# runtime is linked statically (cmake/EmucpcWarnings.cmake).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Libraries and headers come from the target side only, programs from the host.
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Tests run under Wine.
find_program(EMUCPC_WINE wine)
if(EMUCPC_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${EMUCPC_WINE}")
endif()
