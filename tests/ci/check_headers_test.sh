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

# Proves that ci/check_headers.sh refuses a missing or altered header,
# accepts the right one (with or without a "#!" line), ignores third_party/,
# and that --fix inserts the header where there is none, leaves an altered
# one to be reported, and keeps the mode of the file.
#
# Usage: tests/ci/check_headers_test.sh <check_headers.sh>
set -euo pipefail

check=$1
failures=0
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The header as the checker itself carries it (lines 2-15, after its "#!"),
# checked on the real tree by the test check_headers.
sed -n '2,15p' "$check" >"$work/hash_header"
sed 's|^#|//|' "$work/hash_header" >"$work/slash_header"

good=$work/good
mkdir -p "$good/src" "$good/third_party"
{ echo '#!/usr/bin/env bash'; cat "$work/hash_header"; echo; echo 'echo ok'; } >"$good/run.sh"
{ cat "$work/slash_header"; echo; echo 'int f();'; } >"$good/src/f.cpp"
{ cat "$work/hash_header"; echo; echo 'project(x)'; } >"$good/CMakeLists.txt"
echo 'no header in third_party' >"$good/third_party/x.cpp"
chmod +x "$good/run.sh"

# expect <name> <status> <pattern> <checker arguments...>
expect() {
    local name=$1 want=$2 pattern=$3 status=0 output
    shift 3
    output=$("$check" "$@" 2>&1) || status=$?
    if [[ $status != "$want" ]] || ! grep -qF -- "$pattern" <<<"$output"; then
        echo "FAIL $name: status $status (expected $want), output:"
        echo "$output" | sed 's/^/    /'
        failures=$((failures + 1))
        return
    fi
    echo "ok   $name"
}

expect good 0 "3 file(s) checked" "$good"

cp -r "$good" "$work/missing"
echo 'int g();' >"$work/missing/src/g.hpp"
expect missing 1 "src/g.hpp: no header" "$work/missing"

cp -r "$good" "$work/altered"
sed -i 's|MaaaX^^|MaaaX|' "$work/altered/src/f.cpp"
expect altered 1 "src/f.cpp: header altered" "$work/altered"

cp -r "$good" "$work/no_blank"
{ cat "$work/slash_header"; echo 'int f();'; } >"$work/no_blank/src/f.cpp"
expect no-blank-line 1 "src/f.cpp: header altered" "$work/no_blank"

cp -r "$work/missing" "$work/fix"
cp "$work/altered/src/f.cpp" "$work/fix/src/f.cpp"
expect fix-keeps-altered 1 "src/f.cpp: header altered" --fix "$work/fix"
cp "$good/src/f.cpp" "$work/fix/src/f.cpp"
expect fix-inserted 0 "4 file(s) checked" "$work/fix"
if [[ ! -x $work/fix/run.sh ]]; then
    echo "FAIL fix-mode: run.sh lost its mode"
    failures=$((failures + 1))
fi

((failures == 0)) || { echo "$failures case(s) failed"; exit 1; }
