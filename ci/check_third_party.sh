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

# Checks the third-party copies against third_party/VERSIONS.md:
#   - every directory of third_party/ has its section, and every section its
#     directory;
#   - the licence of each section is in the allowed list (SPDX identifiers
#     compatible with GPL-3.0-or-later) and its licence file exists;
#   - every file of a copy is listed with its SHA-256, the sum matches, and
#     every listed file exists.
#
# Usage: ci/check_third_party.sh [<third_party dir>]
#
# Lists every fault, then exits 1 if there was one.
set -euo pipefail

allowed_licences=(MIT Zlib BSL-1.0 BSD-2-Clause BSD-3-Clause ISC Apache-2.0)

dir=$(cd "${1:-$(dirname "$0")/../third_party}" && pwd)
versions=$dir/VERSIONS.md
[[ -f $versions ]] || { echo "check_third_party: $versions does not exist" >&2; exit 1; }

faults=0
fault() {
    echo "check_third_party: $*" >&2
    faults=$((faults + 1))
}

# One record per line, from the tables of VERSIONS.md:
#   section|<title>|<directory>|<licence>|<licence file>
#   file|<directory of the section>|<path>|<sha-256>
records=$(awk '
    function flush() {
        if (title != "") print "section|" title "|" directory "|" licence "|" licence_file
    }
    function cell(line, n,    parts) {
        split(line, parts, "|")
        gsub(/^[ \t]+|[ \t]+$/, "", parts[n + 1])
        return parts[n + 1]
    }
    function unquote(text) { gsub(/`/, "", text); return text }
    /^## / { flush(); title = substr($0, 4); directory = licence = licence_file = ""; next }
    title == "" { next }
    /^\| Directory \|/ { directory = unquote(cell($0, 2)); sub(/\/$/, "", directory); next }
    /^\| Licence \|/ {
        value = cell($0, 2)
        licence = value; sub(/ .*/, "", licence)
        licence_file = value
        if (match(licence_file, /`[^`]+`/)) licence_file = substr(licence_file, RSTART + 1, RLENGTH - 2)
        else licence_file = ""
        next
    }
    /^\| `[^`]+` \|/ {
        sum = unquote(cell($0, 3))
        if (sum ~ /^[0-9a-f]+$/ && length(sum) == 64) print "file|" directory "|" unquote(cell($0, 1)) "|" sum
        else print "badsum|" directory "|" unquote(cell($0, 1)) "|" sum
    }
    END { flush() }
' "$versions")

declare -A section_of listed
sections=0
files=0
while IFS='|' read -r kind a b c d; do
    case $kind in
        section)
            sections=$((sections + 1))
            if [[ -z $b ]]; then
                fault "section '$a' has no Directory line"
                continue
            fi
            section_of[$b]=$a
            [[ -d $dir/$b ]] || fault "section '$a': directory $b/ does not exist"
            allowed=0
            for l in "${allowed_licences[@]}"; do
                [[ $c == "$l" ]] && allowed=1
            done
            ((allowed)) || fault "section '$a': licence '$c' is not in the allowed list"
            if [[ -z $d ]]; then
                fault "section '$a': no licence file named"
            elif [[ ! -f $dir/$d ]]; then
                fault "section '$a': licence file $d does not exist"
            fi
            ;;
        file)
            files=$((files + 1))
            listed[$b]=1
            if [[ $b != "$a"/* ]]; then
                fault "$b is listed in the section of $a/ but lies outside it"
            elif [[ ! -f $dir/$b ]]; then
                fault "$b is listed but does not exist"
            elif [[ $(sha256sum "$dir/$b" | cut -d' ' -f1) != "$c" ]]; then
                fault "$b does not match its recorded SHA-256"
            fi
            ;;
        badsum)
            fault "$b: '$c' is not a SHA-256"
            ;;
    esac
done <<<"$records"

# Every directory has its section, every file of a copy is listed.
while IFS= read -r -d '' sub; do
    name=${sub#"$dir"/}
    [[ -n ${section_of[$name]+x} ]] || fault "directory $name/ has no section in VERSIONS.md"
    while IFS= read -r -d '' file; do
        rel=${file#"$dir"/}
        [[ -n ${listed[$rel]+x} ]] || fault "$rel is not listed in VERSIONS.md"
    done < <(find "$sub" -type f -print0)
done < <(find "$dir" -mindepth 1 -maxdepth 1 -type d -print0)

if ((faults > 0)); then
    echo "check_third_party: $faults fault(s)" >&2
    exit 1
fi
echo "check_third_party: $sections copies, $files files checked"
