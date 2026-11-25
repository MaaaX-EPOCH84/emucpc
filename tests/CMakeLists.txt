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

# Unit tests (doctest), one executable per library; benches are separate
# programs declared to ctest. Labels: fast (every commit), full, local
# (needs data that is not in the repository), hw (needs the real machine).

add_executable(emucpc_test_toolchain toolchain_test.cpp)
target_link_libraries(emucpc_test_toolchain PRIVATE
    emucpc::settings emucpc::version emucpc::doctest emucpc::json)
add_test(NAME toolchain COMMAND emucpc_test_toolchain)
set_tests_properties(toolchain PROPERTIES LABELS fast)

add_executable(emucpc_test_z80 z80_test.cpp)
target_link_libraries(emucpc_test_z80 PRIVATE emucpc::settings emucpc::z80 emucpc::doctest)
add_test(NAME z80 COMMAND emucpc_test_z80)
set_tests_properties(z80 PROPERTIES LABELS fast)

add_test(NAME emucpc_version COMMAND emucpc --version)
set_tests_properties(emucpc_version PROPERTIES
    LABELS fast PASS_REGULAR_EXPRESSION "^EmuCPC [^\r\n]+\n$")

add_test(NAME emucpc_bad_option COMMAND emucpc --no-such-option)
set_tests_properties(emucpc_bad_option PROPERTIES LABELS fast WILL_FAIL TRUE)

# Checks of the CI scripts themselves, under Linux only: ci/check_core.sh on
# the real tree, then each checker against fixtures that it must refuse
# (tests/ci). The symbol check of ci/check_core.sh is written for gcc and
# glibc.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    find_program(EMUCPC_BASH bash REQUIRED)
    set(ci_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_core")
    set(ci_built "${CMAKE_CURRENT_BINARY_DIR}/check_core")

    # Fixture core libraries, laid out as <build dir>/libs/<lib>/libemucpc_<lib>.a.
    function(emucpc_check_core_fixture tree lib)
        set(target check_core_fixture_${tree}_${lib})
        add_library(${target} STATIC ${ARGN})
        set_target_properties(${target} PROPERTIES
            OUTPUT_NAME emucpc_${lib}
            ARCHIVE_OUTPUT_DIRECTORY "${ci_built}/${tree}/libs/${lib}")
    endfunction()
    emucpc_check_core_fixture(good z80 "${ci_fixtures}/good/libs/z80/src/ok.cpp")
    target_include_directories(check_core_fixture_good_z80 PRIVATE
        "${ci_fixtures}/good/libs/z80/include")
    emucpc_check_core_fixture(symbols expr "${ci_fixtures}/symbols/libs/expr/src/fopen.cpp")
    emucpc_check_core_fixture(symbols media "${ci_fixtures}/symbols/libs/media/src/throw.cpp")
    emucpc_check_core_fixture(symbols chips "${ci_fixtures}/symbols/libs/chips/src/time.cpp")

    add_test(NAME check_core
        COMMAND "${EMUCPC_BASH}" "${PROJECT_SOURCE_DIR}/ci/check_core.sh"
            "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
    add_test(NAME check_core_fixtures
        COMMAND "${EMUCPC_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_core_test.sh"
            "${PROJECT_SOURCE_DIR}/ci/check_core.sh" "${ci_fixtures}" "${ci_built}")
    add_test(NAME core_link
        COMMAND "${EMUCPC_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/ci/core_link_test.sh"
            "${CMAKE_COMMAND}" "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/core_link")
    add_test(NAME check_third_party
        COMMAND "${EMUCPC_BASH}" "${PROJECT_SOURCE_DIR}/ci/check_third_party.sh"
            "${PROJECT_SOURCE_DIR}/third_party")
    add_test(NAME check_third_party_faults
        COMMAND "${EMUCPC_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_third_party_test.sh"
            "${PROJECT_SOURCE_DIR}/ci/check_third_party.sh" "${PROJECT_SOURCE_DIR}/third_party")
    add_test(NAME check_headers
        COMMAND "${EMUCPC_BASH}" "${PROJECT_SOURCE_DIR}/ci/check_headers.sh"
            "${PROJECT_SOURCE_DIR}")
    add_test(NAME check_headers_faults
        COMMAND "${EMUCPC_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/ci/check_headers_test.sh"
            "${PROJECT_SOURCE_DIR}/ci/check_headers.sh")
    set_tests_properties(check_core check_core_fixtures core_link
        check_third_party check_third_party_faults check_headers check_headers_faults
        PROPERTIES LABELS fast)
endif()
