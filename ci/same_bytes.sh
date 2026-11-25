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

# Runs the same cases with the Linux build and with the Windows build (under
# Wine), each into its own directory of outputs, then requires the two
# directories to hold the same files with the same bytes.
#
# Usage: ci/same_bytes.sh <Linux build dir> <Windows build dir> <output dir>
#
# Outputs go to <output dir>/linux and <output dir>/windows, emptied first.
# Only --help for now; the batch mode adds its scripts here (milestone E5).
set -euo pipefail

linux_build=$1
windows_build=$2
out=$3
export WINEDEBUG=-all

# One case per line: <output file> <arguments of emucpc>.
cases=(
    "help.txt --help"
)

run_cases() {
    local dir=$1 entry file
    shift
    rm -rf "$dir"
    mkdir -p "$dir"
    for entry in "${cases[@]}"; do
        read -r file args <<<"$entry"
        # shellcheck disable=SC2086 # the arguments are split on purpose
        "$@" $args >"$dir/$file"
    done
}

run_cases "$out/linux" "$linux_build/apps/emucpc/emucpc"
run_cases "$out/windows" wine "$windows_build/apps/emucpc/emucpc.exe"

count=0
faults=0
while IFS= read -r -d '' file; do
    rel=${file#"$out/linux/"}
    count=$((count + 1))
    if [[ ! -f $out/windows/$rel ]]; then
        echo "same_bytes: $rel: no Windows output" >&2
        faults=$((faults + 1))
    elif ! cmp -s "$file" "$out/windows/$rel"; then
        echo "same_bytes: $rel differs: $(cmp "$file" "$out/windows/$rel" 2>&1 || true)" >&2
        faults=$((faults + 1))
    fi
done < <(find "$out/linux" -type f -print0)
extra=$(cd "$out/windows" && find . -type f | sed 's|^\./||' | while IFS= read -r f; do
    [[ -f $out/linux/$f ]] || echo "$f"; done)
if [[ -n $extra ]]; then
    echo "same_bytes: Windows outputs without a Linux one: $extra" >&2
    faults=$((faults + 1))
fi
((count > 0)) || { echo "same_bytes: nothing was compared" >&2; exit 1; }
((faults == 0)) || exit 1
echo "same_bytes: $count file(s), same bytes under Linux and Windows"
