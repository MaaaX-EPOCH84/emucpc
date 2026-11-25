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

// Fixture of ci/check_core.sh: allowed includes, angled and quoted.
#include <emucpc/z80/ok.hpp>

#include "../include/emucpc/z80/ok.hpp"

#include <algorithm>

namespace fixture {
std::vector<std::uint8_t> repeat(std::uint8_t value, std::size_t count)
{
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < count; ++i) {
        bytes.push_back(value);
    }
    std::reverse(bytes.begin(), bytes.end());
    return bytes;
}
} // namespace fixture
