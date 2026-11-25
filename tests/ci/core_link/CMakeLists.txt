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

# Fixture project of tests/ci/core_link_test.sh: declares core libraries
# through cmake/EmucpcTargets.cmake, with the link given in LINK, and the
# library named in NAME. Configured only, never built.
cmake_minimum_required(VERSION 3.28)
project(core_link_fixture LANGUAGES CXX)

include("${EMUCPC_SOURCE}/cmake/EmucpcTargets.cmake")

# Stand-ins for what the real project defines.
add_library(emucpc_settings INTERFACE)
add_library(emucpc::settings ALIAS emucpc_settings)
add_library(emucpc_session STATIC empty.cpp)
add_library(emucpc::session ALIAS emucpc_session)

emucpc_core_library(z80 empty.cpp)
emucpc_core_library(${NAME} empty.cpp LINK ${LINK})
