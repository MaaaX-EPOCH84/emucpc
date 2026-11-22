#!/usr/bin/env bash
# Proves that ci/check_core.sh refuses what it must refuse and accepts what
# it must accept, on the fixture trees of tests/ci/check_core (see its
# README.md) and the fixture libraries built by tests/CMakeLists.txt.
#
# Usage: tests/ci/check_core_test.sh <check_core.sh> <fixtures dir> <fixture build dir>
set -euo pipefail

check=$1
fixtures=$2
built=$3
failures=0

# expect <name> <status> <pattern>... -- <arguments of check_core.sh>:
# the status must match and every pattern must appear in the output.
expect() {
    local name=$1 want=$2 status=0 output pattern
    shift 2
    local patterns=()
    while [[ $1 != -- ]]; do
        patterns+=("$1")
        shift
    done
    shift
    output=$("$check" "$@" 2>&1) || status=$?
    if [[ $status != "$want" ]]; then
        echo "FAIL $name: status $status, expected $want"
        echo "$output" | sed 's/^/    /'
        failures=$((failures + 1))
        return
    fi
    for pattern in "${patterns[@]}"; do
        if ! grep -qF -- "$pattern" <<<"$output"; then
            echo "FAIL $name: no '$pattern' in the output"
            echo "$output" | sed 's/^/    /'
            failures=$((failures + 1))
            return
        fi
    done
    echo "ok   $name"
}

empty=$(mktemp -d)
trap 'rm -rf "$empty"' EXIT

expect good 0 "2 core source file(s) checked" "1 core library checked" \
    -- "$fixtures/good" "$built/good"
expect good-without-build 0 "symbols: no build dir given, skipped" \
    -- "$fixtures/good"
expect missing-archive 1 "libs/z80 has sources but" \
    -- "$fixtures/good" "$empty"
expect bad-include 1 "include not allowed in the core: <thread" \
    "include not allowed in the core: <cstdio" "2 include(s) not allowed" \
    -- "$fixtures/bad_include"
expect bad-quoted 1 'include not allowed in the core: "../../session/include/s.hpp' \
    -- "$fixtures/bad_quoted"
expect symbols 1 "3 core source file(s) checked" "forbidden symbol: fopen" \
    "forbidden symbol: __cxa_throw" "forbidden symbol: time" "4 forbidden symbol(s)" \
    -- "$fixtures/symbols" "$built/symbols"

((failures == 0)) || { echo "$failures case(s) failed"; exit 1; }
