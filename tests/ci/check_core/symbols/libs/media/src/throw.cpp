// Fixture of ci/check_core.sh: an exception thrown by the core.
// SPDX-License-Identifier: GPL-3.0-or-later
void fixture_throw(int value)
{
    if (value < 0) {
        throw value;
    }
}
