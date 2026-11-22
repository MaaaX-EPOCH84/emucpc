#!/usr/bin/env bash
# Checks that the core (libraries z80, media, chips, machine, expr) does no
# I/O, reads no clock, starts no thread, throws and catches no exception.
# The language has no declarative way to forbid this; three checks stand in:
#
#   1. links: emucpc_core_library() refuses, at configuration time, any link
#      to a target outside the core (cmake/EmucpcTargets.cmake; its own test
#      is tests/ci/core_link);
#   2. includes: every #include of a core source is either a standard header
#      of the allowed list, a public header of a core library
#      (<emucpc/<core>/...>), or a quoted path that resolves to a file of a
#      core library;
#   3. symbols: the undefined symbols of the core static libraries
#      (nm --undefined-only) name no system call and no forbidden function;
#      this catches what an indirect include let in.
#
# Usage: ci/check_core.sh <source dir> [<build dir>]
#
# Core library <lib> has its sources in <source dir>/libs/<lib> and its
# archive in <build dir>/libs/<lib>/libemucpc_<lib>.a. Without a build dir,
# check 3 is skipped. Check 3 is written for gcc and glibc and runs under
# Linux only (ctest declares it only there).
# Prints what was checked; exits 1 on the first kind of fault found, after
# listing every fault of that kind.
set -euo pipefail

core=(z80 media chips machine expr)

# Standard headers a core source may include. Not allowed in particular:
# <thread> <mutex> <atomic> <chrono> <ctime> <random> <cstdio> <iostream>
# <fstream> <filesystem> <cmath> <cstdlib> <stdexcept> <exception>.
allowed_headers=(
    algorithm array bit bitset cassert cstddef cstdint cstring compare
    concepts functional initializer_list iterator limits map memory numeric
    optional ranges set span string string_view tuple type_traits
    unordered_map utility variant vector
)

# Undefined symbols a core library may not reference (extended regular
# expression, matched against the whole name).
forbidden_symbols='(f?open(64)?|fdopen|freopen|f?read|f?write|f?close|fputs|fgets|puts|putchar|getchar|printf|fprintf|sprintf|snprintf|vprintf|vfprintf|perror|remove|rename|tmpfile|socket|connect|bind|listen|accept|send|recv|select|poll|ioctl|mmap|pthread_[a-z_]+|clock_gettime|gettimeofday|time|clock|localtime(_r)?|gmtime(_r)?|rand|srand|random|getenv|setenv|system|(a?sin|a?cos|a?tan|atan2|sinh|cosh|tanh|exp|exp2|expm1|log|log2|log10|log1p|pow|cbrt|hypot)[fl]?|__cxa_throw|__cxa_begin_catch|__cxa_allocate_exception|__cxa_rethrow|_ZSt4cout|_ZSt4cerr|_ZSt4clog|_ZSt3cin|_ZNSt8ios_base4Init.*|_ZNSt6thread.*|_ZNSt6chrono.*clock3now.*|_ZNSt13random_device.*)'

usage() {
    echo "usage: ci/check_core.sh <source dir> [<build dir>]" >&2
    exit 2
}
[[ $# -ge 1 && $# -le 2 ]] || usage
src=$(cd "$1" && pwd)
build=${2:-}
[[ -z $build ]] || build=$(cd "$build" && pwd)

is_allowed_header() {
    local h
    for h in "${allowed_headers[@]}"; do
        [[ $1 == "$h" ]] && return 0
    done
    return 1
}

is_core() {
    local c
    for c in "${core[@]}"; do
        [[ $1 == "$c" ]] && return 0
    done
    return 1
}

# Check 2: includes.
include_re='^[[:space:]]*#[[:space:]]*include[[:space:]]*([<"])([^>"]+)[>"]'
faults=0
files=0
for lib in "${core[@]}"; do
    dir=$src/libs/$lib
    [[ -d $dir ]] || continue
    while IFS= read -r -d '' file; do
        files=$((files + 1))
        lineno=0
        while IFS= read -r line || [[ -n $line ]]; do
            lineno=$((lineno + 1))
            [[ $line =~ $include_re ]] || continue
            kind=${BASH_REMATCH[1]}
            name=${BASH_REMATCH[2]}
            if [[ $kind == '<' ]]; then
                if is_allowed_header "$name"; then
                    continue
                fi
                if [[ $name =~ ^emucpc/([a-z0-9_]+)/ ]] && is_core "${BASH_REMATCH[1]}"; then
                    continue
                fi
            else
                target=$(realpath -m "$(dirname "$file")/$name")
                if [[ -f $target && $target =~ ^$src/libs/([a-z0-9_]+)/ ]] && is_core "${BASH_REMATCH[1]}"; then
                    continue
                fi
            fi
            echo "check_core: ${file#"$src"/}:$lineno: include not allowed in the core: $kind$name" >&2
            faults=$((faults + 1))
        done <"$file"
    done < <(find "$dir" -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' -o -name '*.ipp' -o -name '*.inc' \) -print0 | sort -z)
done
if ((faults > 0)); then
    echo "check_core: $faults include(s) not allowed" >&2
    exit 1
fi
echo "check_core: includes: $files core source file(s) checked"

# Check 3: symbols.
[[ -n $build ]] || { echo "check_core: symbols: no build dir given, skipped"; exit 0; }
libs=0
for lib in "${core[@]}"; do
    [[ -d $src/libs/$lib ]] || continue
    # Where libs/<lib>/CMakeLists.txt puts it; a missing archive is a fault,
    # so that a moved library cannot make this check pass on nothing.
    archive=$build/libs/$lib/libemucpc_$lib.a
    if [[ ! -f $archive ]]; then
        echo "check_core: libs/$lib has sources but $archive does not exist" >&2
        faults=$((faults + 1))
        continue
    fi
    libs=$((libs + 1))
    symbols=$(nm --undefined-only --format=just-symbols "$archive" | sed 's/@.*//' | sort -u)
    while IFS= read -r symbol; do
        if [[ -n $symbol && $symbol =~ ^${forbidden_symbols}$ ]]; then
            echo "check_core: libs/$lib/libemucpc_$lib.a: forbidden symbol: $symbol" >&2
            faults=$((faults + 1))
        fi
    done <<<"$symbols"
done
if ((faults > 0)); then
    echo "check_core: $faults forbidden symbol(s)" >&2
    exit 1
fi
echo "check_core: symbols: $libs core librar$( ((libs == 1)) && echo y || echo ies) checked"
