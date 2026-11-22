#!/usr/bin/env bash
# Local continuous integration. In order: third-party copies, Linux build
# and tests (ci/check_core.sh and the checks of the CI scripts among them),
# clang-tidy, ASan + UBSan build and fast tests, Windows build (MinGW) and
# tests under Wine, same bytes under Linux and Windows.
#
# Usage: ci/run.sh
#
# Builds through the ci-* presets in build-ci/, never in build/ (where the
# emulator is used by hand). Logs go to build-ci/logs. Prints one line per
# step and exits with the status of the first failing step. Called by the
# Git pre-commit hook (ci/install-hook.sh); tests the working tree.
#
# Requires: cmake, gcc, x86_64-w64-mingw32-g++-posix, wine, clang-tidy and
# run-clang-tidy, nm, curl (first run, ci/deps.sh).
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
ci=$root/build-ci
logs=$ci/logs
jobs=$(nproc 2>/dev/null || echo 4)
mkdir -p "$logs"
cd "$root"
start=$SECONDS

# step <name> <command...>: runs it, its output into logs/<name>.log.
step() {
    local name=$1 t=$SECONDS code
    shift
    if "$@" >"$logs/$name.log" 2>&1; then
        printf 'ci: %-18s ok (%ds)\n' "$name" $((SECONDS - t))
    else
        code=$?
        printf 'ci: %-18s FAILED, status %d, see %s\n' "$name" "$code" "$logs/$name.log"
        tail -n 20 "$logs/$name.log" | sed 's/^/    /'
        exit "$code"
    fi
}

# clang-tidy on our code only, from the compile commands of the Linux build.
tidy() {
    run-clang-tidy -quiet -j "$jobs" -p "$ci/linux" "^$root/(apps|libs|devtools)/"
}

step third-party ci/check_third_party.sh
step deps ci/deps.sh
step linux-configure cmake --preset ci-linux
step linux-build cmake --build --preset ci-linux -j "$jobs"
step linux-test ctest --preset ci-linux -j "$jobs"
step clang-tidy tidy
step san-configure cmake --preset ci-san
step san-build cmake --build --preset ci-san -j "$jobs"
step san-test ctest --preset ci-san -j "$jobs"
step windows-configure cmake --preset ci-windows
step windows-build cmake --build --preset ci-windows -j "$jobs"
step windows-test ctest --preset ci-windows -j "$jobs"
step same-bytes ci/same_bytes.sh "$ci/linux" "$ci/windows" "$ci/same-bytes"

echo "ci: all steps passed in $((SECONDS - start)) s"
