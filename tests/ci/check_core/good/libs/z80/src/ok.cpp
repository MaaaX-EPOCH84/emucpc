// Fixture of ci/check_core.sh: allowed includes, angled and quoted.
// SPDX-License-Identifier: GPL-3.0-or-later
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
