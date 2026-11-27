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

# emucpc-z80test: the Z80 against z80test 1.2a of Patrik Rak, on a bus that
# stands in for a 48K Spectrum. Programs and expected screens are in
# third_party/z80test/ (taken from the official release by
# extract_release.py, sums in third_party/VERSIONS.md).
add_executable(emucpc-z80test main.cpp)
target_link_libraries(emucpc-z80test PRIVATE emucpc::settings emucpc::z80)

# One test per program and variant (z80ccf assumes a Zilog: Zilog only),
# and z80ccfscr against the reference image of each variant.
# fast: z80full on the Zilog (all flags and registers, ≈ 2 s; also slow:
# ≈ 145 s under the sanitizers, which ci-san leaves out) and the three
# screens (milliseconds); full: the whole matrix.
set(z80test_data "${PROJECT_SOURCE_DIR}/third_party/z80test")
foreach(program IN ITEMS z80full z80flags z80doc z80docflags z80ccf z80memptr)
    foreach(variant IN ITEMS zilog nec st)
        if(program STREQUAL "z80ccf" AND NOT variant STREQUAL "zilog")
            continue()
        endif()
        set(name "z80test_${program}_${variant}")
        add_test(NAME ${name}
            COMMAND emucpc-z80test --program "${z80test_data}/${program}.bin" --variant ${variant})
        if(program STREQUAL "z80full" AND variant STREQUAL "zilog")
            set_tests_properties(${name} PROPERTIES LABELS "fast;full;slow")
        else()
            set_tests_properties(${name} PROPERTIES LABELS "full")
        endif()
    endforeach()
endforeach()
foreach(variant IN ITEMS zilog nec st)
    add_test(NAME z80test_z80ccfscr_${variant}
        COMMAND emucpc-z80test --program "${z80test_data}/z80ccfscr.bin" --variant ${variant}
                --screen "${z80test_data}/z80ccfscr-${variant}.scr")
    set_tests_properties(z80test_z80ccfscr_${variant} PROPERTIES LABELS "fast;full")
endforeach()
