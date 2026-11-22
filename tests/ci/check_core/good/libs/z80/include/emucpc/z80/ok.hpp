// Fixture of ci/check_core.sh: a core header with allowed includes only.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fixture {
std::vector<std::uint8_t> repeat(std::uint8_t value, std::size_t count);
}
