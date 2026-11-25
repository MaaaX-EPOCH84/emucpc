#!/usr/bin/env bash
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

# Proves that emucpc_core_library() (cmake/EmucpcTargets.cmake) accepts a
# link to a core library and refuses, at configuration time, a link to any
# other target, and a core library of an unknown name.
#
# Usage: tests/ci/core_link_test.sh <cmake> <source dir> <work dir>
set -euo pipefail

cmake=$1
source=$2
work=$3
failures=0

# expect <name> <status: 0 or fail> <pattern> <library name> <link>
expect() {
    local name=$1 want=$2 pattern=$3 status=0 output
    rm -rf "$work/$name"
    output=$("$cmake" -S "$source/tests/ci/core_link" -B "$work/$name" \
        -DEMUCPC_SOURCE="$source" -DNAME="$4" -DLINK="$5" 2>&1) || status=$?
    if [[ $want == 0 && $status != 0 || $want == fail && $status == 0 ]]; then
        echo "FAIL $name: status $status, expected $want"
        echo "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    elif ! grep -qF -- "$pattern" <<<"$output"; then
        echo "FAIL $name: no '$pattern' in the output"
        echo "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    else
        echo "ok   $name"
    fi
}

expect core-to-core 0 "Generating done" chips emucpc::z80
expect core-to-threads fail "link to 'Threads::Threads' refused" chips Threads::Threads
expect core-to-session fail "link to 'emucpc::session' refused" chips emucpc::session
expect unknown-core fail "'session' is not a core library" session emucpc::z80

((failures == 0)) || { echo "$failures case(s) failed"; exit 1; }
