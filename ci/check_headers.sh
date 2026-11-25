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

# Checks that every file we write starts with the project header, the same
# everywhere, after a "#!" line if there is one, followed by an empty line.
# The header below is its only definition.
#
# Files: C and C++ (*.c *.cpp *.h *.hpp, comment "//"), CMake (CMakeLists.txt,
# *.cmake), shell (*.sh), Python (*.py), YAML (*.yml *.yaml) (comment "#");
# never third_party/. In a Git work tree, the tracked and the untracked but
# not ignored files; elsewhere (the fixtures of tests/ci), every file found.
#
# Usage: ci/check_headers.sh [--fix] <source dir>
#   --fix  inserts the header in the files that have none (a file whose
#          header is present but altered is still reported, not rewritten)
#
# Prints what was checked; exits 1 if a file lacks the header or has it
# altered, 2 on a usage error.
set -euo pipefail

header=(
    '================================================================'
    '    ______                 __________  ______'
    '   / ____/___ ___  __  __/ ____/ __ \/ ____/'
    '  / __/ / __ `__ \/ / / / /   / /_/ / /'
    ' / /___/ / / / / / /_/ / /___/ ____/ /___'
    '/_____/_/ /_/ /_/\__,_/\____/_/    \____/'
    ''
    'EmuCPC — Amstrad CPC, CPC Plus and GX4000, down to the 1/16 µs'
    '================================================================'
    'Author: Maxime MARCONATO (MaaaX^^, aka EPOCH84)'
    '================================================================'
    'License: GNU General Public License v3.0 or later — see LICENSE.'
    'SPDX-License-Identifier: GPL-3.0-or-later'
    '================================================================'
)

usage() {
    echo "usage: ci/check_headers.sh [--fix] <source dir>" >&2
    exit 2
}

fix=0
if [[ ${1:-} == --fix ]]; then
    fix=1
    shift
fi
[[ $# -eq 1 ]] || usage
root=$(cd "$1" && pwd)

# render <comment>: the header with that comment marker, one line each.
render() {
    local line
    for line in "${header[@]}"; do
        if [[ -z $line ]]; then
            printf '%s\n' "$1"
        else
            printf '%s %s\n' "$1" "$line"
        fi
    done
}

comment_of() {
    case $1 in
    *.c | *.cpp | *.h | *.hpp) echo '//' ;;
    *) echo '#' ;;
    esac
}

list_files() {
    if git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git -C "$root" ls-files -z --cached --others --exclude-standard
    else
        (cd "$root" && find . -type f -print0 | sed -z 's|^\./||')
    fi
}

is_ours() {
    [[ $1 != third_party/* ]] || return 1
    case $1 in
    *.c | *.cpp | *.h | *.hpp | *.cmake | *.sh | *.py | *.yml | *.yaml | CMakeLists.txt | */CMakeLists.txt) return 0 ;;
    *) return 1 ;;
    esac
}

checked=0
faults=0
fixed=0
while IFS= read -r -d '' file; do
    is_ours "$file" || continue
    path=$root/$file
    [[ -f $path ]] || continue  # deleted but still in the index
    checked=$((checked + 1))
    expected=$(render "$(comment_of "$file")")
    skip=0
    if [[ $(sed -n '1{s/^\(..\).*/\1/p;q}' "$path") == '#!' ]]; then
        skip=1
    fi
    count=${#header[@]}
    # sed, not head in a pipe: under pipefail, SIGPIPE would stop the script.
    actual=$(sed -n "$((skip + 1)),$((skip + count))p" "$path")
    after=$(sed -n "$((skip + count + 1))p" "$path")
    if [[ $actual == "$expected" && -z $after ]]; then
        continue
    fi
    first_marker="$(comment_of "$file") ${header[0]}"
    has_header=0
    if [[ $(sed -n "$((skip + 1))p" "$path") == "$first_marker" ]]; then
        has_header=1
    fi
    if ((fix == 1 && has_header == 0)); then
        tmp=$(mktemp)
        {
            ((skip == 0)) || sed -n "1,${skip}p" "$path"
            printf '%s\n\n' "$expected"
            sed -n "$((skip + 1)),\$p" "$path"
        } >"$tmp"
        # A new file moved in place, not a rewrite: bash reads a running
        # script as it goes, and this script may be fixing itself.
        chmod --reference="$path" "$tmp"
        mv "$tmp" "$path"
        fixed=$((fixed + 1))
        echo "check_headers: $file: header inserted"
        continue
    fi
    if ((has_header == 1)); then
        echo "check_headers: $file: header altered" >&2
    else
        echo "check_headers: $file: no header" >&2
    fi
    faults=$((faults + 1))
done < <(list_files)

if ((faults > 0)); then
    echo "check_headers: $faults file(s) without the project header" >&2
    exit 1
fi
if ((fixed > 0)); then
    echo "check_headers: $checked file(s) checked, header inserted in $fixed"
else
    echo "check_headers: $checked file(s) checked"
fi
