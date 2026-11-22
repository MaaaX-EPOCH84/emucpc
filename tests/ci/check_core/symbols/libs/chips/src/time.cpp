// Fixture of ci/check_core.sh: no include, yet a clock read.
// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" long time(long* out);

long fixture_now()
{
    return time(nullptr);
}
