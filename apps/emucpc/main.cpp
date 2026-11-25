// ================================================================
//     ______                 __________  ______
//    / ____/___ ___  __  __/ ____/ __ \/ ____/
//   / __/ / __ `__ \/ / / / /   / /_/ / /
//  / /___/ / / / / / /_/ / /___/ ____/ /___
// /_____/_/ /_/ /_/\__,_/\____/_/    \____/
//
// EmuCPC — Amstrad CPC, CPC Plus and GX4000, down to the 1/16 µs
// ================================================================
// Author: Maxime MARCONATO (MaaaX^^, aka EPOCH84)
// ================================================================
// License: GNU General Public License v3.0 or later — see LICENSE.
// SPDX-License-Identifier: GPL-3.0-or-later
// ================================================================

// Entry point of the emulator: command line, version, help.

#include <cstdio>
#include <string_view>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <emucpc/version.hpp>

namespace {

constexpr std::string_view usage =
    "Usage: emucpc [--version] [--help]\n"
    "\n"
    "  --version  print the version and exit\n"
    "  --help     print this help and exit\n";

} // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
    // Same bytes as under Linux: "\n", never "\r\n".
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--version") {
            std::printf("EmuCPC %s\n", emucpc::version);
            return 0;
        }
        if (arg == "--help") {
            std::fwrite(usage.data(), 1, usage.size(), stdout);
            return 0;
        }
        std::fprintf(stderr, "emucpc: unknown option '%s'\n", argv[i]);
        std::fwrite(usage.data(), 1, usage.size(), stderr);
        return 2;
    }
    std::fwrite(usage.data(), 1, usage.size(), stdout);
    return 0;
}
