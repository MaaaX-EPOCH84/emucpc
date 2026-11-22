#!/usr/bin/env bash
# Proves that ci/check_third_party.sh accepts the real copies and refuses
# each kind of fault, planted in a scratch copy of third_party/.
#
# Usage: tests/ci/check_third_party_test.sh <check_third_party.sh> <third_party dir>
set -euo pipefail

check=$1
real=$2
failures=0
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# expect <name> <status> <pattern> <sabotage command...>: the sabotage runs
# in a fresh copy (current directory), then the check must give the status
# and print the pattern.
expect() {
    local name=$1 want=$2 pattern=$3 status=0 output
    shift 3
    rm -rf "$work/tp"
    cp -R "$real" "$work/tp"
    (cd "$work/tp" && "$@")
    output=$("$check" "$work/tp" 2>&1) || status=$?
    if [[ $status != "$want" ]]; then
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

expect real 0 "files checked" true
expect altered-byte 1 "doctest/doctest/doctest.h does not match its recorded SHA-256" \
    sh -c 'printf " " >> doctest/doctest/doctest.h'
expect unlisted-file 1 "doctest/extra.h is not listed" \
    sh -c 'echo "// extra" > doctest/extra.h'
expect missing-licence 1 "licence file doctest/LICENSE.txt does not exist" \
    rm doctest/LICENSE.txt
expect missing-file 1 "nlohmann_json/nlohmann/json.hpp is listed but does not exist" \
    rm nlohmann_json/nlohmann/json.hpp
expect directory-without-section 1 "directory newlib/ has no section" \
    sh -c 'mkdir newlib && echo x > newlib/a.h'
expect licence-not-allowed 1 "licence 'GPL-2.0-only' is not in the allowed list" \
    sed -i 's/^| Licence | MIT (`doctest/| Licence | GPL-2.0-only (`doctest/' VERSIONS.md
expect section-without-directory 1 "directory nlohmann_json/ does not exist" \
    sh -c 'rm -r nlohmann_json'

((failures == 0)) || { echo "$failures case(s) failed"; exit 1; }
