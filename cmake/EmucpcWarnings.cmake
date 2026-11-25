# ================================================================
#     ______                 __________  ______
#    / ____/___ ___  __  __/ ____/ __ \/ ____/
#   / __/ / __ `__ \/ / / / /   / /_/ / /
#  / /___/ / / / / / /_/ / /___/ ____/ /___
# /_____/_/ /_/ /_/\__,_/\____/_/    \____/
#
# EmuCPC — Amstrad CPC, CPC Plus and GX4000, down to the 1/16 µs
# ================================================================
# Author: Maxime MARCONATO (MaaaX^^, aka EPOCH84)
# ================================================================
# License: GNU General Public License v3.0 or later — see LICENSE.
# SPDX-License-Identifier: GPL-3.0-or-later
# ================================================================

# Compiler settings shared by every target of the project, applied through
# emucpc_project_settings(<target>). Third-party code is included as system
# headers (third_party/CMakeLists.txt), so these warnings never fire on it.

add_library(emucpc_settings INTERFACE)
add_library(emucpc::settings ALIAS emucpc_settings)

target_compile_options(emucpc_settings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Werror)

# Bounds-checked operator[] of the standard containers in Debug and under
# sanitizers, never in release builds.
target_compile_definitions(emucpc_settings INTERFACE
    $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
    $<$<BOOL:${EMUCPC_SANITIZE}>:_GLIBCXX_ASSERTIONS>)

if(EMUCPC_SANITIZE)
    if(WIN32)
        message(FATAL_ERROR "EMUCPC_SANITIZE is not available with MinGW")
    endif()
    set(sanitize_flags -fsanitize=address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer)
    target_compile_options(emucpc_settings INTERFACE ${sanitize_flags})
    target_link_options(emucpc_settings INTERFACE ${sanitize_flags})
endif()

# MinGW: the -posix thread model is selected by the toolchain file; its
# runtime (libwinpthread) and the C++ runtime are linked statically, so that
# a Windows executable needs no MinGW DLL beside it.
if(MINGW)
    target_link_options(emucpc_settings INTERFACE -static)
endif()
