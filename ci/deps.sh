#!/usr/bin/env bash
# Fetches SDL3 for MinGW (Windows build) into the cache, once, and prints the
# directory given to CMAKE_PREFIX_PATH (see the windows-mingw preset).
#
# Usage: ci/deps.sh
#
# The archive comes from the SDL project's releases and is checked against
# the SHA-256 recorded below, which is also the digest GitHub lists for the
# release asset (the SDL project signs only its source archives).
# Cache: $EMUCPC_DEPS, default ~/.cache/emucpc-deps; the windows-mingw
# preset expects the default.
set -euo pipefail

version=3.4.20
archive=SDL3-devel-$version-mingw.tar.gz
url=https://github.com/libsdl-org/SDL/releases/download/release-$version/$archive
sha=d2d3bf3fb97a90b22e908a306d7362631ef1bab229d988fe6669595dc29cd9ed

deps=${EMUCPC_DEPS:-$HOME/.cache/emucpc-deps}
prefix=$deps/SDL3-$version-mingw/SDL3-$version/x86_64-w64-mingw32

if [[ ! -d $prefix ]]; then
    mkdir -p "$deps/download"
    if [[ ! -f $deps/download/$archive ]]; then
        echo "ci/deps.sh: downloading $url" >&2
        curl -sSfL -o "$deps/download/$archive.part" "$url"
        mv "$deps/download/$archive.part" "$deps/download/$archive"
    fi
    [[ $(sha256sum "$deps/download/$archive" | cut -d' ' -f1) == "$sha" ]] \
        || { echo "ci/deps.sh: $archive does not match its recorded SHA-256" >&2; exit 1; }
    # Extracted into its own directory, as downloaded files are not trusted;
    # into a temporary one first, so that an interrupted run leaves nothing.
    tmp=$(mktemp -d "$deps/SDL3-$version-mingw.XXXXXX")
    trap 'rm -rf "$tmp"' EXIT
    tar xzf "$deps/download/$archive" -C "$tmp"
    rm -rf "$deps/SDL3-$version-mingw"
    mv "$tmp" "$deps/SDL3-$version-mingw"
    trap - EXIT
fi
echo "$prefix"
