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

cmake_minimum_required(VERSION 3.28)

project(EmuCPC
    DESCRIPTION "Amstrad CPC, CPC Plus and GX4000 emulator"
    HOMEPAGE_URL "https://github.com/MaaaX-EPOCH84/emucpc"
    LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(EMUCPC_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(EMUCPC_TEST_WINDOW "Add the tests that open a window (need a display)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EmucpcWarnings)
include(EmucpcTargets)

# SDL3 is not used by any target yet; it is looked up so that the result of
# ci/deps.sh is checked from the first configuration on.
find_package(SDL3 3.4 CONFIG QUIET)
if(SDL3_FOUND)
    message(STATUS "SDL3 ${SDL3_VERSION}: ${SDL3_DIR}")
else()
    message(STATUS "SDL3 not found (needed from the interface milestone on)")
endif()

# Version from `git describe`, refreshed at each build, rewritten only when
# it changes.
set(EMUCPC_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(emucpc_version_header
    COMMAND "${CMAKE_COMMAND}"
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${EMUCPC_GENERATED_DIR}/emucpc/version.hpp
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake"
    BYPRODUCTS "${EMUCPC_GENERATED_DIR}/emucpc/version.hpp"
    COMMENT "Version from git describe"
    VERBATIM)
add_library(emucpc_version INTERFACE)
add_library(emucpc::version ALIAS emucpc_version)
target_include_directories(emucpc_version INTERFACE "${EMUCPC_GENERATED_DIR}")
add_dependencies(emucpc_version emucpc_version_header)

# Before any subdirectory: add_test() is ignored where testing is not yet
# enabled (the benches of devtools/ declare their own tests).
enable_testing()

add_subdirectory(third_party)
add_subdirectory(libs/z80)
add_subdirectory(apps/emucpc)
add_subdirectory(devtools/emucpc-ssts)
add_subdirectory(devtools/emucpc-z80test)
add_subdirectory(tests)
