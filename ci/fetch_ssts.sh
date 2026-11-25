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

# Fetches SingleStepTests (github.com/SingleStepTests/z80) at one commit into
# <dir>, unless <dir>/v1 is already there (restored from a cache). Only that
# commit is fetched, without history. Used by the GitHub workflow; written
# for the bash of Linux, of MSYS2 and of macOS (3.2).
#
# Usage: ci/fetch_ssts.sh <dir> <commit>
set -euo pipefail

[ $# -eq 2 ] || { echo "usage: ci/fetch_ssts.sh <dir> <commit>" >&2; exit 2; }
dir=$1
commit=$2

if [ -d "$dir/v1" ]; then
    echo "fetch_ssts: $dir/v1 already there"
    exit 0
fi
rm -rf "$dir"
git init -q "$dir"
git -C "$dir" remote add origin https://github.com/SingleStepTests/z80.git
git -C "$dir" fetch -q --depth 1 origin "$commit"
git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
rm -rf "$dir/.git"
count=$(find "$dir/v1" -name '*.json' | wc -l | tr -d ' ')
echo "fetch_ssts: $count data files at $commit"
[ "$count" -eq 1604 ] || { echo "fetch_ssts: 1604 files expected" >&2; exit 1; }
